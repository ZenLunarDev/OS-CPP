#include <stdint.h>
#include <stdbool.h>

#define KEYBOARD_PORT 0x60

// Scan Code Set 1 Table
static const char scancode_ascii_lowercase[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0,
  '*',   0, ' ',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0, '-',   0,   0,
    0, '+',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static const char scancode_ascii_uppercase[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
  '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',   0,
  '*',   0, ' ',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0, '-',   0,   0,
    0, '+',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static bool shift_pressed = false;

extern "C" void vga_putchar(char c); // Prototype สำหรับพิมพ์ออกจอ VGA

extern "C" void process_keyboard_scancode(uint8_t scancode) {
    // Break Code Check ( Bit 7 Set = Key Release )
    if (scancode & 0x80) {
        uint8_t released_code = scancode & 0x7F;
        if (released_code == 0x2A || released_code == 0x36) { // Left/Right Shift
            shift_pressed = false;
        }
        return;
    }

    // Make Code Check ( Key Press )
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = true;
        return;
    }

    char ascii = shift_pressed ? scancode_ascii_uppercase[scancode]
                               : scancode_ascii_lowercase[scancode];

    if (ascii != 0) {
        vga_putchar(ascii); // ส่งต่อไปพิมพ์ออกหน้าจอ
    }
}