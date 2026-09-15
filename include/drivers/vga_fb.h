#ifndef VGA_FB_H
#define VGA_FB_H

#include <stdint.h>

// ---- MeowOS GUI foundation: Bochs VBE/dispi graphics + LFB framebuffer ----
// ทุก drawing ลง backbuffer (DRAM ปกติ) แล้วค่อย fb_present() ทีเดียวทั้งเฟรม
// = direct rendering แบบ double-buffered ตั้งแต่ยังไม่มี GPU

#define FB_WIDTH   1024
#define FB_HEIGHT  768
#define FB_BPP     16
#define FB_PITCH   (FB_WIDTH * 2)          // bytes ต่อแถว
#define FB_LFB_VIRT 0xE0000000UL           // LFB ที่ map ไว้ใน kernel space

// สี 16bpp RGB565
#define RGB565(r, g, b) \
    ((uint16_t)((((r) & 0x1F) << 11) | (((g) & 0x3F) << 5) | ((b) & 0x1F)))

// สีพื้นฐาน (r8 g8 b8 → ย่อเป็น 565)
#define FB_BLACK     RGB565(0, 0, 0)
#define FB_WHITE     RGB565(31, 63, 31)
#define FB_RED       RGB565(31, 0, 0)
#define FB_GREEN     RGB565(0, 31, 0)
#define FB_BLUE      RGB565(0, 0, 31)
#define FB_YELLOW    RGB565(31, 63, 0)
#define FB_CYAN      RGB565(0, 31, 31)
#define FB_MAGENTA   RGB565(31, 0, 31)
#define FB_GRAY      RGB565(16, 32, 16)
#define FB_DARKGRAY  RGB565(8, 16, 8)

extern "C" {

// เข้าสู่ graphics mode (lazy init: probe hardware + ตี font ครั้งแรกที่เรียก)
// คืน false ถ้า hardware ไม่ตอบสนอง; หลังจากนี้ vga text output จะไม่ขึ้นจอ (serial เท่านั้น)
bool fb_enter_graphics();

// probe + font capture (เรียกอัตโนมัติแล้วใน fb_enter_graphics)
bool fb_init();

// กลับสู่ text mode 80x25 (shell ใช้ต่อ)
void fb_text_mode();

// ---- Drawing primitives (render ลง backbuffer ทั้งหมด, clip ให้แล้ว) ----
void fb_fill(uint16_t color);                                   // ทั้งจอ
void fb_pixel(int x, int y, uint16_t color);
void fb_hline(int x1, int x2, int y, uint16_t color);
void fb_vline(int x, int y1, int y2, uint16_t color);
void fb_line(int x1, int y1, int x2, int y2, uint16_t color);   // Bresenham
void fb_rect(int x, int y, int w, int h, uint16_t color);       // กรอบ
void fb_fill_rect(int x, int y, int w, int h, uint16_t color);  // ทึบ
void fb_circle(int cx, int cy, int r, uint16_t color);          // กรอบวงกลม
void fb_fill_circle(int cx, int cy, int r, uint16_t color);

// วาดตัวอักษร/ข้อความด้วย font 8x16 (จาก VGA char ROM)
void fb_char(int x, int y, char c, uint16_t fg, uint16_t bg);
void fb_text(int x, int y, const char* s, uint16_t fg, uint16_t bg);

// ก๊อปปี้ภาพ (src → จอ) สำหรับ window/bitmap ในอนาคต; ทึบเท่านั้น
void fb_blit(const uint16_t* src, int sx, int sw, int sh,
             int dx, int dy);

// เอา backbuffer ทั้งเฟรมขึ้นจอ (memcpy LFB)
void fb_present();

// สถานะ
bool fb_available();          // graphics mode ทำงานอยู่ไหม
uint32_t fb_tick();           // frame counter — ใช้วัดว่า present ทำงานจริง

// demo frame: layout คงที่ (title bar, rect, circle, line) ไว้ให้ fb_test.py ตรวจ pixel
void fb_draw_demo();

} // extern "C"

#endif
