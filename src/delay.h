#ifndef DELAY_H
#define DELAY_H

#include <stdint.h>

/* Busy-wait. ~10 CPU cycles per iteration at 8 MHz (measured), i.e. ~1.25 us. */
static inline void delay(uint32_t n)
{
    /* volatile keeps the compiler from deleting the empty loop. */
    for (volatile uint32_t i = 0; i < n; i++) {
    }
}

#endif
