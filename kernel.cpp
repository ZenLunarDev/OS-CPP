#include <stdint.h>
#include <stddef.h>
#include "gdt.h"
#include "idt.h"

constexpr uintptr_t VGA_ADDRESS = 0xB8000;
constexpr size_t VGA_WIDTH = 80;
constexpr size_t VGA_HEIGHT = 25;

class VgaTerminal {
private:
    size_t row{0};
    size_t column{0};
    uint8_t color{0x0F}; // White text on Black
    uint16_t* buffer{(uint16_t*)VGA_ADDRESS};

public:
    void clear() {
        for (size_t y = 0; y < VGA_HEIGHT; ++y) {
            for (size_t x = 0; x < VGA_WIDTH; ++x) {
                buffer[y * VGA_WIDTH + x] = ' ' | (color << 8);
            }
        }
    }

    void write_char(char c) {
        if (c == '\n') {
            column = 0;
            if (++row == VGA_HEIGHT) row = 0;
            return;
        }
        if (c == '\b') {
            if (column > 0) {
                column--;
                buffer[row * VGA_WIDTH + column] = ' ' | (color << 8);
            }
            return;
        }

        buffer[row * VGA_WIDTH + column] = c | (color << 8);
        if (++column == VGA_WIDTH) {
            column = 0;
            if (++row == VGA_HEIGHT) row = 0;
        }
    }

    void write_string(const char* data) {
        for (size_t i = 0; data[i] != '\0'; ++i) {
            write_char(data[i]);
        }
    }
};

static VgaTerminal terminal;

// Export ให้ keyboard.cpp เรียกใช้
extern "C" void kernel_print_char(char c) {
    terminal.write_char(c);
}

extern "C" void kernel_main() {
    terminal.clear();
    terminal.write_string("MeowOS Keyboard Driver Loaded!\n");
    terminal.write_string("Try typing something: ");

    GDT::init();
    IDT::init();

    // Loop รอ Interrupt
    while (1) {
        asm volatile("hlt");
    }
}