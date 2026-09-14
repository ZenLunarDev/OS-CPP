#include <stdint.h>

struct IDTEntry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attributes;
    uint16_t offset_high;
} __attribute__((packed));

struct IDTPtr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static IDTEntry idt[256];
static IDTPtr idtp;

extern "C" void isr0();
extern "C" void isr6();
extern "C" void isr13();
extern "C" void isr14();

extern "C" void irq0_stub();
extern "C" void irq1_stub();
extern "C" void syscall_stub();

static void pic_remap() {
    asm volatile("outb %0, %1" :: "a"((uint8_t)0x11), "Nd"((uint16_t)0x20));
    asm volatile("outb %0, %1" :: "a"((uint8_t)0x11), "Nd"((uint16_t)0xA0));

    asm volatile("outb %0, %1" :: "a"((uint8_t)0x20), "Nd"((uint16_t)0x21));
    asm volatile("outb %0, %1" :: "a"((uint8_t)0x28), "Nd"((uint16_t)0xA1));

    asm volatile("outb %0, %1" :: "a"((uint8_t)0x04), "Nd"((uint16_t)0x21));
    asm volatile("outb %0, %1" :: "a"((uint8_t)0x02), "Nd"((uint16_t)0xA1));

    asm volatile("outb %0, %1" :: "a"((uint8_t)0x01), "Nd"((uint16_t)0x21));
    asm volatile("outb %0, %1" :: "a"((uint8_t)0x01), "Nd"((uint16_t)0xA1));

    asm volatile("outb %0, %1" :: "a"((uint8_t)0xFC), "Nd"((uint16_t)0x21));
    asm volatile("outb %0, %1" :: "a"((uint8_t)0xFF), "Nd"((uint16_t)0xA1));
}

static void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low      = (base & 0xFFFF);
    idt[num].offset_high     = (base >> 16) & 0xFFFF;
    idt[num].selector        = sel;
    idt[num].zero            = 0;
    idt[num].type_attributes = flags;
}

extern "C" void init_interrupts() {
    idtp.limit = sizeof(IDTEntry) * 256 - 1;
    idtp.base  = reinterpret_cast<uint32_t>(&idt);

    pic_remap();

    // CPU Exceptions (DPL = 0)
    idt_set_gate(0,  reinterpret_cast<uint32_t>(isr0),  0x08, 0x8E);
    idt_set_gate(6,  reinterpret_cast<uint32_t>(isr6),  0x08, 0x8E);
    idt_set_gate(13, reinterpret_cast<uint32_t>(isr13), 0x08, 0x8E);
    idt_set_gate(14, reinterpret_cast<uint32_t>(isr14), 0x08, 0x8E);

    // Hardware IRQs (DPL = 0)
    idt_set_gate(0x20, reinterpret_cast<uint32_t>(irq0_stub), 0x08, 0x8E);
    idt_set_gate(0x21, reinterpret_cast<uint32_t>(irq1_stub), 0x08, 0x8E);

    // Syscall INT 0x80 (DPL = 3 -> 0xEE)
    idt_set_gate(0x80, reinterpret_cast<uint32_t>(syscall_stub), 0x08, 0xEE);

    asm volatile("lidt %0" :: "m"(idtp));
    asm volatile("sti");
}