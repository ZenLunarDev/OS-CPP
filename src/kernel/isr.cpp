#include "isr.h"

extern "C" void vga_puts(const char* str);

// isr_dispatch อยู่ที่ interrupts.cpp — ไฟล์นี้เหลือไว้เพื่อความเข้ากันได้
// กับ extern ที่อ้าง isr_handler จากที่อื่น (stub asm เรียก isr_dispatch โดยตรง)
