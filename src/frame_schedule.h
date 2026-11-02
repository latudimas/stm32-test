/* 60 Hz deadlines on a millisecond timebase, wrap-safe within 2^31 ms. */
#ifndef FRAME_SCHEDULE_H
#define FRAME_SCHEDULE_H
#include <stdint.h>

/* Advance to the first future deadline, discarding missed frames.
 * Three intervals total 50 ms: 16, 17, 17. phase starts at zero. */
static inline void frame_schedule_next(uint32_t *deadline, uint32_t *phase,
                                       uint32_t now)
{
    if ((int32_t)(now - *deadline) >= 0) {
        /* Skip whole cycles first so even a long stall takes constant time. */
        *deadline += ((uint32_t)(now - *deadline) / 50u) * 50u;
        do {
            *deadline += *phase == 0 ? 16u : 17u;
            *phase = (*phase + 1u) % 3u;
        } while ((int32_t)(now - *deadline) >= 0);
    }
}
#endif
