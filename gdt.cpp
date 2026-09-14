#include "gdt.h"

struct GDTEntry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct GDTPtr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static GDTEntry gdt[6];
static GDTPtr gdt_ptr;
static TSSEntry tss_entry;

extern "C" void gdt_flush(uint32_t);
extern "C" void tss_flush();

static void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low    = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;
    gdt[num].limit_low   = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;
    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access      = access;
}

extern "C" void set_kernel_stack(uint32_t stack) {
    tss_entry.esp0 = stack;
}

extern "C" void init_gdt() {
    gdt_ptr.limit = (sizeof(GDTEntry) * 6) - 1;
    gdt_ptr.base  = reinterpret_cast<uint32_t>(&gdt);

    gdt_set_gate(0, 0, 0, 0, 0);                // Null segment
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // Kernel Code (0x08)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Kernel Data (0x10)
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // User Code (0x1B)
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // User Data (0x23)

    // TSS Entry Setup
    uint32_t base = reinterpret_cast<uint32_t>(&tss_entry);
    uint32_t limit = sizeof(tss_entry);
    gdt_set_gate(5, base, limit, 0xE9, 0x00);

    tss_entry.ss0 = 0x10;
    tss_entry.esp0 = 0x0;

    gdt_flush(reinterpret_cast<uint32_t>(&gdt_ptr));
    tss_flush();
}