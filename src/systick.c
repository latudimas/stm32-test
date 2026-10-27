/*
 * SysTick (PM0056 §4.5): 24-bit down-counter on the processor clock. It counts
 * LOAD..0, then reloads and raises exception 15, which we use as a 1 ms tick.
 */
#include "systick.h"
#include "clock.h"
#include "stm32f103.h"

/* Set from cpu_hz in systick_init(): 72000 / 72 at 72 MHz, 8000 / 8 at 8 MHz. */
static uint32_t ticks_per_ms;
static uint32_t ticks_per_us;

static volatile uint32_t ms_ticks;

/* Exception 15 — wired into the vector table in startup.c. */
void SysTick_Handler(void)
{
    ms_ticks++;
}

void systick_init(void)
{
    ticks_per_ms = cpu_hz / 1000u;
    ticks_per_us = cpu_hz / 1000000u;

    /* PM0056 §4.5.2 STK_LOAD: count ticks_per_ms cycles per period (N-1 for N).
     * 72000 - 1 fits the 24-bit field (max 16777215). */
    STK_LOAD = ticks_per_ms - 1u;
    /* PM0056 §4.5.3 STK_VAL: any write clears it, so the first period is full. */
    STK_VAL = 0;
    /* PM0056 §4.5.1 STK_CTRL: processor clock, interrupt on wrap, enable.
     * Exceptions are enabled out of reset (PRIMASK=0), so no cpsie needed. */
    STK_CTRL = STK_CTRL_CLKSOURCE | STK_CTRL_TICKINT | STK_CTRL_ENABLE;
}

uint32_t millis(void)
{
    return ms_ticks;
}

void delay_ms(uint32_t ms)
{
    uint32_t start = ms_ticks;
    /* Unsigned subtraction stays correct across the 2^32 wrap. */
    while (ms_ticks - start < ms) {
    }
}

void delay_us(uint32_t us)
{
    /* Too short for the 1 ms tick: watch the counter itself and add up how far
     * it has moved, allowing for it reloading from 0 back to LOAD. */
    uint32_t target = us * ticks_per_us;
    uint32_t elapsed = 0;
    uint32_t last = STK_VAL;
    while (elapsed < target) {
        uint32_t now = STK_VAL;
        elapsed += (now <= last) ? last - now : last + ticks_per_ms - now;
        last = now;
    }
}
