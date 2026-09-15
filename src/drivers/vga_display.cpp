#include <stdint.h>
#include <stddef.h>

static uint16_t* const VGA_MEMORY = (uint16_t*)0xB8000;
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

static size_t terminal_row = 0;
static size_t terminal_column = 0;
static uint8_t terminal_color = 0x0F; // White text on Black background

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | (uint16_t) color << 8;
}

// ---- Serial Console (COM1) — ใช้ debug ผ่าน QEMU (-serial) ได้ ----
static bool serial_ready = false;

static inline void serial_outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t serial_inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void serial_init_once() {
    serial_outb(0x3F8 + 1, 0x00); // ปิด interrupt ของ serial
    serial_outb(0x3F8 + 3, 0x80); // เปิด DLAB
    serial_outb(0x3F8 + 0, 0x03); // divisor low: 38400 baud
    serial_outb(0x3F8 + 1, 0x00); // divisor high
    serial_outb(0x3F8 + 3, 0x03); // 8 bits, no parity, 1 stop
    serial_outb(0x3F8 + 2, 0xC7); // FIFO on, clear
    serial_outb(0x3F8 + 4, 0x0B); // RTS/DSR
    serial_ready = true;
}

static void serial_putchar(char c) {
    if (!serial_ready) serial_init_once();
    while (!(serial_inb(0x3F8 + 5) & 0x20)); // รอ THR ว่าง
    serial_outb(0x3F8, (uint8_t)c);
}

static void vga_scroll() {
    // เลื่อนทุกบรรทัดขึ้น 1 แถว (memmove ทีละ 2 bytes เพราะ char + attribute)
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[(y - 1) * VGA_WIDTH + x] = VGA_MEMORY[y * VGA_WIDTH + x];
        }
    }
    // เคลียร์บรรทัดสุดท้าย
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
    }
    terminal_row = VGA_HEIGHT - 1;
}

extern "C" {
void vga_putchar(char c) {
    serial_putchar(c); // mirror ทุกตัวอักษรไป serial console ด้วย

    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) vga_scroll();
        return;
    }

    if (c == '\b') { // Backspace
        if (terminal_column > 0) {
            terminal_column--;
        } else if (terminal_row > 0) {
            terminal_row--;
            terminal_column = VGA_WIDTH - 1;
        }
        const size_t index = terminal_row * VGA_WIDTH + terminal_column;
        VGA_MEMORY[index] = vga_entry(' ', terminal_color);
        return;
    }

    const size_t index = terminal_row * VGA_WIDTH + terminal_column;
    VGA_MEMORY[index] = vga_entry(c, terminal_color);

    if (++terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) vga_scroll();
    }
}

void vga_clear() {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            VGA_MEMORY[index] = vga_entry(' ', terminal_color);
        }
    }
    terminal_row = 0;
    terminal_column = 0;
}

void vga_puts(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        vga_putchar(str[i]);
    }
}

void kernel_print_char(char c) {
    vga_putchar(c);
}
}