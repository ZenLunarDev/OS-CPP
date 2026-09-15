#include <stdint.h>
#include "isr.h"
#include "paging.h"
#include "io.h"

extern "C" void vga_puts(const char* str);
extern "C" void vga_putchar(char c);

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

// ---- Exception Stubs: ตาราง 32 stub จาก interrupts.asm ----
extern "C" const uint32_t isr_stub_table[32];

// vectors ที่ CPU push error code มาให้เอง (ต้องใช้ ISR_ERR stub)
static const bool has_error_code[32] = {
    false, false, false, false, false, false, false, false,  // 0-7
    true,  false, true,  true,  true,  true,  true,  false,  // 8-15
    false, false, true,  false, false, false, false, false,  // 16-23
    false, false, false, false, false, false, false, false   // 24-31
};

// ---- IRQ / Syscall Stubs ----
extern "C" void irq0_stub();
extern "C" void irq1_stub();
extern "C" void syscall_stub();

// ---- C Handlers ----
extern "C" void irq0_c_handler();                  // timer.cpp
extern "C" void irq1_c_handler(uint32_t scancode); // shell_engine.cpp
extern "C" uint32_t syscall_handler(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t); // syscall.cpp
extern "C" void kill_user_process();               // shell_engine.cpp — กลับ shell หลัง user พัง

static const char* exception_messages[] = {
    "Division By Zero", "Debug", "Non Maskable Interrupt", "Breakpoint",
    "Into Detected Overflow", "Out of Bounds", "Invalid Opcode", "No Coprocessor",
    "Double Fault", "Coprocessor Segment Overrun", "Bad TSS", "Segment Not Present",
    "Stack Fault", "General Protection Fault", "Page Fault", "Unknown Interrupt",
    "Coprocessor Fault", "Alignment Check", "Machine Check"
};

static void print_hex(uint32_t val) {
    char hex[] = "0123456789ABCDEF";
    vga_puts("0x");
    for (int i = 7; i >= 0; i--) {
        vga_putchar(hex[(val >> (i * 4)) & 0xF]);
    }
}

// ตัวจัดการ Exception กลาง (stub ส่ง Registers* มาให้)
extern "C" void isr_dispatch(Registers* regs) {
    bool from_user = (regs->cs & 3) == 3;

    vga_puts("\n\n=== KERNEL PANIC: EXCEPTION DETECTED ===\n");

    if (regs->int_no < 32) {
        vga_puts("Exception: ");
        vga_puts(exception_messages[regs->int_no]);
        vga_puts("\n");
    }

    if (regs->int_no == 14) {
        uint32_t fault_addr;
        asm volatile("mov %%cr2, %0" : "=r"(fault_addr));
        vga_puts("Faulting Address: ");      // print_hex ใส่ 0x ให้แล้ว
        print_hex(fault_addr);
        vga_puts("\nError Code: ");
        print_hex(regs->err_code);
        vga_puts(regs->err_code & 0x1 ? " [Protection]" : " [Not Present]");
        vga_puts(regs->err_code & 0x2 ? " [Write]" : " [Read]");
        vga_puts(regs->err_code & 0x4 ? " [User]" : " [Kernel]");
        vga_puts("\n");
    }

    vga_puts("EIP: ");       print_hex(regs->eip);
    vga_puts(" | CS: ");     print_hex(regs->cs);
    vga_puts(" | EFLAGS: "); print_hex(regs->eflags);
    vga_puts("\n");

    // fault มาจาก Ring 3 = โปรแกรม user พัง ไม่ใช่ kernel — ฆ่า process แล้วกลับ shell
    if (from_user) {
        vga_puts("[User process terminated.]");
        kill_user_process();   // ไม่มีทางกลับจากฟังก์ชันนี้
    }

    vga_puts("System halted to prevent damage.");
    while (1) { asm volatile("cli; hlt"); }
}

static void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low      = (base & 0xFFFF);
    idt[num].offset_high     = (base >> 16) & 0xFFFF;
    idt[num].selector        = sel;
    idt[num].zero            = 0;
    idt[num].type_attributes = flags;
}

static void pic_remap() {
    outb(0x20, 0x11); outb(0xA0, 0x11);   // ICW1: init + ICW4
    outb(0x21, 0x20); outb(0xA1, 0x28);   // ICW2: offsets 0x20 / 0x28
    outb(0x21, 0x04); outb(0xA1, 0x02);   // ICW3: cascade
    outb(0x21, 0x01); outb(0xA1, 0x01);   // ICW4: 8086 mode

    outb(0x21, 0xFC); // Unmask IRQ0 (timer) + IRQ1 (keyboard)
    outb(0xA1, 0xFF); // Mask ทุกอย่างบน slave
}

extern "C" void init_interrupts() {
    idtp.limit = sizeof(IDTEntry) * 256 - 1;
    idtp.base  = reinterpret_cast<uint32_t>(&idt);

    for (int i = 0; i < 32; i++) {
        (void)has_error_code[i]; // stub จัดการ err_code ให้แล้ว — เก็บไว้เป็นเอกสาร
        idt_set_gate(i, isr_stub_table[i], 0x08, 0x8E); // ทุก exception มี gate ตัวเอง
    }
    for (int i = 32; i < 256; i++) {
        idt_set_gate(i, isr_stub_table[0], 0x08, 0x8E); // software ints ที่ยังไม่ใช้ → isr0 ปลอดภัย
    }

    pic_remap();

    // Hardware IRQs (DPL = 0)
    idt_set_gate(0x20, reinterpret_cast<uint32_t>(irq0_stub), 0x08, 0x8E);
    idt_set_gate(0x21, reinterpret_cast<uint32_t>(irq1_stub), 0x08, 0x8E);

    // Syscall INT 0x80 (DPL = 3 -> 0xEE เพื่อให้ Ring 3 เรียกได้)
    idt_set_gate(0x80, reinterpret_cast<uint32_t>(syscall_stub), 0x08, 0xEE);

    asm volatile("lidt %0" :: "m"(idtp));
}
