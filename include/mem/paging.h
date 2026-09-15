#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

// ---- Page flags (bit ของ PTE/PDE) — single source of truth ระหว่าง paging.cpp / heap.cpp ----
#define PAGE_FLAG_PRESENT 0x01
#define PAGE_FLAG_WRITE   0x02
#define PAGE_FLAG_USER    0x04 // User/Supervisor (0 = Ring 0-2, 1 = Ring 3)

// Kernel pages: supervisor เท่านั้น — Ring 3 เข้าถึงไม่ได้ (#1 safety fix)
#define PAGE_KERNEL_RW (PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE)
// User pages: Ring 3 อ่าน/เขียน/รันได้
#define PAGE_USER_RW   (PAGE_FLAG_PRESENT | PAGE_FLAG_WRITE | PAGE_FLAG_USER)

extern "C" {
void init_paging();
void user_space_init();
void map_page(uint32_t virt, uint32_t phys, uint32_t flags);
void map_region(uint32_t virt_start, uint32_t phys_start, uint32_t size, uint32_t flags);
uint32_t paging_get_phys(uint32_t virt);
void page_fault_handler(uint32_t err_code, uint32_t int_no);
}

#endif