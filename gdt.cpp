#include "gdt.h"

static GdtEntry gdt_entries[3];
static GdtPtr   gdt_ptr;

extern "C" void gdt_flush_asm(uint32_t gdt_ptr_addr);

void GDT::set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;

    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;
    gdt_entries[num].granularity |= gran & 0xF0;
    gdt_entries[num].access      = access;
}

void GDT::init() {
    gdt_ptr.limit = (sizeof(GdtEntry) * 3) - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;

    set_gate(0, 0, 0, 0, 0);                // Null segment
    set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // Kernel Code Segment (0x08)
    set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Kernel Data Segment (0x10)

    gdt_flush_asm((uint32_t)&gdt_ptr);
}