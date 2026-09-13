#pragma once
#include <stdint.h>

class Keyboard {
public:
    static void init();
    static void handle_interrupt();
private:
    static char scancode_to_ascii(uint8_t scancode);
};