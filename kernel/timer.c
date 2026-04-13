#include "timer.h"
#include "idt.h"
#include "io.h"
#include <stdint.h>

/* ---- PIT (Programmable Interval Timer, 8253/8254) ------------------- */
#define PIT_CHANNEL0  0x40
#define PIT_CHANNEL2  0x42
#define PIT_CMD       0x43
#define PIT_BASE_FREQ 1193180UL   /* PIT input frequency in Hz */

static volatile uint32_t timer_ticks = 0;
static uint32_t ticks_per_ms = 0;

static void timer_irq(regs_t *r) {
    (void)r;
    timer_ticks++;
}

void timer_init(uint32_t hz) {
    ticks_per_ms = hz / 1000;
    if (ticks_per_ms == 0) ticks_per_ms = 1;

    uint32_t divisor = (uint32_t)(PIT_BASE_FREQ / hz);
    outb(PIT_CMD, 0x36);                     /* channel 0, square wave, lobyte/hibyte */
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    irq_install_handler(0, timer_irq);
}

void timer_wait(uint32_t ms) {
    uint32_t target = timer_ticks + ms * ticks_per_ms;
    while (timer_ticks < target)
        __asm__ volatile ("hlt");
}

/* ---- PC Speaker ----------------------------------------------------- */
static void speaker_on(uint32_t freq) {
    if (freq == 0) return;
    uint32_t div = (uint32_t)(PIT_BASE_FREQ / freq);
    outb(PIT_CMD, 0xB6);                     /* channel 2, square wave, lobyte/hibyte */
    outb(PIT_CHANNEL2, (uint8_t)(div & 0xFF));
    outb(PIT_CHANNEL2, (uint8_t)((div >> 8) & 0xFF));
    uint8_t tmp = inb(0x61);
    if ((tmp & 0x03) != 0x03)
        outb(0x61, tmp | 0x03);              /* enable speaker gate */
}

static void speaker_off(void) {
    uint8_t tmp = inb(0x61);
    outb(0x61, tmp & 0xFC);
}

void beep(uint32_t freq, uint32_t ms) {
    if (freq == 0) { freq = 440; ms = 100; }
    speaker_on(freq);
    timer_wait(ms);
    speaker_off();
}
