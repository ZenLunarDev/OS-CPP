#include "syscall.h"
#include "userinfo.h"

extern "C" void vga_puts(const char* str);
extern "C" uint32_t get_ticks();
extern "C" void user_mode_return(uint32_t new_stack);  // noreturn — user_mode.asm
extern "C" void shell_flush_input();                    // shell_engine.cpp — ล้าง keyboard buffer

// set โดย shell ก่อน enter Ring 3 — บอกว่า kernel stack ของ shell อยู่ที่ไหน
static uint32_t g_user_kernel_esp = 0;

extern "C" void set_user_kernel_esp(uint32_t esp) {
    g_user_kernel_esp = esp;
}

// ---- #2: ตรวจ pointer จาก Ring 3 — ห้ามชี้เข้ามาใน address ของ kernel ----
static bool user_ptr_ok(uint32_t addr, uint32_t ring) {
    if (ring != 3) return true;   // kernel เรียกเอง เชื่อได้
    return addr >= USER_BASE && addr < 0xFFC00000;  // อยู่ในช่วง user space เท่านั้น
}

extern "C" uint32_t syscall_handler(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t caller_ring) {
    (void)arg2;
    (void)arg3;

    switch (num) {
        case SYS_WRITE:
            if (!user_ptr_ok(arg1, caller_ring)) {
                vga_puts("\n[SECURITY] Ring 3 syscall with kernel pointer blocked!");
                return -1;
            }
            vga_puts(reinterpret_cast<const char*>(arg1));
            return 0;

        case SYS_GETTICKS:
            return get_ticks();        case SYS_EXIT: {
            // ทิ้ง keystroke ที่พิมพ์ค้างระหว่าง user run ก่อนกลับ shell
            // (กัน buffer เก่ากลายเป็น command มั่ว) — เหมือนที่ kill_user_process ทำ
            shell_flush_input();

            vga_puts("\n[User Process Exited]");            if (g_user_kernel_esp == 0) {
                // เรียกจาก Ring 0 โดยไม่มี user context — ไม่มีทางกลับ ต้อง halt
                vga_puts("\n[SYS_EXIT] No user context. Halting.");
                while (1) { asm volatile("cli; hlt"); }
            }
            uint32_t target_stack = g_user_kernel_esp;
            g_user_kernel_esp = 0;

            // กลับ shell: สลับไป kernel stack สะอาด (จุดที่ run_user_program ตั้ง TSS esp0 ไว้)
            // โหลด segment กลับ Ring 0 แล้ว jump เข้า shell loop ตรงๆ (noreturn)
            uint32_t new_stack = target_stack - 16;
            user_mode_return(new_stack);
            return 0;   // ไม่มีทางมาถึง
        }

        default:
            vga_puts("\nUnknown Syscall Called!");
            return -1;
    }
}