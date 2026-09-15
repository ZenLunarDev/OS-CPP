#include <stdint.h>
#include <stdbool.h>
#include "keyboard_ringbuffer.h"
#include "pmm.h"
#include "heap.h"
#include "timer.h"
#include "rtc.h"
#include "task.h"
#include "gdt.h"
#include "paging.h"
#include "syscall.h"
#include "vfs.h"
#include "userinfo.h"
#include "vga_fb.h"

static void* test_ptr = nullptr;

#define CMD_MAX_LEN 128
static char cmd_buffer[CMD_MAX_LEN];
static uint8_t cmd_idx = 0;
static bool shift_pressed = false;

// Kernel stack สำหรับ Ring 3 (ใช้ทั้ง TSS esp0 และ return path กลับ shell)
// non-static เพราะ syscall.cpp อ้าง symbol นี้ตรงๆ (braced extern "C" = definition จริง)
extern "C" {
uint8_t kernel_stack_for_tss[4096] __attribute__((aligned(4096)));
}

extern "C" void vga_putchar(char c);
extern "C" void vga_puts(const char* str);
extern "C" void vga_clear();
extern "C" void enter_user_mode(uint32_t entry_point, uint32_t user_stack);
extern "C" void user_mode_return(uint32_t new_stack);  // noreturn (user_mode.asm)

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static const char* skip_whitespace(const char* str) {
    while (*str == ' ' || *str == '\t') str++;
    return str;
}

static bool streq(const char* a, const char* b) {
    a = skip_whitespace(a);
    b = skip_whitespace(b);
    while (*a && (*a == *b)) { a++; b++; }
    return (*a - *b) == 0;
}

static const char scancode_map_normal[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

static const char scancode_map_shift[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' '
};

extern "C" void irq1_c_handler(uint32_t scancode) {
    ringbuffer_push(static_cast<uint8_t>(scancode));
}

static void print_number(uint32_t val, bool pad) {
    if (pad && val < 10) vga_putchar('0');
    char buf[16];
    int idx = 0;
    if (val == 0) buf[idx++] = '0';
    else {
        char temp[16];
        int t_idx = 0;
        while (val > 0) { temp[t_idx++] = '0' + (val % 10); val /= 10; }
        while (t_idx > 0) buf[idx++] = temp[--t_idx];
    }
    buf[idx] = '\0';
    vga_puts(buf);
}

static void background_task_demo() {
    vga_puts("\n[Task 1] Hello from background task!");
    for (;;) { asm volatile("hlt"); }   // ห้าม return (Task struct ไม่มี return address)
}

// ---- Flat Binary User Programs (loaded at USER_BASE at runtime) ----
// hello: link ที่ 0x40000000 + objcopy -O binary (symbols จาก object ที่ objcopy -I binary สร้าง)
// usercrash: nasm -f bin ตรงๆ (org 0x40000000)
extern "C" uint8_t _binary_hello_bin_start[];
extern "C" uint8_t _binary_hello_bin_size[];
extern "C" uint8_t _binary_usercrash_bin_start[];
extern "C" uint8_t _binary_usercrash_bin_size[];

// ---- Syscall helpers ใน asm (ป้องกัน compiler ย้าย entry ไปหลัง int) ----
extern "C" void user_syscall_write();
extern "C" void user_syscall_exit();
extern "C" void user_syscall_getticks();

// ---- Built-in commands ที่โหลดเข้า user space ----
struct UserProgram {
    const char* name;
    const uint8_t* data;
    uint32_t size;
};

static const UserProgram user_programs[] = {
    { "hello", _binary_hello_bin_start,
      reinterpret_cast<uint32_t>(_binary_hello_bin_size) },
};

#define USER_PROGRAM_COUNT (sizeof(user_programs) / sizeof(user_programs[0]))

// ฝั่ง kernel: จุดกลับเข้า shell หลัง user process จบ (อยู่ใน user_mode.asm)
extern "C" void user_mode_return_point();

// โหลดโปรแกรม user ลง user space แล้วลง Ring 3 (ทางกลับผ่าน kernel stack)
static void run_user_program(const uint8_t* data, uint32_t size) {
    volatile uint8_t* dst = (volatile uint8_t*)USER_BASE;
    for (uint32_t i = 0; i < size; i++) {
        dst[i] = data[i];
    }

    // หยุด scheduler ชั่วคราว — ระหว่าง user run, kernel stack นี้เป็นของ syscall/pagefault
    // ถ้า timer สลับ task ไป จะทำให้ return path ใช้ stack ที่ถูกทับทิ้ง
    scheduler_pause(true);

    // TSS esp0 ชี้กลาง stack นี้ — ทุก int 0x80 / fault จาก Ring 3 เข้าที่นี่
    // ทางกลับ (SYS_EXIT หรือ fault) คือ user_mode_return() ซึ่งสลับไป stack สะอาด
    // แล้ว jump เข้า user_mode_loop โดยตรง — ไม่พึ่ง crafted iretd frame เลย
    uint32_t kstack_top = reinterpret_cast<uint32_t>(kernel_stack_for_tss + sizeof(kernel_stack_for_tss));
    set_kernel_stack(kstack_top);
    set_user_kernel_esp(kstack_top);   // SYS_EXIT ใช้เป็นจุดเริ่ม return stack

    enter_user_mode(USER_BASE, USER_BASE + 0x1000);   // ไม่มีทางกลับมา
}

// จุดกลับเข้า shell หลัง user process พัง (exception จาก Ring 3)
// เรียกจาก isr_dispatch บน kernel stack ของ fault handler — สลับไป kernel stack
// ของ shell แล้ว jump เข้า shell loop ตรงๆ (user_mode_return ทำให้)
// ล้าง keyboard ring buffer (ใช้ตอนกลับจาก user mode)
extern "C" void shell_flush_input() {
    uint8_t junk;
    while (ringbuffer_pop(&junk)) {}
    // ทิ้งบรรทัดค้างด้วย: execute_command ออกทาง enter_user_mode (noreturn)
    // จึงไม่เคยผ่านโค้ด reset ของ shell_update — cmd_buffer ยังกองคำสั่งเก่าอยู่
    cmd_idx = 0;
    cmd_buffer[0] = '\0';
    shift_pressed = false;
}

extern "C" void kill_user_process() {
    scheduler_pause(false);   // เปิด scheduler กลับมาก่อนอย่างอื่น

    // ทิ้ง keystroke ที่พิมพ์ค้างระหว่าง user run — ไม่งั้นจะกลายเป็น command มั่วหลังกลับ shell
    shell_flush_input();
    vga_puts("\n[Shell] Returned from user mode.\nMeowOS> ");

    // ยังอยู่บน kernel stack ของ fault handler (TSS esp0) — สลับไป stack สะอาด
    // ของ shell แล้ว jump เข้า shell loop ตรงๆ
    user_mode_return(reinterpret_cast<uint32_t>(kernel_stack_for_tss + sizeof(kernel_stack_for_tss)) - 16);
}

static void execute_command(const char* raw_cmd) {
    const char* cmd = skip_whitespace(raw_cmd);
    if (cmd[0] == '\0') return;

    if (streq(cmd, "help")) {
        vga_puts("\nAvailable commands: help, cls, reboot, meminfo, alloc, free, uptime, date, crash, ls, cat, task, sysdemo, user, gui, text");
    } else if (streq(cmd, "cls")) {
        vga_clear();
    } else if (streq(cmd, "reboot")) {
        vga_puts("\nRebooting system...");
        uint8_t good = 0x02;
        while (good & 0x02) good = inb(0x64);
        outb(0x64, 0xFE);
    } else if (streq(cmd, "meminfo")) {
        vga_puts("\nFree Pages (4KB): ");
        print_number(pmm_get_free_block_count(), false);
    } else if (streq(cmd, "alloc")) {
        if (!test_ptr) {
            test_ptr = kmalloc(64);
            if (!test_ptr) {
                vga_puts("\nAllocation failed (heap exhausted?)");
                return;
            }
            vga_puts("\nAllocated 64 bytes at: 0x");
            uint32_t addr = reinterpret_cast<uint32_t>(test_ptr);
            char hex[] = "0123456789ABCDEF";
            for (int i = 7; i >= 0; i--) {
                vga_putchar(hex[(addr >> (i * 4)) & 0xF]);
            }
        } else {
            vga_puts("\nAlready allocated. Run 'free' first.");
        }
    } else if (streq(cmd, "free")) {
        if (test_ptr) {
            kfree(test_ptr);
            test_ptr = nullptr;
            vga_puts("\nFreed memory successfully.");
        } else {
            vga_puts("\nNothing to free.");
        }
    } else if (streq(cmd, "uptime")) {
        vga_puts("\nUptime: ");
        print_number(get_ticks() / 100, false);
        vga_puts(" seconds");
    } else if (streq(cmd, "date")) {
        RTCData rtc;
        read_rtc(&rtc);

        vga_puts("\nDate: ");
        print_number(rtc.year, false); vga_putchar('-');
        print_number(rtc.month, true); vga_putchar('-');
        print_number(rtc.day, true); vga_putchar(' ');
        print_number(rtc.hour, true); vga_putchar(':');
        print_number(rtc.minute, true); vga_putchar(':');
        print_number(rtc.second, true);
    } else if (streq(cmd, "crash")) {
        volatile int x = 0;
        volatile int y = 10 / x;
        (void)y;
    } else if (streq(cmd, "ls")) {
        VFS_dump();
    } else if (streq(cmd, "cat hello.txt")) {
        VFS_read_file(0);
    } else if (streq(cmd, "cat readme.txt")) {
        VFS_read_file(1);
    } else if (streq(cmd, "task")) {
        scheduler_pause(false);   // เปิด preemption กลับมา — background task ต้องโดน timer
        init_multitasking();
        create_task(background_task_demo);
        vga_puts("\nTask created. Scheduling active.");
    } else if (streq(cmd, "sysdemo")) {
        // ยังอยู่ Ring 0 — เรียก handler ตรงๆ เพื่อโชว์ path เดียวกับ INT 0x80
        syscall_handler(SYS_WRITE, reinterpret_cast<uint32_t>("\n[Syscall] Hello from INT 0x80 handler!"), 0, 0, 0);
    } else if (streq(cmd, "hello")) {
        run_user_program(user_programs[0].data, user_programs[0].size);
    } else if (streq(cmd, "usercrash")) {
        // พิสูจน์ #1: user code พยายามเขียน kernel memory แล้วต้อง page fault ทันที
        run_user_program(_binary_usercrash_bin_start,
                         reinterpret_cast<uint32_t>(_binary_usercrash_bin_size));
    } else if (streq(cmd, "user")) {
        // legacy: รัน 'hello' ผ่านชื่อเดิม
        vga_puts("\nSwitching execution to Ring 3 (User Mode)...");
        run_user_program(user_programs[0].data, user_programs[0].size);
    } else if (streq(cmd, "gui")) {
        // ---- GUI stack: เข้า graphics mode + วาด demo frame (direct rendering) ----
        vga_puts("\n[gui] entering graphics mode 1024x768x16...");
        if (fb_enter_graphics()) {
            fb_draw_demo();
            vga_puts("\n[gui] frame presented. VGA text output now goes to serial only.");
        } else {
            vga_puts("\n[gui] framebuffer unavailable (init failed?)");
        }
    } else if (streq(cmd, "text")) {
        // ---- กลับ text mode 80x25 — shell กลับมาเห็นบนจอปกติ ----
        if (fb_available()) {
            fb_text_mode();
            vga_clear();   // VRAM ตอนนี้มีขยะจากตอน graphics mode — ล้างก่อนพิมพ์ต่อ
            vga_puts("[text] back to text mode.");   // prompt พิมพ์โดย shell_update เอง
        } else {
            vga_puts("\n[text] already in text mode.");
        }
    } else {
        vga_puts("\nUnknown command: ");
        vga_puts(cmd);
    }
}

extern "C" void shell_update() {
    uint8_t scancode;
    while (ringbuffer_pop(&scancode)) {
        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = true;
            continue;
        }
        if (scancode == (0x2A | 0x80) || scancode == (0x36 | 0x80)) {
            shift_pressed = false;
            continue;
        }

        if (scancode & 0x80) continue;

        char ascii = shift_pressed ? scancode_map_shift[scancode & 0x7F] : scancode_map_normal[scancode & 0x7F];
        if (!ascii) continue;

        if (ascii == '\n') {
            cmd_buffer[cmd_idx] = '\0';
            execute_command(cmd_buffer);
            cmd_idx = 0;
            vga_puts("\nMeowOS> ");
        } else if (ascii == '\b') {
            if (cmd_idx > 0) {
                cmd_idx--;
                vga_putchar('\b');
            }
        } else if (cmd_idx < CMD_MAX_LEN - 1) {
            cmd_buffer[cmd_idx++] = ascii;
            vga_putchar(ascii);
        }
    }
}

// Shell main loop — kernel_main เข้ามาที่นี่ และ user-mode return path (ใน user_mode.asm)
// ก็ jump เข้าที่นี่เช่นกัน ทำให้กลับจาก Ring 3 ได้อย่างเป็นทางการพร้อม stack สะอาด
extern "C" void user_mode_loop() {
    for (;;) {
        shell_update();
        __asm__ volatile("hlt");
    }
}
