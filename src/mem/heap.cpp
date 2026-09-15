#include "heap.h"
#include "pmm.h"
#include "paging.h"

extern "C" void vga_puts(const char* str);

// ---- #5b: Heap อยู่ใน virtual region 0xC0000000 (kernel space, supervisor-only) ----
// physical pages มาจาก PMM ที่ไหนก็ได้ (รวม >4MB) — heap code มองเป็น virtual เท่านั้น
#define HEAP_VIRT_BASE 0xC0000000UL
#define HEAP_VIRT_PAGES 1024                  // ceiling: 4MB virtual
#define HEAP_SEED_PAGES 4                     // เริ่มต้น 16KB

struct Header {
    uint32_t size;      // ขนาด data (ไม่รวม header)
    uint32_t magic;     // HEAP_MAGIC = valid block
    uint32_t is_free;
    Header* next;
};

static Header* heap_start = nullptr;
static const uint32_t HEAP_MAGIC = 0xDEADC0DE;

// ---- Page level: map/unmap virtual page ใน HEAP region (PMM-backed) ----
static uint32_t heap_pages_mapped = 0;                 // จำนวน virtual page ที่ map แล้ว
static uint32_t heap_mapped_phys[HEAP_VIRT_PAGES];     // phys ของแต่ละ virtual page (0 = ยังไม่ map)

// map virtual page vpage_idx -> PMM page ใหม่
static bool heap_map_page(uint32_t vpage_idx) {
    void* phys = pmm_alloc_block();
    if (!phys) return false;
    map_page(HEAP_VIRT_BASE + vpage_idx * PAGE_SIZE,
             reinterpret_cast<uint32_t>(phys), PAGE_KERNEL_RW);
    heap_mapped_phys[vpage_idx] = reinterpret_cast<uint32_t>(phys);
    return true;
}

extern "C" void heap_init() {
    heap_pages_mapped = 0;
    for (uint32_t i = 0; i < HEAP_VIRT_PAGES; i++) heap_mapped_phys[i] = 0;

    for (uint32_t i = 0; i < HEAP_SEED_PAGES; i++) {
        if (!heap_map_page(i)) {
            vga_puts("\n[heap] FATAL: PMM exhausted at init");
            return;
        }
        heap_pages_mapped++;
    }

    heap_start = reinterpret_cast<Header*>(HEAP_VIRT_BASE);
    heap_start->size = HEAP_SEED_PAGES * PAGE_SIZE - sizeof(Header);
    heap_start->magic = HEAP_MAGIC;
    heap_start->is_free = 1;
    heap_start->next = nullptr;
}

static Header* find_free(uint32_t size) {
    Header* curr = heap_start;
    while (curr) {
        if (curr->is_free && curr->size >= size) return curr;
        curr = curr->next;
    }
    return nullptr;
}

static void split_block(Header* block, uint32_t size) {
    if (block->size >= size + sizeof(Header) + 4) {
        Header* next_block = (Header*)((uint8_t*)block + sizeof(Header) + size);
        next_block->size = block->size - size - sizeof(Header);
        next_block->magic = HEAP_MAGIC;
        next_block->is_free = 1;
        next_block->next = block->next;
        block->size = size;
        block->next = next_block;
    }
}

// ขยาย heap: map N pages ใหม่ต่อท้าย region แล้วคืน block ใหม่ที่ท้าย list
static Header* heap_grow(uint32_t pages) {
    if (heap_pages_mapped + pages > HEAP_VIRT_PAGES) return nullptr;

    for (uint32_t k = 0; k < pages; k++) {
        if (!heap_map_page(heap_pages_mapped + k)) return nullptr;
    }
    heap_pages_mapped += pages;

    Header* tail = heap_start;
    while (tail->next) tail = tail->next;

    Header* new_block = (Header*)(HEAP_VIRT_BASE + (heap_pages_mapped - pages) * PAGE_SIZE);
    new_block->size = pages * PAGE_SIZE - sizeof(Header);
    new_block->magic = HEAP_MAGIC;
    new_block->is_free = 1;
    new_block->next = nullptr;
    tail->next = new_block;
    return new_block;
}

extern "C" void* kmalloc(size_t size) {
    if (!heap_start || size == 0) return nullptr;
    size = (size + 3) & ~3u;

    Header* block = find_free(size);
    if (!block) {
        // ---- ขยาย heap: map หน้าใหม่จาก PMM ผ่าน virtual region (physical ไหนก็ได้) ----
        uint32_t pages = (size + sizeof(Header) + PAGE_SIZE - 1) / PAGE_SIZE;
        if (pages < 2) pages = 2;
        block = heap_grow(pages);
        if (!block) return nullptr;
    }

    split_block(block, size);
    block->is_free = 0;
    return (void*)((uint8_t*)block + sizeof(Header));
}

extern "C" void kfree(void* ptr) {
    if (!ptr) return;

    Header* header = (Header*)((uint8_t*)ptr - sizeof(Header));
    if (header->magic != HEAP_MAGIC) return; // ไม่ใช่ block ของเรา ห้าม free

    header->is_free = 1;

    // ---- Coalesce: รวมเฉพาะ block ว่างที่ติดกันจริงใน virtual memory ----
    Header* curr = heap_start;
    while (curr && curr->next) {
        if (curr->is_free && curr->next->is_free) {
            uint8_t* curr_end = (uint8_t*)curr + sizeof(Header) + curr->size;
            if (curr_end == (uint8_t*)curr->next) {
                curr->size += sizeof(Header) + curr->next->size;
                curr->next = curr->next->next;
            } else {
                curr = curr->next;
            }
        } else {
            curr = curr->next;
        }
    }
}
