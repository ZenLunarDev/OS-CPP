#!/usr/bin/env python3
"""MeowOS framebuffer test: boots the kernel, runs `gui` (Bochs dispi 1024x768x16 + demo frame),
grabs a QMP screendump and verifies exact pixels — proving the LFB mapping, double-buffered
present, and drawing primitives all actually reached the real display. Then `text` must restore
text mode and the shell must keep working."""
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from smoke_test import (QMPClient, QMP_HOST, QMP_PORT, KERNEL, SERIAL_LOG,  # noqa: E402
                        QEMU_STDERR, send_string)

PPM_PATH = os.path.abspath("meow_fb.ppm")


def c565(r5, g6, b5):
    """RGB565 → RGB888 แบบ bit-replication (สูตรเดียวกับที่ QEMU/pixman ใช้ตอน screendump)"""
    return ((r5 << 3) | (r5 >> 2), (g6 << 2) | (g6 >> 4), (b5 << 3) | (b5 >> 2))


# ค่าคงที่เดียวกับ include/drivers/vga_fb.h
BLACK = c565(0, 0, 0)
WHITE = c565(31, 63, 31)
RED = c565(31, 0, 0)
GREEN = c565(0, 31, 0)
BLUE = c565(0, 0, 31)
YELLOW = c565(31, 63, 0)
MAGENTA = c565(31, 0, 31)
DARKGRAY = c565(8, 16, 8)

# ---- จุดตรวจ — ตาม layout ของ fb_draw_demo() ----
# mode 'px'  : pixel (x,y) ต้องเป็นสีนั้น (±TOL)
# mode 'scan': ในกรอบ (x,y,w,h) ต้องมี pixel เป็นสีนั้นอย่างน้อย 1 จุด (ทนต่อรายละเอียด glyph)
CHECKS = [
    ("px",   150, 130, RED,      "red fill-rect interior"),
    ("px",   100, 100, WHITE,    "rect white border (tl corner)"),
    ("px",   240, 130, BLACK,    "blue-rect interior stays black"),
    ("px",   400, 200, GREEN,    "green fill-circle center"),
    ("px",   350, 200, WHITE,    "circle white outline (left edge)"),
    ("px",   650, 370, MAGENTA,  "magenta circle outline (top)"),
    ("px",   900, 300, YELLOW,   "yellow line endpoint"),
    ("px",   512,  12, DARKGRAY, "title bar darkgray"),
    ("scan",   8,   4, 300, 16, WHITE, "title text rendered (font ROM)"),
]
TOL = 4


def load_ppm(path):
    """แปลง PPM (P6) ของ QEMU screendump เป็น (w, h, bytes) — ข้าม comment ให้ด้วย"""
    with open(path, "rb") as f:
        data = f.read()
    fields = []
    idx = 0
    while len(fields) < 4:
        while idx < len(data) and data[idx:idx + 1].isspace():
            idx += 1
        if data[idx:idx + 1] == b"#":
            while idx < len(data) and data[idx:idx + 1] != b"\n":
                idx += 1
            continue
        start = idx
        while idx < len(data) and not data[idx:idx + 1].isspace():
            idx += 1
        fields.append(data[start:idx])
    idx += 1  # whitespace เดียวหลัง maxval ก่อนเข้าข้อมูล binary
    w, h = int(fields[1]), int(fields[2])
    return w, h, data[idx:]


def pixel(buf, w, x, y):
    o = 3 * (y * w + x)
    return buf[o], buf[o + 1], buf[o + 2]


def run_checks(w, h, buf):
    """คืน (จำนวนที่ผิด, รายการข้อความ error)"""
    bad = []
    for chk in CHECKS:
        if chk[0] == "px":
            _, x, y, want, name = chk
            got = pixel(buf, w, x, y)
            if any(abs(g - wnt) > TOL for g, wnt in zip(got, want)):
                bad.append(f"pixel ({x},{y}) {name}: got RGB{got}, want RGB{want}")
        else:  # scan
            _, x, y, sw, sh, want, name = chk
            found = False
            for yy in range(y, min(y + sh, h)):
                for xx in range(x, min(x + sw, w)):
                    got = pixel(buf, w, xx, yy)
                    if all(abs(g - wnt) <= TOL for g, wnt in zip(got, want)):
                        found = True
                        break
                if found:
                    break
            if not found:
                bad.append(f"scan ({x},{y}+{sw}x{sh}) {name}: color never found")
    return bad


def main():
    for p in (SERIAL_LOG, QEMU_STDERR, PPM_PATH):
        if os.path.exists(p):
            os.remove(p)

    qemu_err_f = open(QEMU_STDERR, "wb")
    qemu = subprocess.Popen([
        "qemu-system-i386",
        "-kernel", KERNEL,
        "-m", "64M",
        "-serial", f"file:{SERIAL_LOG}",
        "-display", "none",
        "-qmp", f"tcp:{QMP_HOST}:{QMP_PORT},server=on,wait=off",
        "-no-reboot",
    ], stdout=qemu_err_f, stderr=subprocess.STDOUT)

    try:
        # ---- รอ QMP (เหมือน smoke_test) ----
        qmp = None
        for _ in range(150):
            if qemu.poll() is not None:
                break
            try:
                qmp = QMPClient(QMP_HOST, QMP_PORT)
                break
            except OSError:
                time.sleep(0.2)
        if qmp is None:
            print(f"FAIL: QMP port never opened (qemu exit code: {qemu.poll()})")
            try:
                print("--- qemu stderr/stdout tail ---")
                print(open(QEMU_STDERR, "r", errors="replace").read()[-1500:])
            except Exception:
                pass
            return 1
        qmp.cmd("qmp_capabilities")

        def read_serial():
            try:
                with open(SERIAL_LOG, "rb") as f:
                    return f.read().decode("utf-8", errors="replace")
            except FileNotFoundError:
                return ""

        def wait_for(needle, timeout, what):
            deadline = time.time() + timeout
            while time.time() < deadline:
                out = read_serial()
                if needle in out:
                    return out
                if qemu.poll() is not None:
                    print(f"FAIL: qemu exited early while waiting for {what}")
                    return None
                time.sleep(0.5)
            print(f"FAIL: {what} never appeared within {timeout}s")
            print(read_serial()[-1500:])
            return None

        # ---- boot ----
        if wait_for("MeowOS>", 120, "shell prompt") is None:
            return 1
        print("=== Boot OK ===")

        # ---- gui: เข้า graphics mode + demo frame ----
        send_string(qmp, "gui\n")
        if wait_for("frame presented", 90, "[gui] frame presented") is None:
            return 1
        time.sleep(1.0)  # ให้ present (memcpy 1.5MB บน TCG) เสร็จก่อน screendump

        # ---- screendump + ตรวจ pixel (poll ซ้ำได้ — TCG บน CI ช้า) ----
        bad = None
        deadline = time.time() + 60
        while time.time() < deadline:
            resp = qmp.cmd("screendump", filename=PPM_PATH)
            if "error" in resp:
                print(f"FAIL: screendump error: {resp['error']}")
                return 1
            try:
                w, h, buf = load_ppm(PPM_PATH)
            except Exception as e:
                print(f"  (ppm not ready: {e})")
                time.sleep(1)
                continue
            if w != 1024 or h != 768:
                print(f"FAIL: dump is {w}x{h}, expected 1024x768")
                return 1
            bad = run_checks(w, h, buf)
            if not bad:
                break
            time.sleep(2)  # อาจ dump ก่อน present เสร็จ — ลองใหม่

        print(f"=== {len(CHECKS)} display checks across 1024x768 dump ===")
        if bad:
            for b in bad:
                print(f"  *** {b}")
            print(f"FAIL: {len(bad)} check(s) wrong")
            return 1
        print("FB PIXELS VERIFIED: direct rendering works end-to-end")

        # ---- text: กลับ text mode ได้ ----
        send_string(qmp, "text\n")
        if wait_for("back to text mode", 30, "[text] restore") is None:
            return 1

        # ---- shell ยังมีชีวิต (ทั้ง serial และ input path) ----
        send_string(qmp, "help\n")
        if wait_for("Available commands", 30, "shell alive after text restore") is None:
            return 1

        print("\n=== FB TEST SUMMARY ===")
        print("ALL FB TESTS PASSED")
        qmp.cmd("quit")
        time.sleep(0.5)
        return 0

    finally:
        qemu.terminate()
        try:
            qemu.wait(timeout=3)
        except Exception:
            qemu.kill()


if __name__ == "__main__":
    sys.exit(main())
