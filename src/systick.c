/*
 * SysTick (PM0056 §4.5): 24-bit down-counter on the processor clock. It counts
 * LOAD..0, then reloads and raises exception 15, which we use as a 1 ms tick.
 */
#include "systick.h"
#include "stm32f103.h"

#define TICKS_PER_MS (CPU_HZ / 1000u)    /* 8000 at 8 MHz */
#define TICKS_PER_US (CPU_HZ / 1000000u) /* 8 at 8 MHz */

static volatile uint32_t ms_ticks;

/* Exception 15 — wired into the vector table in startup.c. */
void SysTick_Handler(void)
{
    ms_ticks++;
}

void systick_init(void)
{
    /* PM0056 §4.5.2 STK_LOAD: count TICKS_PER_MS cycles per period (N-1 for N). */
    STK_LOAD = TICKS_PER_MS - 1u;
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
    uint32_t target = us * TICKS_PER_US;
    uint32_t elapsed = 0;
    uint32_t last = STK_VAL;
    while (elapsed < target) {
        uint32_t now = STK_VAL;
        elapsed += (now <= last) ? last - now : last + TICKS_PER_MS - now;
        last = now;
    }
}
