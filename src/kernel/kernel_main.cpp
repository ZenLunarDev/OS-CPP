#include "gdt.h"
#include "pmm.h"
#include "heap.h"
#include "timer.h"
#include "vfs.h"

extern "C" void init_interrupts();
extern "C" void init_paging();
extern "C" void vga_clear();
extern "C" void vga_puts(const char* str);
extern "C" void vga_putchar(char c);
extern "C" void user_mode_loop();  // shell main loop (shell_engine.cpp) — ไม่กลับมา

// จาก boot.asm — multiboot magic + info pointer
extern "C" uint32_t multiboot_magic;
extern "C" uint32_t multiboot_info;

extern "C" void kernel_main() {
    GDT::init();
    init_paging();
    init_interrupts();

    // ---- PMM ----
    if (multiboot_magic == 0x2BADB002 && multiboot_info != 0) {
        // multiboot_info->flags bit 6 = mmap present
        uint32_t flags = *reinterpret_cast<uint32_t*>(multiboot_info);
#ifdef PMM_DEBUG
        {
            vga_puts("\n[DBG] magic ok, flags=");
            for (int i = 7; i >= 0; i--) vga_putchar("0123456789ABCDEF"[(flags >> (i*4)) & 0xF]);
        }
#endif
        if (flags & (1 << 6)) {
            // multiboot_info layout: mmap_length = field 11 (offset 44), mmap_addr = field 12 (offset 48)
            uint32_t mmap_len  = *reinterpret_cast<uint32_t*>(multiboot_info + 11 * 4);
            uint32_t mmap_addr = *reinterpret_cast<uint32_t*>(multiboot_info + 12 * 4);
            pmm_init_from_mmap(mmap_addr, mmap_len);
        } else {
            // fallback: mem_upper คือ field ที่ 2 (offset 8)
            uint32_t mem_upper = *reinterpret_cast<uint32_t*>(multiboot_info + 2 * 4);
            pmm_init(mem_upper);
        }
    } else {
        pmm_init(31 * 1024); // สมมติ 32MB — fallback ถ้าไม่มี multiboot info
    }
    pmm_finalize();

    // ---- Heap ----
    heap_init();

    // ---- VFS ----
    VFS_init();

    // ---- Timer (100 Hz) ----
    init_timer(100);

    vga_clear();
    vga_puts("MeowOS Kernel Loaded\nMeowOS> ");

    __asm__ volatile("sti");

    // ลง shell loop ตลอดชีพ — user_mode_resume/kill_user_process จะ jmp เข้าที่นี่
    // พร้อม stack สะอาด ทำให้ syscall path ต่อจาก Ring 3 กลับมาได้พอดี
    user_mode_loop();

    for (;;) { __asm__ volatile("hlt"); }  // ไม่ควรมาถึง
}