#ifndef RTC_H
#define RTC_H

#include <stdint.h>

struct RTCData {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint32_t year;
};

extern "C" {
void read_rtc(RTCData* rtc);
}

#endif