#include <stdint.h>

// IDT Entry Structure
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

extern "C" void irq1_stub();

// Remap Master/Slave PIC
static void pic_remap() {
    // ICW1: Init Sequence
    asm volatile("outb %0, %1" :: "a"((uint8_t)0x11), "Nd"((uint16_t)0x20));
    asm volatile("outb %0, %1" :: "a"((uint8_t)0x11), "Nd"((uint16_t)0xA0));

    // ICW2: Vector Offset Mapping (0x20 for Master, 0x28 for Slave)
    asm volatile("outb %0, %1" :: "a"((uint8_t)0x20), "Nd"((uint16_t)0x21));
    asm volatile("outb %0, %1" :: "a"((uint8_t)0x28), "Nd"((uint16_t)0xA1));

    // ICW3: Cascading setup
    asm volatile("outb %0, %1" :: "a"((uint8_t)0x04), "Nd"((uint16_t)0x21));
    asm volatile("outb %0, %1" :: "a"((uint8_t)0x02), "Nd"((uint16_t)0xA1));

    // ICW4: Environment Mode (8086)
    asm volatile("outb %0, %1" :: "a"((uint8_t)0x01), "Nd"((uint16_t)0x21));
    asm volatile("outb %0, %1" :: "a"((uint8_t)0x01), "Nd"((uint16_t)0xA1));

    // Unmask IRQ1 (Keyboard) on Master PIC, mask all others
    asm volatile("outb %0, %1" :: "a"((uint8_t)0xFD), "Nd"((uint16_t)0x21));
    asm volatile("outb %0, %1" :: "a"((uint8_t)0xFF), "Nd"((uint16_t)0xA1));
}

// Gate Registration Helper
static void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low      = (base & 0xFFFF);
    idt[num].offset_high     = (base >> 16) & 0xFFFF;
    idt[num].selector        = sel;
    idt[num].zero            = 0;
    idt[num].type_attributes = flags;
}

// Primary Initialization Routine (Single Entry with extern "C")
extern "C" void init_interrupts() {
    idtp.limit = sizeof(IDTEntry) * 256 - 1;
    idtp.base  = reinterpret_cast<uint32_t>(&idt);

    pic_remap();

    // 0x21 = IRQ1 (Master PIC Offset 0x20 + 1)
    // Attribute 0x8E = Present, Ring 0, 32-bit Interrupt Gate
    idt_set_gate(0x21, reinterpret_cast<uint32_t>(irq1_stub), 0x08, 0x8E);

    // Load IDTR Instruction
    asm volatile("lidt %0" :: "m"(idtp));

    // Enable Hardware Interrupts
    asm volatile("sti");
}