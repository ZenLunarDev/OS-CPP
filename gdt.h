// gdt.h
#ifndef GDT_H
#define GDT_H

#include <stdint.h>

struct TSSEntry {
    uint32_t prev_tss, esp0, ss0, esp1, ss1, esp2, ss2;
    uint32_t cr3, eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs, ldt, trap, iomap_base;
} __attribute__((packed));

extern "C" {
void init_gdt();
void set_kernel_stack(uint32_t stack);
}

#endif