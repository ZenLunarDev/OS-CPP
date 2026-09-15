#ifndef KEYBOARD_RINGBUFFER_H
#define KEYBOARD_RINGBUFFER_H

#include <stdint.h>
#include <stdbool.h>

#define KBD_BUFFER_SIZE 256

typedef struct {
    uint8_t buffer[KBD_BUFFER_SIZE];
    uint16_t head;
    uint16_t tail;
} RingBuffer;

static RingBuffer kbd_rb = { {0}, 0, 0 };

static inline bool ringbuffer_push(uint8_t data) {
    uint16_t next = (kbd_rb.head + 1) % KBD_BUFFER_SIZE;
    if (next == kbd_rb.tail) return false; // Buffer Full
    kbd_rb.buffer[kbd_rb.head] = data;
    kbd_rb.head = next;
    return true;
}

static inline bool ringbuffer_pop(uint8_t *data) {
    if (kbd_rb.head == kbd_rb.tail) return false; // Buffer Empty
    *data = kbd_rb.buffer[kbd_rb.tail];
    kbd_rb.tail = (kbd_rb.tail + 1) % KBD_BUFFER_SIZE;
    return true;
}

#endif