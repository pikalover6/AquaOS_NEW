#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void timer_init(uint32_t hz);
void timer_wait(uint32_t ms);

/* Play a tone on the PC speaker for `ms` milliseconds at `freq` Hz.
   If freq == 0, plays a default beep (~440 Hz, 100 ms). */
void beep(uint32_t freq, uint32_t ms);

#endif /* TIMER_H */
