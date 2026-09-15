#include "timer.h"
#include "task.h"
#include "io.h"

static uint32_t ticks = 0;

extern "C" void init_timer(uint32_t frequency) {
    uint32_t divisor = 1193180 / frequency;

    outb(0x43, 0x36); // Command byte: Channel 0, LSB/MSB, Square Wave
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

// IRQ0: stub ส่ง Registers* มาเป็นตัวแรก — ไม่ใช้ แต่รับไว้ให้ stack ตรง
extern "C" void irq0_c_handler(void* regs) {
    (void)regs;

    ticks++;
    outb(0x20, 0x20); // EOI ก่อน schedule เสมอ (กัน IRQ ค้างระหว่าง context switch)
    schedule();
}

extern "C" uint32_t get_ticks() {
    return ticks;
}