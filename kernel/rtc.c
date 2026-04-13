#include "rtc.h"
#include "vga.h"
#include "io.h"
#include <stdint.h>

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

static int rtc_updating(void) {
    outb(CMOS_ADDR, 0x0A);
    return (inb(CMOS_DATA) & 0x80) != 0;
}

static uint8_t bcd2bin(uint8_t v) {
    return (uint8_t)(((v >> 4) * 10) + (v & 0x0F));
}

void rtc_read(rtc_time_t *t) {
    /* Wait until no update in progress */
    while (rtc_updating());

    uint8_t status_b = cmos_read(0x0B);
    int binary_mode  = (status_b & 0x04) != 0;
    int hour24       = (status_b & 0x02) != 0;

    t->second = cmos_read(0x00);
    t->minute = cmos_read(0x02);
    t->hour   = cmos_read(0x04);
    t->day    = cmos_read(0x07);
    t->month  = cmos_read(0x08);
    uint8_t yr = cmos_read(0x09);
    uint8_t cent = cmos_read(0x32);  /* century register */

    if (!binary_mode) {
        t->second = bcd2bin(t->second);
        t->minute = bcd2bin(t->minute);
        t->hour   = bcd2bin(t->hour & 0x7F);
        t->day    = bcd2bin(t->day);
        t->month  = bcd2bin(t->month);
        yr        = bcd2bin(yr);
        cent      = bcd2bin(cent);
    }

    if (!hour24 && (t->hour == 12)) t->hour = 0;

    /* Century handling */
    if (cent != 0)
        t->year = (uint16_t)(cent * 100 + yr);
    else
        t->year = (uint16_t)((yr >= 70 ? 1900 : 2000) + yr);
}

/* Print two-digit number with leading zero */
static void print2(uint8_t n) {
    vga_putchar('0' + n / 10);
    vga_putchar('0' + n % 10);
}

void rtc_print(void) {
    rtc_time_t t;
    rtc_read(&t);
    vga_putint(t.month);
    vga_putchar('/');
    print2(t.day);
    vga_putchar('/');
    vga_putint(t.year);
    vga_putchar(' ');
    print2(t.hour);
    vga_putchar(':');
    print2(t.minute);
    vga_putchar(':');
    print2(t.second);
    vga_putchar(' ');
    vga_puts(t.hour < 12 ? "AM" : "PM");
    vga_putchar('\n');
}
