#ifndef RTC_H
#define RTC_H

#include <stdint.h>

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
} rtc_time_t;

void rtc_read(rtc_time_t *t);

/* Print current date/time to VGA in the style of DateTime.Now */
void rtc_print(void);

#endif /* RTC_H */
