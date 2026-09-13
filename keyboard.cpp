#include "keyboard.h"
#include "io.h"

static const char scancode_ascii[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
     0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
     0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
   '*',   0, ' '
};

// บังคับใช้ C Linkage ให้ตรงกับ kernel.cpp
extern "C" void kernel_print_char(char c);

void Keyboard::handle_interrupt() {
    uint8_t scancode = inb(0x60);

    if (!(scancode & 0x80)) {
        if (scancode < sizeof(scancode_ascii)) {
            char c = scancode_ascii[scancode];
            if (c) {
                kernel_print_char(c);
            }
        }
    }

    outb(0x20, 0x20);
}

extern "C" void irq1_handler() {
    Keyboard::handle_interrupt();
}