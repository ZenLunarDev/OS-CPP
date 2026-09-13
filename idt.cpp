#include "idt.h"
#include "io.h"

static IdtEntry idt_entries[256];
static IdtPtr   idt_ptr;

extern "C" void idt_load_asm(uint32_t idt_ptr_addr);
extern "C" void default_isr_handler();
extern "C" void irq1_stub();

void IDT::set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt_entries[num].base_low  = base & 0xFFFF;
    idt_entries[num].base_high = (base >> 16) & 0xFFFF;
    idt_entries[num].sel       = sel;
    idt_entries[num].always0   = 0;
    idt_entries[num].flags     = flags;
}

void IDT::init() {
    idt_ptr.limit = sizeof(IdtEntry) * 256 - 1;
    idt_ptr.base  = (uint32_t)&idt_entries;

    for (int i = 0; i < 256; i++) {
        set_gate(i, (uint32_t)default_isr_handler, 0x08, 0x8E);
    }

    // Remap PIC 8259
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    // Unmask IRQ1 (Keyboard Only)
    outb(0x21, 0xFD);
    outb(0xA1, 0xFF);

    set_gate(33, (uint32_t)irq1_stub, 0x08, 0x8E);

    idt_load_asm((uint32_t)&idt_ptr);
    asm volatile("sti");
}