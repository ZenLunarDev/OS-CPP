#!/usr/bin/env python3
"""QEMU sendkey smoke test for MeowOS: boots the kernel, types commands via QMP sendkey, checks serial output."""
import json
import os
import socket
import subprocess
import sys
import time

QMP_HOST = "127.0.0.1"
QMP_PORT = 4444
SERIAL_LOG = os.path.abspath("meow_serial.log")
KERNEL = os.path.abspath("build/kernel.bin")

KEYMAP = {
    'a': 'a', 'b': 'b', 'c': 'c', 'd': 'd', 'e': 'e', 'f': 'f', 'g': 'g',
    'h': 'h', 'i': 'i', 'j': 'j', 'k': 'k', 'l': 'l', 'm': 'm', 'n': 'n',
    'o': 'o', 'p': 'p', 'q': 'q', 'r': 'r', 's': 's', 't': 't', 'u': 'u',
    'v': 'v', 'w': 'w', 'x': 'x', 'y': 'y', 'z': 'z',
    '1': '1', '2': '2', '3': '3', '4': '4', '5': '5', '6': '6', '7': '7',
    '8': '8', '9': '9', '0': '0',
    ' ': 'spc', '-': 'minus', '=': 'equal', '[': 'bracket_left',
    ']': 'bracket_right', ';': 'semicolon', "'": 'apostrophe',
    '`': 'grave_accent', '\\': 'backslash', ',': 'comma', '.': 'dot',
    '/': 'slash', '\n': 'ret',
}


class QMPClient:
    def __init__(self, host, port, timeout=10):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.buf = b""
        self._read_json()  # greeting

    def _read_json(self):
        while True:
            try:
                line, _, rest = self.buf.partition(b"\n")
                if line:
                    self.buf = rest
                    return json.loads(line)
                data = self.sock.recv(4096)
                if not data:
                    raise ConnectionError("QMP closed")
                self.buf += data
            except socket.timeout:
                raise

    def cmd(self, execute, **args):
        msg = {"execute": execute}
        if args:
            msg["arguments"] = args
        raw = json.dumps(msg)
        if os.environ.get("SMOKE_DEBUG"):
            print(f"  [qmp] >> {raw}")
        self.sock.sendall(raw.encode())
        while True:
            resp = self._read_json()
            if os.environ.get("SMOKE_DEBUG"):
                print(f"  [qmp] << {resp}")
            if "return" in resp or "error" in resp:
                return resp


def send_string(qmp, text):
    for ch in text:
        key = KEYMAP.get(ch)
        if key is None:
            print(f"  [warn] no key for {ch!r}, skipping")
            continue
        qmp.cmd("send-key", keys=[{"type": "qcode", "data": key}])
        time.sleep(0.12)  # ช้าๆ หน่อย กัน QEMU กลืนคีย์


def main():
    if os.path.exists(SERIAL_LOG):
        os.remove(SERIAL_LOG)

    qemu = subprocess.Popen([
        "qemu-system-i386",
        "-kernel", KERNEL,
        "-m", "64M",
        "-serial", f"file:{SERIAL_LOG}",
        "-display", "none",
        "-qmp", f"tcp:{QMP_HOST}:{QMP_PORT},server,nowait",
        "-no-reboot",
    ], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    try:
        # รอ QMP port เปิด
        qmp = None
        for _ in range(50):
            try:
                qmp = QMPClient(QMP_HOST, QMP_PORT)
                break
            except OSError:
                time.sleep(0.2)
        if qmp is None:
            print("FAIL: QMP port never opened")
            return 1
        qmp.cmd("qmp_capabilities")

        def read_serial():
            try:
                with open(SERIAL_LOG, "rb") as f:
                    return f.read().decode("utf-8", errors="replace")
            except FileNotFoundError:
                return ""

        # รอจน kernel boot จริง (เห็น prompt ใน serial) — TCG emulation บน CI ช้ากว่า local
        # มาก ห้ามใช้ sleep คงที่
        boot_deadline = time.time() + 120
        while time.time() < boot_deadline:
            if "MeowOS>" in read_serial():
                break
            time.sleep(0.5)
        else:
            print("FAIL: kernel never printed the shell prompt within 120s")
            print(read_serial()[-2000:])
            qmp.cmd("quit")
            return 1
        time.sleep(0.5)  # settle

        print("=== Boot output ===")
        print(read_serial())
        print("===================")

        tests = [
            ("help\n", "Available commands"),
            ("meminfo\n", "Free Pages"),
            ("alloc\n", "Allocated 64 bytes"),
            ("free\n", "Freed memory"),
            ("uptime\n", "Uptime"),
            ("ls\n", "Files in /"),
            ("cat hello.txt\n", "Hello World from MeowOS"),
            ("cat readme.txt\n", "MeowOS v0.2"),
            ("task\n", "Task created"),
            ("hello\n", "[hello] Greetings from a REAL user program!"),
            ("user\n", "[hello] Greetings from a REAL user program!"),  # legacy alias
            ("usercrash\n", "[User process terminated.]"),  # Ring 3 เขียน kernel mem → fault → กลับ shell
            ("help\n", "Available commands"),  # ต้องกลับมาทำงานได้หลัง user exit/crash
        ]

        failures = 0
        wait_marker = None    # คำสั่งก่อนหน้าลง user mode → รอ marker จบ user process ก่อนพิมพ์คำถัดไป
        for cmd, expect in tests:
            if wait_marker:
                # state-based: kernel flush input ก่อนพิมพ์ marker เสมอ — เห็น marker = พิมพ์ได้ปลอดภัย
                # (ห้ามรอ prompt 'ใหม่': exit path พิมพ์ prompt ค้างไว้แล้ว จำนวน prompt จะไม่เพิ่มอีก)
                deadline = time.time() + 90
                while time.time() < deadline:
                    if wait_marker in read_serial():
                        break
                    time.sleep(0.5)
                else:
                    print(f"  *** marker {wait_marker!r} never appeared after user program")
                    failures += 1
                wait_marker = None
            print(f"\n>>> {cmd.strip()!r} (expect: {expect!r})")
            baseline = read_serial()   # log ก่อนพิมพ์คำสั่งนี้
            send_string(qmp, cmd)
            # รอจน expect ปรากฏในส่วนที่เพิ่มใหม่ของ serial — TCG บน CI ช้ากว่า local มาก
            # จึงห้ามพึ่ง sleep คงที่; deadline ยาวพอสำหรับ user-mode round trip
            deadline = time.time() + 90
            while time.time() < deadline:
                out = read_serial()
                if expect in out[len(baseline):]:
                    break
                time.sleep(0.5)
            out = read_serial()
            tail = out[-600:].replace("\r", "")
            print(tail)
            if expect not in out[len(baseline):]:
                failures += 1
                print(f"  *** MISSING (90s): {expect!r}")
            if cmd.strip() in ("hello", "user"):
                wait_marker = "[User Process Exited]"
            elif cmd.strip() == "usercrash":
                wait_marker = "[Shell] Returned from user mode."

        print("\n=== SUMMARY ===")
        if failures == 0:
            print("ALL TESTS PASSED")
            code = 0
        else:
            print(f"{failures} TEST(S) FAILED")
            code = 1

        qmp.cmd("quit")
        time.sleep(0.5)
        return code

    finally:
        qemu.terminate()
        try:
            qemu.wait(timeout=3)
        except subprocess.TimeoutExpired:
            qemu.kill()


if __name__ == "__main__":
    sys.exit(main())
