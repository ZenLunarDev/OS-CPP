#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096

extern "C" {
void pmm_init(uint32_t mem_upper_kb);
void pmm_init_from_mmap(uint32_t mmap_addr, uint32_t mmap_length);
void pmm_finalize();
void* pmm_alloc_block();
void pmm_alloc_blocks(uint32_t count, void** out);
void pmm_free_block(void* ptr);
void pmm_mark_used(void* ptr);
uint32_t pmm_get_free_block_count();
}

#endif