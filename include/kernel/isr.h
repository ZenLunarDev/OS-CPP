#ifndef ISR_H
#define ISR_H

#include <stdint.h>

// Layout นี้ต้องตรงกับ frame ที่ stub asm push ให้ครบทุก field
// (ดู ISR_NOERR/ISR_ERR macros ใน interrupts.asm)
struct Registers {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags;
    // fault จาก Ring 3: CPU push useresp+ss ไว้ใต้ eip บน stack — stub ไม่ copy มาไว้ที่นี่
    // (ตอนนี้ dispatch ใช้ err_code + CR2 พอ)
};

extern "C" {
void isr_dispatch(Registers* regs);
}

#endif
