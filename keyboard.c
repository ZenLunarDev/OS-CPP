#include <stdint.h>

#define PIC1_COMMAND 0x20
#define PIC_EOI      0x20
#define KEYBOARD_PORT 0x60

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__ ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__ ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void keyboard_handler_main(void) {
    // 1. Clear Hardware Buffer (ต้องอ่าน ไม่งั้น IRQ จะค้าง)
    uint8_t scancode = inb(KEYBOARD_PORT);
    (void)scancode; // นำ scancode ไปใช้งานต่อที่นี่

    // 2. Send EOI to PIC Master
    outb(PIC1_COMMAND, PIC_EOI);
}