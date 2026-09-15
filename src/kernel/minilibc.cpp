// Minimal libc routines — GCC อนุญาตให้ emit call หา memcpy/memset/memmove ได้แม้ -ffreestanding
// (เช่น struct copy ใหญ่ๆ หรือ loop ที่ compiler เปลี่ยนเป็น memcpy) — ถ้าไม่มีตัวนี้จะ link ไม่ผ่าน
#include <stdint.h>
#include <stddef.h>

extern "C" {

void* memcpy(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    // คัดหัวจน dst ตรงขอบ 4 bytes แล้วยัดเป็น word (P4 clocking ยังช่วยบน TCG ด้วย)
    while (n && ((uintptr_t)d & 3)) { *d++ = *s++; n--; }
    uint32_t* d4 = (uint32_t*)d;
    const uint32_t* s4 = (const uint32_t*)s;
    while (n >= 4) { *d4++ = *s4++; n -= 4; }
    d = (uint8_t*)d4; s = (const uint8_t*)s4;
    while (n--) { *d++ = *s++; }
    return dst;
}

void* memset(void* dst, int c, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    uint8_t v = (uint8_t)c;
    while (n && ((uintptr_t)d & 3)) { *d++ = v; n--; }
    uint32_t w = v | (v << 8) | (v << 16) | (v << 24);
    uint32_t* d4 = (uint32_t*)d;
    while (n >= 4) { *d4++ = w; n -= 4; }
    d = (uint8_t*)d4;
    while (n--) { *d++ = v; }
    return dst;
}

void* memmove(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    if (d == s || n == 0) return dst;
    if (d < s) {
        while (n--) { *d++ = *s++; }
    } else {
        d += n; s += n;
        while (n--) { *--d = *--s; }
    }
    return dst;
}

int memcmp(const void* a, const void* b, size_t n) {
    const uint8_t* x = (const uint8_t*)a;
    const uint8_t* y = (const uint8_t*)b;
    for (; n; n--, x++, y++) {
        if (*x != *y) return *x - *y;
    }
    return 0;
}

}
