#include "rtc.h"

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static int get_update_in_progress_flag() {
    outb(CMOS_ADDRESS, 0x0A);
    return (inb(CMOS_DATA) & 0x80);
}

static uint8_t get_rtc_register(int reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

extern "C" void read_rtc(RTCData* rtc) {
    while (get_update_in_progress_flag());

    rtc->second = get_rtc_register(0x00);
    rtc->minute = get_rtc_register(0x02);
    rtc->hour   = get_rtc_register(0x04);
    rtc->day    = get_rtc_register(0x07);
    rtc->month  = get_rtc_register(0x08);
    rtc->year   = get_rtc_register(0x09);

    uint8_t registerB = get_rtc_register(0x0B);

    if (!(registerB & 0x04)) {
        rtc->second = (rtc->second & 0x0F) + ((rtc->second / 16) * 10);
        rtc->minute = (rtc->minute & 0x0F) + ((rtc->minute / 16) * 10);
        rtc->hour   = ((rtc->hour & 0x0F) + (((rtc->hour & 0x70) / 16) * 10)) | (rtc->hour & 0x80);
        rtc->day    = (rtc->day & 0x0F) + ((rtc->day / 16) * 10);
        rtc->month  = (rtc->month & 0x0F) + ((rtc->month / 16) * 10);
        rtc->year   = (rtc->year & 0x0F) + ((rtc->year / 16) * 10);
    }

    if (!(registerB & 0x02) && (rtc->hour & 0x80)) {
        rtc->hour = ((rtc->hour & 0x7F) + 12) % 24;
    }

    rtc->year += 2000;
}