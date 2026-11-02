#include "motion.h"

/* Relative acceleration, not calibrated g. Wobble requires repeated signed
 * excursions; amplitude alone also responds to lifting/repositioning. Shake's
 * original magnitude/change/evidence rule is deliberately unchanged. */
#define GRAVITY_MS 80u
#define MAX_GAP_MS 100u
#define QUIET_MG   25u
#define WOBBLE_MG  70u
#define SHAKE_MG  420u
#define SHAKE_CONFIRM_MS  160u

/* Provisional gesture timing: >=2 cycles on one axis, not a lift's single
 * acceleration/deceleration pair. Short spikes and fast ringing do not count.
 * Validate these settings with independent real handling recordings. */
#define WOBBLE_HOLD_MS       50u
#define WOBBLE_HALF_MIN_MS  100u
#define WOBBLE_HALF_MAX_MS  600u
#define WOBBLE_SPAN_MIN_MS  600u
#define WOBBLE_WINDOW_MS   2200u
#define WOBBLE_LOBES          5u

static void wobble_reset(struct motion_wobble_axis *a)
{
    a->first_ms = a->last_ms = a->candidate_since = 0;
    a->sign = a->candidate_sign = 0;
    a->lobes = 0;
}

static int wobble_update(struct motion_wobble_axis *a, uint32_t ms, int32_t residual)
{
    if (a->lobes && (ms - a->last_ms > WOBBLE_HALF_MAX_MS ||
                    ms - a->first_ms > WOBBLE_WINDOW_MS)) {
        wobble_reset(a); /* disconnected handling events cannot accumulate */
    }
    int8_t sign = residual >= (int32_t)WOBBLE_MG ? 1 :
                 residual <= -(int32_t)WOBBLE_MG ? -1 : 0;
    if (!sign) {
        a->candidate_sign = 0; /* neutral deadband breaks excursion persistence */
        return 0;
    }
    if (sign != a->candidate_sign) {
        a->candidate_sign = sign;
        a->candidate_since = ms;
    }
    if (ms - a->candidate_since < WOBBLE_HOLD_MS || sign == a->sign) {
        return 0;
    }
    if (!a->lobes || ms - a->last_ms < WOBBLE_HALF_MIN_MS) {
        a->lobes = 1; /* too-fast alternation starts a new candidate sequence */
        a->first_ms = ms;
    } else if (a->lobes < WOBBLE_LOBES) {
        a->lobes++;
    }
    a->sign = sign;
    a->last_ms = ms;
    return a->lobes >= WOBBLE_LOBES && ms - a->first_ms >= WOBBLE_SPAN_MIN_MS;
}

void motion_init(struct motion *m)
{
    m->state = MOTION_DEFAULT;
    m->initialized = 0;
    m->last_ms = m->quiet_since = 0;
    m->energy_mg2 = 0;
    motion_missing(m);
}

void motion_missing(struct motion *m)
{
    m->quiet = m->changing_samples = 0;
    m->shake_ms = 0;
    for (uint32_t i = 0; i < 3; i++) {
        wobble_reset(&m->wobble[i]);
    }
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
    int wobbling = 0;
    for (uint32_t i = 0; i < 3; i++) {
        int32_t error = mg[i] * 256 - m->gravity_q8[i];
        /* dt <= 100; worst-case signed input keeps this product in int32_t. */
        m->gravity_q8[i] += error * (int32_t)dt / (int32_t)(GRAVITY_MS + dt);
        int32_t residual = (mg[i] * 256 - m->gravity_q8[i]) / 256;
        energy += (uint32_t)(residual * residual);
        wobbling |= wobble_update(&m->wobble[i], ms, residual);
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
    m->shake_ms = evidence(m->shake_ms, dt,
        changing && energy >= SHAKE_MG * SHAKE_MG, SHAKE_CONFIRM_MS);

    if (m->changing_samples >= 3u && m->shake_ms >= SHAKE_CONFIRM_MS) {
        m->state = MOTION_CONFUSED;
    } else if (m->state == MOTION_DEFAULT && wobbling) {
        m->state = MOTION_SQUINTING;
    }
    /* No confused -> squinting downgrade while weaker motion continues. */
    if (energy <= QUIET_MG * QUIET_MG) {
        if (!m->quiet) {
            m->quiet = 1;
            m->quiet_since = ms;
        }
        uint32_t quiet_ms = m->state == MOTION_CONFUSED ?
            MOTION_CONFUSED_QUIET_MS : MOTION_SQUINT_QUIET_MS;
        if (ms - m->quiet_since >= quiet_ms) {
            m->state = MOTION_DEFAULT;
            m->shake_ms = 0;
            for (uint32_t i = 0; i < 3; i++) {
                wobble_reset(&m->wobble[i]);
            }
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
