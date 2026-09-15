#include <stdint.h>
#include "userinfo.h"
#include "pmm.h"
#include "paging.h"

extern "C" void vga_puts(const char* str);
extern "C" void vga_putchar(char c);

// ---- Temp mapping window: ให้ kernel เห็น physical page ไหนก็ได้ (#5) ----
// PDE[1023] ชี้ไปที่ temp_page_table (BSS ปกติ) ถาวร — temp_map(X) แค่เขียน PTE[0]
// ทำให้ 0xFFC00000 = physical page X. ปลอดภัยเพราะ translation ไม่ผ่านตารางที่ถูก clear
// (ไม่มี chicken-and-egg แบบตอนชี้ PDE ตรงเข้าตารางใหม่ — ทำให้ fresh[i]=0 fault ทันที)
#define TEMP_WINDOW_BASE 0xFFC00000UL
#define TEMP_PDE_INDEX   1023

static uint32_t temp_page_table[1024] __attribute__((aligned(4096)));

static uint32_t page_directory[1024] __attribute__((aligned(4096)));
static uint32_t first_page_table[1024] __attribute__((aligned(4096)));
static uint32_t user_page_table[1024] __attribute__((aligned(4096)));

// ---- Temp window helpers (CR3 ต้อง active แล้วเท่านั้น) ----
static void temp_map(uint32_t phys) {
    temp_page_table[0] = (phys & 0xFFFFF000) | PAGE_KERNEL_RW;
    asm volatile("invlpg (%0)" :: "r"(TEMP_WINDOW_BASE) : "memory");
}

static void temp_unmap() {
    temp_page_table[0] = 0;
    asm volatile("invlpg (%0)" :: "r"(TEMP_WINDOW_BASE) : "memory");
}

// Map 1 physical page ไปยัง virtual address (สร้าง page table ใหม่ถ้ายังไม่มี)
void map_page(uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t pd_idx = virt / 0x400000;          // หมายเลข Page Directory Entry
    uint32_t pt_idx = (virt / 0x1000) % 1024;   // หมายเลข Page Table Entry

    if (pd_idx == TEMP_PDE_INDEX) return;   // PDE[1023] = temp window เอง — ห้าม map ทับ

    if (!(page_directory[pd_idx] & PAGE_FLAG_PRESENT)) {
        uint32_t pt_phys = reinterpret_cast<uint32_t>(pmm_alloc_block());
        if (!pt_phys) return;

        // เคลียร์ page table ใหม่ผ่าน temp window (physical อาจอยู่เหนือ 4MB)
        temp_map(pt_phys);
        volatile uint32_t* fresh = (volatile uint32_t*)TEMP_WINDOW_BASE;
        for (int i = 0; i < 1024; i++) fresh[i] = 0;
        temp_unmap();

        page_directory[pd_idx] = pt_phys | flags;
    }

    // เขียน PTE ผ่าน temp window เสมอ — ทำงานได้แม้ table อยู่เหนือ 4MB
    temp_map(page_directory[pd_idx] & 0xFFFFF000);
    volatile uint32_t* table = (volatile uint32_t*)TEMP_WINDOW_BASE;
    table[pt_idx] = (phys & 0xFFFFF000) | flags;
    temp_unmap();

    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

// Map physical -> virtual ต่อเนื่องหลายหน้า
void map_region(uint32_t virt_start, uint32_t phys_start, uint32_t size, uint32_t flags) {
    for (uint32_t off = 0; off < size; off += 0x1000) {
        map_page(virt_start + off, phys_start + off, flags);
    }
}

// ---- ส่วนของ User Space ----
extern "C" void user_space_init() {
    // เคลียร์ user page table
    for (int i = 0; i < 1024; i++) user_page_table[i] = 0;

    // ขอ 3 หน้าจาก PMM (ข้อมูล + stack, เริ่มที่ 2MB เพื่อไม่ชน kernel image)
    uint32_t user_phys_start = 2 * 1024 * 1024;
    for (uint32_t off = 0; off < USER_REGION_SIZE; off += 0x1000) {
        uint32_t pt_idx = off / 0x1000;
        user_page_table[pt_idx] = ((user_phys_start + off) & 0xFFFFF000) | PAGE_USER_RW;
        pmm_mark_used((void*)(user_phys_start + off));
    }

    // เสียบ page table เข้ากับ page directory
    uint32_t user_pd_idx = USER_BASE / 0x400000;
    page_directory[user_pd_idx] =
        reinterpret_cast<uint32_t>(user_page_table) | PAGE_USER_RW;
}

extern "C" void init_paging() {
    // Identity map 0x00000000 -> 0x00400000 (4MB) สำหรับ Kernel
    // ★ supervisor-only: Ring 3 แตะ kernel ไม่ได้อีกต่อไป (#1)
    for (int i = 0; i < 1024; i++) {
        first_page_table[i] = (i * 0x1000) | PAGE_KERNEL_RW;
    }
    page_directory[0] = reinterpret_cast<uint32_t>(first_page_table) | PAGE_KERNEL_RW;

    for (int i = 1; i < 1024; i++) {
        page_directory[i] = 0x00000002; // Not Present, Read/Write
    }

    // PDE[1023] → temp_page_table: ทำให้ temp window ใช้งานได้ทันทีหลังเปิด paging (#5)
    page_directory[TEMP_PDE_INDEX] = reinterpret_cast<uint32_t>(temp_page_table) | PAGE_KERNEL_RW;

    // Load Page Directory to CR3 ก่อน เพื่อให้ temp window ใช้งานได้
    asm volatile("mov %0, %%cr3" :: "r"(page_directory));

    // Enable Paging (Bit 31 of CR0)
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" :: "r"(cr0));

    user_space_init();
}

// ให้ kernel มองเห็น physical address ใดๆ (อ่านเท่านั้น — อย่าเก็บ pointer ไว้!)
uint32_t paging_get_phys(uint32_t virt) {
    uint32_t pd_idx = virt / 0x400000;
    if (!(page_directory[pd_idx] & PAGE_FLAG_PRESENT)) return 0;

    uint32_t table_phys = page_directory[pd_idx] & 0xFFFFF000;
    temp_map(table_phys);
    volatile uint32_t* table = (volatile uint32_t*)TEMP_WINDOW_BASE;
    uint32_t pt_idx = (virt / 0x1000) % 1024;
    uint32_t pte = table[pt_idx];
    temp_unmap();

    if (!(pte & PAGE_FLAG_PRESENT)) return 0;
    return (pte & 0xFFFFF000) + (virt & 0xFFF);
}

// ---- Page Fault Handler (INT 14) ----
static void print_hex32(uint32_t val) {
    char hex[] = "0123456789ABCDEF";
    for (int i = 7; i >= 0; i--) {
        vga_putchar(hex[(val >> (i * 4)) & 0xF]);
    }
}

extern "C" void page_fault_handler(uint32_t err_code, uint32_t int_no) {
    (void)int_no;
    uint32_t fault_addr;
    asm volatile("mov %%cr2, %0" : "=r"(fault_addr));

    vga_puts("\n=== PAGE FAULT ===");
    vga_puts("\nFaulting Address: 0x");
    print_hex32(fault_addr);

    vga_puts("\nError Code: 0x");
    print_hex32(err_code);
    vga_puts(err_code & 0x1 ? " [Protection Violation]" : " [Page Not Present]");
    vga_puts(err_code & 0x2 ? " [Write]" : " [Read]");
    vga_puts(err_code & 0x4 ? " [User Mode]" : " [Kernel Mode]");

    vga_puts("\nSystem halted.");
    while (1) { asm volatile("cli; hlt"); }
}
