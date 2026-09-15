#include "vga_fb.h"
#include "paging.h"

extern "C" void vga_puts(const char* str);
extern "C" void* memcpy(void* dst, const void* src, unsigned int n);  // minilibc.cpp

// ---- Bochs VBE / dispi registers (QEMU std-VGA รองรับหมด) ----
#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA  0x01CF

#define VBE_DISPI_INDEX_ID        0x00
#define VBE_DISPI_INDEX_XRES      0x01
#define VBE_DISPI_INDEX_YRES      0x02
#define VBE_DISPI_INDEX_BPP       0x03
#define VBE_DISPI_INDEX_ENABLE    0x04
#define VBE_DISPI_INDEX_VIRT_WIDTH 0x06
#define VBE_DISPI_INDEX_VIRT_HEIGHT 0x07

#define VBE_DISPI_DISABLED        0x00
#define VBE_DISPI_ENABLED         0x01
#define VBE_DISPI_LFB_ENABLED     0x40

// ---- VGA register ports (สำหรับตี font จาก char ROM) ----
#define VGA_CRTC_INDEX   0x03D4
#define VGA_SEQ_INDEX    0x03C4
#define VGA_SEQ_DATA     0x03C5
#define VGA_GC_INDEX     0x03CE
#define VGA_GC_DATA      0x03CF
#define VGA_ROM_WINDOW   0x000A0000   // แผนที่ plane 2 ตอนอ่าน font

// ทุก helper ใช้ constraint "d" (port ผ่าน %dx) — พอร์ต VGA 0x3C4, dispi 0x1CE และ PCI 0xCF8
// ล้วนเกิน 255 จึงห้ามใช้ "N" (immediate 8-bit) เด็ดขาด: assembler จะว่า operand type mismatch
static inline void outb_p(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "d"(port));
}
static inline uint8_t inb_p(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "d"(port));
    return ret;
}
static inline void outw_p(uint16_t port, uint16_t val) {
    asm volatile ("outw %0, %1" : : "a"(val), "d"(port));
}
static inline uint16_t inw_p(uint16_t port) {
    uint16_t ret;
    asm volatile ("inw %1, %0" : "=a"(ret) : "d"(port));
    return ret;
}
static inline void outl_p(uint16_t port, uint32_t val) {
    asm volatile ("outl %0, %1" : : "a"(val), "d"(port));
}
static inline uint32_t inl_p(uint16_t port) {
    uint32_t ret;
    asm volatile ("inl %1, %0" : "=a"(ret) : "d"(port));
    return ret;
}

static void dispi_write(uint16_t index, uint16_t val) {
    outw_p(VBE_DISPI_IOPORT_INDEX, index);
    outw_p(VBE_DISPI_IOPORT_DATA, val);
}
static uint16_t dispi_read(uint16_t index) {
    outw_p(VBE_DISPI_IOPORT_INDEX, index);
    return inw_p(VBE_DISPI_IOPORT_DATA);
}

// ---- State ----
static uint16_t backbuf[FB_WIDTH * FB_HEIGHT];   // 1.5MB BSS — double buffer
static uint8_t font8x16[256 * 16];               // font ตัดจาก char ROM (32B/char → เก็บ 16)
static bool gfx_mode = false;
static bool driver_ready = false;
static uint32_t lfb_phys = 0;
static uint32_t frames_presented = 0;

// ---- PCI: หา physical address ของ framebuffer BAR (bus 0, dev 2, func 0 = VGA) ----
static uint32_t pci_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    uint32_t addr = 0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)dev << 11)
                  | ((uint32_t)func << 8) | (offset & 0xFC);
    outl_p(0xCF8, addr);
    return inl_p(0xCFC);
}

static uint32_t find_lfb_physical() {
    // QEMU std-VGA: 00:02.0, BAR0 = framebuffer
    uint32_t bar = pci_read32(0, 2, 0, 0x10);
    if (bar & 0x01) return 0;               // bit0 = I/O BAR ไม่ใช่ memory BAR
    uint32_t base = bar & 0xFFFFFFF0u;
    // เชื่อได้แค่ช่วง MMIO ที่สมเหตุสมผล (QEMU ใส่ LFB บน PCI hole ช่วงนี้)
    if (base < 0xC0000000u || base > 0xFFF00000u) return 0;
    return base;
}

// ---- ตี font 8x16 จาก VGA character ROM (plane 2 ผ่าน A0000) ----
// ทำ 'ก่อน' สลับ graphics mode เพราะ text mode คือ state ที่ ROM ถูก map อยู่
static bool capture_font() {
    // 0xA0000-0xBFFFF อยู่ใน identity map 4MB แรกอยู่แล้ว — ใช้ตรงๆ ได้
    volatile uint8_t* rom = (volatile uint8_t*)VGA_ROM_WINDOW;

    // Sequencer: Clocking mode — ปิด screen off bit ไม่ต้อง, แต่ตั้ง Memory Mode (idx 4)
    outb_p(VGA_SEQ_INDEX, 0x04);
    uint8_t memmode = inb_p(VGA_SEQ_DATA);
    outb_p(VGA_SEQ_DATA, (memmode & ~0x08) | 0x04);  // Chain4 off, Odd/Even off

    // Graphics controller: Misc (idx 6) → map A0000-BFFFF
    outb_p(VGA_GC_INDEX, 0x06);
    uint8_t misc = inb_p(VGA_GC_DATA);
    outb_p(VGA_GC_DATA, (misc & 0xFC) | 0x04);

    // Read Map Select (idx 4) → plane 2 (font)
    outb_p(VGA_GC_INDEX, 0x04);
    uint8_t rdmap = inb_p(VGA_GC_DATA);
    outb_p(VGA_GC_DATA, (rdmap & 0xFC) | 0x02);

    // คัดลอก: 8KB (256 chars × 32 bytes) — เก็บเฉพาะ 16 bytes แรกต่อ char (8x16)
    // sanity check หลังคัด: ตัว 'A' (0x41) ต้องไม่ว่างเปล่า
    bool ok = false;
    for (int c = 0; c < 256; c++) {
        const volatile uint8_t* src = rom + c * 32;
        uint8_t* dst = &font8x16[c * 16];
        for (int i = 0; i < 16; i++) dst[i] = src[i];
    }
    for (int i = 0; i < 16; i++) if (font8x16[0x41 * 16 + i]) { ok = true; break; }

    // Restore registers
    outb_p(VGA_GC_INDEX, 0x04); outb_p(VGA_GC_DATA, rdmap);
    outb_p(VGA_GC_INDEX, 0x06); outb_p(VGA_GC_DATA, misc);
    outb_p(VGA_SEQ_INDEX, 0x04); outb_p(VGA_SEQ_DATA, memmode);
    return ok;
}

// ---- Init: probe + font capture (ยังอยู่ text mode) ----
extern "C" void vga_putchar(char c);  // ใช้ใน fb_init ตอนพิมพ์ dispi id

extern "C" bool fb_init() {
    driver_ready = false;

    uint16_t id = dispi_read(VBE_DISPI_INDEX_ID);
    if ((id & 0xFFF0) != 0xB0C0) {
        vga_puts("\n[fb] dispi not detected (id=");
        const char* hex = "0123456789ABCDEF";
        vga_putchar(hex[(id >> 12) & 0xF]);
        vga_putchar(hex[(id >>  8) & 0xF]);
        vga_putchar(hex[(id >>  4) & 0xF]);
        vga_putchar(hex[ id        & 0xF]);
        vga_puts(")");
        return false;
    }

    lfb_phys = find_lfb_physical();
    if (!lfb_phys) lfb_phys = 0xE0000000u;   // QEMU std-vga default fallback

    if (!capture_font()) {
        vga_puts("\n[fb] font capture failed");
        return false;
    }

    driver_ready = true;
    return true;
}

// ---- เข้า/ออก graphics mode ----
extern "C" bool fb_enter_graphics() {
    // lazy init: probe hardware + ตี font ตอนใช้ครั้งแรก (ยังอยู่ text mode ก่อนบรรทัดนี้เสมอ)
    if (!driver_ready && !fb_init()) return false;

    // map LFB ทั้ง 4MB (1 ตาราง page พอดี) ก่อนสลับ mode
    for (uint32_t off = 0; off < 4 * 1024 * 1024; off += 0x1000) {
        map_page(FB_LFB_VIRT + off, lfb_phys + off, PAGE_KERNEL_RW);
    }

    dispi_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    dispi_write(VBE_DISPI_INDEX_XRES, FB_WIDTH);
    dispi_write(VBE_DISPI_INDEX_YRES, FB_HEIGHT);
    dispi_write(VBE_DISPI_INDEX_BPP, FB_BPP);
    dispi_write(VBE_DISPI_INDEX_VIRT_WIDTH, FB_WIDTH);
    dispi_write(VBE_DISPI_INDEX_VIRT_HEIGHT, FB_HEIGHT * 2);
    dispi_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);

    gfx_mode = true;
    frames_presented = 0;
    fb_fill(FB_BLACK);
    fb_present();
    return true;
}

extern "C" void fb_text_mode() {
    if (!driver_ready) return;
    dispi_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);  // กลับ standard VGA
    gfx_mode = false;
}

extern "C" bool fb_available() { return gfx_mode; }
extern "C" uint32_t fb_tick() { return frames_presented; }

// ---- Primitives (ทั้งหมดลง backbuffer + clip) ----
extern "C" void fb_fill(uint16_t color) {
    for (uint32_t i = 0; i < FB_WIDTH * FB_HEIGHT; i++) backbuf[i] = color;
}

extern "C" void fb_pixel(int x, int y, uint16_t color) {
    if (x < 0 || y < 0 || x >= FB_WIDTH || y >= FB_HEIGHT) return;
    backbuf[y * FB_WIDTH + x] = color;
}

extern "C" void fb_hline(int x1, int x2, int y, uint16_t color) {
    if (y < 0 || y >= FB_HEIGHT) return;
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (x1 < 0) x1 = 0;
    if (x2 >= FB_WIDTH) x2 = FB_WIDTH - 1;
    for (int x = x1; x <= x2; x++) backbuf[y * FB_WIDTH + x] = color;
}

extern "C" void fb_vline(int x, int y1, int y2, uint16_t color) {
    if (x < 0 || x >= FB_WIDTH) return;
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    if (y1 < 0) y1 = 0;
    if (y2 >= FB_HEIGHT) y2 = FB_HEIGHT - 1;
    for (int y = y1; y <= y2; y++) backbuf[y * FB_WIDTH + x] = color;
}

extern "C" void fb_line(int x1, int y1, int x2, int y2, uint16_t color) {
    int dx = x2 - x1, dy = y2 - y1;
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    int sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1;
    int err = ax - ay;
    for (;;) {
        fb_pixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = err * 2;
        if (e2 > -ay) { err -= ay; x1 += sx; }
        if (e2 <  ax) { err += ax; y1 += sy; }
    }
}

extern "C" void fb_rect(int x, int y, int w, int h, uint16_t color) {
    fb_hline(x, x + w - 1, y, color);
    fb_hline(x, x + w - 1, y + h - 1, color);
    fb_vline(x, y, y + h - 1, color);
    fb_vline(x + w - 1, y, y + h - 1, color);
}

extern "C" void fb_fill_rect(int x, int y, int w, int h, uint16_t color) {
    for (int row = y; row < y + h; row++) fb_hline(x, x + w - 1, row, color);
}

extern "C" void fb_circle(int cx, int cy, int r, uint16_t color) {
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        fb_pixel(cx + x, cy + y, color); fb_pixel(cx - x, cy + y, color);
        fb_pixel(cx + x, cy - y, color); fb_pixel(cx - x, cy - y, color);
        fb_pixel(cx + y, cy + x, color); fb_pixel(cx - y, cy + x, color);
        fb_pixel(cx + y, cy - x, color); fb_pixel(cx - y, cy - x, color);
        y++;
        if (err < 0) { err += 2 * y + 1; }
        else { x--; err += 2 * (y - x) + 1; }
    }
}

extern "C" void fb_fill_circle(int cx, int cy, int r, uint16_t color) {
    for (int dy = -r; dy <= r; dy++) {
        int dx = r;
        int dy2 = dy * dy;
        while (dx * dx + dy2 > r * r) dx--;
        fb_hline(cx - dx, cx + dx, cy + dy, color);
    }
}

extern "C" void fb_char(int x, int y, char c, uint16_t fg, uint16_t bg) {
    if (c < 0) c = '?';
    const uint8_t* glyph = &font8x16[(uint8_t)c * 16];
    for (int row = 0; row < 16; row++) {
        uint8_t bits = glyph[row];
        int yy = y + row;
        if (yy < 0 || yy >= FB_HEIGHT) continue;
        for (int col = 0; col < 8; col++) {
            int xx = x + col;
            if (xx < 0 || xx >= FB_WIDTH) continue;
            backbuf[yy * FB_WIDTH + xx] = (bits & (0x80 >> col)) ? fg : bg;
        }
    }
}

extern "C" void fb_text(int x, int y, const char* s, uint16_t fg, uint16_t bg) {
    while (*s) {
        fb_char(x, y, *s, fg, bg);
        x += 8;
        s++;
    }
}

extern "C" void fb_blit(const uint16_t* src, int sx, int sw, int sh, int dx, int dy) {
    for (int row = 0; row < sh; row++) {
        for (int col = 0; col < sw; col++) {
            fb_pixel(dx + col, dy + row, src[row * sw + col]);
        }
    }
    (void)sx;
}

extern "C" void fb_present() {
    if (!gfx_mode) return;
    // ทั้งเฟรม 1.5MB ขึ้น LFB ในครั้งเดียว (minilibc memcpy: align + 32-bit word burst)
    memcpy((void*)FB_LFB_VIRT, backbuf, FB_WIDTH * FB_HEIGHT * 2);
    frames_presented++;
}

// ---- Demo scene: layout คงที่ 100% — fb_test.py เช็ค pixel เหล่านี้ผ่าน QMP screendump ----
extern "C" void fb_draw_demo() {
    fb_fill(FB_BLACK);

    // Title bar + ข้อความ
    fb_fill_rect(0, 0, FB_WIDTH, 24, FB_DARKGRAY);
    fb_rect(0, 0, FB_WIDTH, 24, FB_WHITE);
    fb_text(8, 4, "MeowOS GUI [direct rendering]", FB_WHITE, FB_DARKGRAY);

    // สี่เหลี่ยมทึบแดง + กรอบขาว  |  กรอบสี่เหลี่ยมน้ำเงิน
    fb_fill_rect(100, 100, 80, 60, FB_RED);
    fb_rect(100, 100, 80, 60, FB_WHITE);
    fb_rect(200, 100, 80, 60, FB_BLUE);

    // วงกลมเขียวทึบ + กรอบขาว  |  วงกลมโครงสีม่วง
    fb_fill_circle(400, 200, 50, FB_GREEN);
    fb_circle(400, 200, 50, FB_WHITE);
    fb_circle(650, 450, 80, FB_MAGENTA);

    // เส้นเหลือง (Bresenham) — เช็คที่ปลายทั้งสอง
    fb_line(500, 100, 900, 300, FB_YELLOW);

    fb_present();
}
