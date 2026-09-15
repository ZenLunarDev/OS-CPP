#include "task.h"
#include "heap.h"

static Task* current_task = nullptr;
static Task* main_task = nullptr;
static uint32_t next_pid = 1;
static bool multitasking_enabled = false;
static bool scheduler_paused = false;

// หยุด/เปิด preemption ชั่วคราว (ใช้ตอนรัน user program — kernel stack เป็นของ syscall)
extern "C" void scheduler_pause(bool pause) {
    scheduler_paused = pause;
}

extern "C" void vga_puts(const char* str);

void create_task(void (*entry_point)()) {
    Task* new_task = (Task*)kmalloc(sizeof(Task));
    new_task->id = next_pid++;

    // จอง Kernel Stack ขนาด 4KB
    new_task->stack_base = (uint8_t*)kmalloc(4096);
    uint32_t* esp = (uint32_t*)(new_task->stack_base + 4096);

    // เซ็ตアップ Initial Stack Frame ให้ตรงกับ asm switch_to_task
    *(--esp) = (uint32_t)entry_point; // EIP
    *(--esp) = 0x0202;                // EFLAGS (Enable Interrupts)
    *(--esp) = 0;                     // EAX
    *(--esp) = 0;                     // ECX
    *(--esp) = 0;                     // EDX
    *(--esp) = 0;                     // EBX
    *(--esp) = 0;                     // EBP
    *(--esp) = 0;                     // ESI
    *(--esp) = 0;                     // EDI

    new_task->esp = (uint32_t)esp;

    // ต่อ Node เข้า Round-Robin Linked List
    if (!main_task->next) {
        main_task->next = new_task;
        new_task->next = main_task;
    } else {
        Task* temp = main_task;
        while (temp->next != main_task) temp = temp->next;
        temp->next = new_task;
        new_task->next = main_task;
    }
}

void init_multitasking() {
    main_task = (Task*)kmalloc(sizeof(Task));
    main_task->id = 0;
    main_task->esp = 0;
    main_task->stack_base = nullptr;
    main_task->next = main_task;

    current_task = main_task;
    multitasking_enabled = true;
}

void schedule() {
    if (scheduler_paused) return;
    if (!multitasking_enabled || !current_task || current_task->next == current_task) return;

    Task* old_task = current_task;
    current_task = current_task->next;

    switch_to_task(&old_task->esp, current_task->esp);
}