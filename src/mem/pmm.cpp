#include "pmm.h"

// รองรับ Memory สูงสุด 128MB (32,768 Pages)
#define MAX_BLOCKS 32768
#define MAX_USABLE_ENTRIES 32

static uint32_t pmm_bitmap[MAX_BLOCKS / 32];
static uint32_t total_blocks = 0;
static uint32_t used_blocks = 0;
static bool initialized = false;

struct MemRegion {
    uint32_t start; // physical (page aligned)
    uint32_t end;   // physical (exclusive)
    uint32_t first_block_index; // index แรกของ region นี้ใน bitmap
};

static MemRegion usable[MAX_USABLE_ENTRIES];
static uint32_t usable_count = 0;

static inline void bitmap_set(uint32_t bit) {
    pmm_bitmap[bit / 32] |= (1 << (bit % 32));
}

static inline void bitmap_unset(uint32_t bit) {
    pmm_bitmap[bit / 32] &= ~(1 << (bit % 32));
}

static inline bool bitmap_test(uint32_t bit) {
    return pmm_bitmap[bit / 32] & (1 << (bit % 32));
}

extern "C" void vga_puts(const char* str);
extern "C" void vga_putchar(char c);

// เพิ่ม region ที่ใช้ได้ (reserved โดยเคอร์เนล/บิวต์อิน)
static void add_region(uint32_t start, uint32_t end) {
    // จัด Page Align
    start = (start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    end &= ~(PAGE_SIZE - 1);
    if (start >= end) return;

    // ห้ามใช้ memory ต่ำกว่า 1MB (VGA/BIOS area)
    if (end <= 0x100000) return;
    if (start < 0x100000) start = 0x100000;

    // ทับซ้อนกับ region เดิม? ข้าม
    for (uint32_t i = 0; i < usable_count; i++) {
        if (start < usable[i].end && end > usable[i].start) return;
    }

    if (usable_count >= MAX_USABLE_ENTRIES) return;
    usable[usable_count].start = start;
    usable[usable_count].end = end;
    usable_count++;
}

// สำรอง memory ที่ kernel code/data ครอบอยู่ (ระบุจาก linker symbols)
static void reserve_kernel_image() {
    extern uint8_t _kernel_start[], _kernel_end[];
    uint32_t ks = reinterpret_cast<uint32_t>(_kernel_start) & ~(PAGE_SIZE - 1);
    uint32_t ke = (reinterpret_cast<uint32_t>(_kernel_end) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    for (uint32_t i = 0; i < usable_count; i++) {
        if (ks < usable[i].end && ke > usable[i].start) {
            // แตก region เป็นสองท่อน: หน้า kernel / หลัง kernel
            uint32_t old_end = usable[i].end;
            if (usable[i].start < ks) {
                usable[i].end = ks;
                if (ke < old_end && usable_count < MAX_USABLE_ENTRIES) {
                    usable[usable_count].start = ke;
                    usable[usable_count].end = old_end;
                    usable_count++;
                }
            } else {
                usable[i].start = ke;
            }
        }
    }
}

extern "C" void pmm_init(uint32_t mem_upper_kb) {
    // Default: ใช้ mem_upper_kb จาก Multiboot (memory ตั้งแต่ 1MB ขึ้นไป)
    add_region(0x100000, 0x100000 + (mem_upper_kb * 1024));
}

// จาก Multiboot memory map (flags bit 6) — แม่นยำกว่า mem_upper เดี่ยวๆ
extern "C" void pmm_init_from_mmap(uint32_t mmap_addr, uint32_t mmap_length) {
    struct mmap_entry {
        uint32_t size;      // ขนาด entry (ไม่รวมตัวแปร size เอง)
        uint64_t base_addr;
        uint64_t length;
        uint32_t type;      // 1 = usable RAM
    } __attribute__((packed));

    uint32_t offset = 0;
    while (offset + sizeof(uint32_t) <= mmap_length) {
        mmap_entry* ent = reinterpret_cast<mmap_entry*>(mmap_addr + offset);
        if (ent->type == 1 && ent->base_addr < 0x100000000ULL) {
            uint64_t end = ent->base_addr + ent->length;
            if (end > 0xFFFFFFFFULL) end = 0xFFFFFFFFULL;
            add_region(static_cast<uint32_t>(ent->base_addr), static_cast<uint32_t>(end));
        }
        offset += ent->size + 4;
    }
}

extern "C" void pmm_finalize() {
    reserve_kernel_image();

    // Bitmap เก็บที่ address เท่าไร? (ใช้ static BSS = อยู่ใน kernel image แล้ว)
    total_blocks = 0;
    used_blocks = 0;

    for (uint32_t i = 0; i < usable_count; i++) {
        uint32_t blocks = (usable[i].end - usable[i].start) / PAGE_SIZE;
        usable[i].first_block_index = total_blocks;
        total_blocks += blocks;
        if (total_blocks > MAX_BLOCKS) total_blocks = MAX_BLOCKS;
    }

    for (uint32_t i = 0; i < (MAX_BLOCKS / 32); i++) pmm_bitmap[i] = 0;

    // Reserve ทุก block ที่เกิน region ที่ใช้ได้
    for (uint32_t b = total_blocks; b < MAX_BLOCKS; b++) bitmap_set(b);

    initialized = true;

#ifdef PMM_DEBUG
    vga_puts("\n[PMM] usable regions: ");
    {
        // print usable_count + total_blocks แบบ hex
        uint32_t vals[2] = { usable_count, total_blocks };
        const char* labels[2] = { " count=", " blocks=" };
        for (int k = 0; k < 2; k++) {
            vga_puts(labels[k]);
            for (int i = 7; i >= 0; i--) vga_putchar("0123456789ABCDEF"[(vals[k] >> (i*4)) & 0xF]);
        }
    }
#endif
}

static uint32_t block_addr(uint32_t idx) {
    for (uint32_t i = 0; i < usable_count; i++) {
        uint32_t count = (usable[i].end - usable[i].start) / PAGE_SIZE;
        if (idx < usable[i].first_block_index + count && idx >= usable[i].first_block_index) {
            return usable[i].start + (idx - usable[i].first_block_index) * PAGE_SIZE;
        }
        // ถ้าเกิน region นี้ ให้ไปหา region ต่อไป
    }
    return 0; // ควรไม่เกิด
}

extern "C" void* pmm_alloc_block() {
    if (!initialized || total_blocks == 0) return nullptr;

    for (uint32_t i = 0; i < total_blocks; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_blocks++;
            uint32_t addr = block_addr(i);
            return addr ? reinterpret_cast<void*>(addr) : nullptr;
        }
    }
    return nullptr;
}

// จองหลายหน้าติดกัน (สำหรับ multi-page heap และ user stack)
extern "C" void pmm_alloc_blocks(uint32_t count, void** out) {
    out[0] = nullptr;
    if (!initialized || count == 0 || count > total_blocks) return;

    for (uint32_t i = 0; i + count <= total_blocks; i++) {
        bool ok = true;
        for (uint32_t j = 0; j < count; j++) {
            if (bitmap_test(i + j)) { ok = false; i += j; break; }
        }
        if (!ok) continue;

        for (uint32_t j = 0; j < count; j++) {
            bitmap_set(i + j);
        }
        used_blocks += count;

        uint32_t addr = block_addr(i);
        if (!addr) {
            for (uint32_t j = 0; j < count; j++) bitmap_unset(i + j);
            used_blocks -= count;
            return;
        }
        out[0] = reinterpret_cast<void*>(addr);
        return;
    }
}

// Mark block ว่าเป็นของ kernel แล้ว (เช่น user region ที่ map แบบ manual)
extern "C" void pmm_mark_used(void* ptr) {
    if (!ptr || !initialized) return;
    uint32_t addr = reinterpret_cast<uint32_t>(ptr);

    for (uint32_t i = 0; i < usable_count; i++) {
        if (addr >= usable[i].start && addr < usable[i].end) {
            uint32_t block = usable[i].first_block_index + (addr - usable[i].start) / PAGE_SIZE;
            if (!bitmap_test(block)) {
                bitmap_set(block);
                used_blocks++;
            }
            return;
        }
    }
}

extern "C" void pmm_free_block(void* ptr) {
    if (!ptr) return;
    uint32_t addr = reinterpret_cast<uint32_t>(ptr);

    for (uint32_t i = 0; i < usable_count; i++) {
        if (addr >= usable[i].start && addr < usable[i].end) {
            uint32_t block = usable[i].first_block_index + (addr - usable[i].start) / PAGE_SIZE;
            if (bitmap_test(block)) {
                bitmap_unset(block);
                used_blocks--;
            }
            return;
        }
    }
}

extern "C" uint32_t pmm_get_free_block_count() {
    return total_blocks - used_blocks;
}
