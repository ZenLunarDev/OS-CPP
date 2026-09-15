#ifndef TASK_H
#define TASK_H

#include <stdint.h>

struct Task {
    uint32_t id;
    uint32_t esp;            // Stack Pointer ของ Task
    uint8_t* stack_base;     // Base Pointer สำหรับ free memory
    struct Task* next;
};

extern "C" {
void init_multitasking();
void create_task(void (*entry_point)());
void schedule();
void scheduler_pause(bool pause);
void switch_to_task(uint32_t* old_esp, uint32_t new_esp);
}

#endif