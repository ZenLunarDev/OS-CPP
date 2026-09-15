#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

extern "C" {
void init_timer(uint32_t frequency);
void timer_handler(); // mapped กับ irq0_stub
uint32_t get_ticks();
}

#endif