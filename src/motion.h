/* Sample-driven relative motion classifier; no hardware/display dependencies. */
#ifndef MOTION_H
#define MOTION_H
#include <stdint.h>

enum motion_state { MOTION_DEFAULT, MOTION_SQUINTING, MOTION_CONFUSED };

#define MOTION_QUIET_MS 2000u
#define MOTION_STALE_MS 500u

struct motion {
    enum motion_state state;
    int32_t gravity_q8[3];
    int32_t previous_mg[3];
    uint32_t last_ms, quiet_since;
    uint32_t wobble_ms, shake_ms;
    uint32_t energy_mg2; /* high-pass acceleration squared, nominal mg^2 */
    uint8_t initialized, quiet, changing_samples;
};

void motion_init(struct motion *m);
/* Raw +/-2 g samples, nominal 16384 counts/g. Timestamp must be sample time.
 * First sample establishes a baseline without requiring stationary startup. */
void motion_update(struct motion *m, uint32_t ms, int16_t x, int16_t y, int16_t z);
/* Missing/error samples break evidence and continuous-quiet proof. */
void motion_missing(struct motion *m);
/* Check health even without fresh samples; stale data falls back to default,
 * not a claim of physical quiet. Never completes a quiet timer by itself. */
void motion_poll(struct motion *m, uint32_t now);
const char *motion_name(enum motion_state state);
#endif
