/*
 * SysTick timebase: 1 ms interrupt tick + cycle-counted microsecond delays.
 * All timing derives from CPU_HZ (stm32f103.h).
 */
#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>

void systick_init(void);
uint32_t millis(void);        /* ms since systick_init(); wraps after ~49 days */
void delay_ms(uint32_t ms);
void delay_us(uint32_t us);   /* busy-waits on the counter; for short waits (I2C) */

#endif
