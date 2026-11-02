#include "motion.h"

/* Initial tuning against tests/data/{quiet,wobble,shake}.csv, not calibrated g.
 * Gravity follows an 80 ms low-pass; the remainder is relative acceleration.
 * Separate quiet/wobble/shake levels provide hysteresis. Evidence integrates
 * active milliseconds and decays equally quickly between active samples. */
#define GRAVITY_MS 80u
#define MAX_GAP_MS 100u
#define QUIET_MG   25u
#define WOBBLE_MG  70u
#define SHAKE_MG  420u
#define WOBBLE_CONFIRM_MS 100u
#define SHAKE_CONFIRM_MS  160u

void motion_init(struct motion *m)
{
    m->state = MOTION_DEFAULT;
    m->initialized = 0;
    m->quiet = m->changing_samples = 0;
    m->last_ms = m->quiet_since = 0;
    m->wobble_ms = m->shake_ms = m->energy_mg2 = 0;
}

void motion_missing(struct motion *m)
{
    m->quiet = m->changing_samples = 0;
    m->wobble_ms = m->shake_ms = 0;
}

void motion_poll(struct motion *m, uint32_t now)
{
    if (m->initialized && now - m->last_ms >= MOTION_STALE_MS) {
        motion_init(m);
    }
}

static uint32_t evidence(uint32_t value, uint32_t dt, int active, uint32_t limit)
{
    if (!active) {
        return value > dt ? value - dt : 0;
    }
    value += dt;
    return value > limit ? limit : value;
}

void motion_update(struct motion *m, uint32_t ms, int16_t x, int16_t y, int16_t z)
{
    int32_t mg[3] = {
        (int32_t)x * 1000 / 16384,
        (int32_t)y * 1000 / 16384,
        (int32_t)z * 1000 / 16384,
    };
    uint32_t dt = ms - m->last_ms;
    if (m->initialized && dt == 0) {
        return; /* duplicate timestamp provides no new elapsed evidence */
    }
    if (!m->initialized || dt > MAX_GAP_MS) {
        if (m->initialized && dt >= MOTION_STALE_MS) {
            motion_init(m);
        }
        for (uint32_t i = 0; i < 3; i++) {
            m->gravity_q8[i] = mg[i] * 256;
            m->previous_mg[i] = mg[i];
        }
        motion_missing(m);
        m->energy_mg2 = 0;
        m->initialized = 1;
        m->last_ms = ms;
        return;
    }
    m->last_ms = ms;
    uint32_t energy = 0, change = 0;
    for (uint32_t i = 0; i < 3; i++) {
        int32_t error = mg[i] * 256 - m->gravity_q8[i];
        /* dt <= 100; worst-case signed input keeps this product in int32_t. */
        m->gravity_q8[i] += error * (int32_t)dt / (int32_t)(GRAVITY_MS + dt);
        int32_t residual = (mg[i] * 256 - m->gravity_q8[i]) / 256;
        energy += (uint32_t)(residual * residual);
        int32_t delta = mg[i] - m->previous_mg[i];
        change += (uint32_t)(delta < 0 ? -delta : delta);
        m->previous_mg[i] = mg[i];
    }
    m->energy_mg2 = energy;
    /* Require continuing input changes, not just the filter tail from a single
     * impact. L1 change >= 12 mg per nominal 17 ms, normalized by actual dt. */
    int changing = change * 17u >= 12u * dt;
    if (!changing) {
        m->changing_samples = 0;
    } else if (m->changing_samples < 3u) {
        m->changing_samples++;
    }
    m->wobble_ms = evidence(m->wobble_ms, dt,
        changing && energy >= WOBBLE_MG * WOBBLE_MG, WOBBLE_CONFIRM_MS);
    m->shake_ms = evidence(m->shake_ms, dt,
        changing && energy >= SHAKE_MG * SHAKE_MG, SHAKE_CONFIRM_MS);

    if (m->changing_samples >= 3u && m->shake_ms >= SHAKE_CONFIRM_MS) {
        m->state = MOTION_CONFUSED;
    } else if (m->changing_samples >= 3u && m->state == MOTION_DEFAULT &&
               m->wobble_ms >= WOBBLE_CONFIRM_MS) {
        m->state = MOTION_SQUINTING;
    }
    /* No confused -> squinting downgrade while weaker motion continues. */
    if (energy <= QUIET_MG * QUIET_MG) {
        if (!m->quiet) {
            m->quiet = 1;
            m->quiet_since = ms;
        }
        if (ms - m->quiet_since >= MOTION_QUIET_MS) {
            m->state = MOTION_DEFAULT;
            m->wobble_ms = m->shake_ms = 0;
        }
    } else {
        m->quiet = 0;
    }
}

const char *motion_name(enum motion_state state)
{
    switch (state) {
    case MOTION_SQUINTING: return "squinting";
    case MOTION_CONFUSED: return "confused";
    default: return "default";
    }
}
