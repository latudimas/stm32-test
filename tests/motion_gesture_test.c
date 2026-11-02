/* Synthetic regressions, not substitutes for real lift/carry recordings. */
#include <assert.h>
#include <stdint.h>
#include "motion.h"

static void sample(struct motion *m, uint32_t ms, unsigned axis, int raw)
{
    int16_t v[3] = {0, 0, 14000};
    v[axis] = (int16_t)(v[axis] + raw);
    motion_update(m, ms, v[0], v[1], v[2]);
}

static int triangle_pulse(unsigned t, unsigned start, unsigned width, int peak)
{
    if (t < start || t >= start + width) return 0;
    unsigned phase = t - start;
    if (phase > width / 2) phase = width - phase;
    return (int)phase * peak / (int)(width / 2);
}

static void handling(void)
{
    for (unsigned axis = 0; axis < 3; axis++) {
        for (int direction = -1; direction <= 1; direction += 2) {
            struct motion m;
            motion_init(&m);
            /* Lift: accelerate, coast, decelerate; then remain still. */
            for (unsigned t = 0; t <= 4000; t += 20) {
                int value = triangle_pulse(t, 100, 400, 6000) -
                            triangle_pulse(t, 1100, 400, 6000);
                sample(&m, t, axis, direction * value);
                assert(m.state == MOTION_DEFAULT);
            }
            motion_init(&m);
            /* A reposition changes the gravity/bias vector once, then holds. */
            for (unsigned t = 0; t <= 4000; t += 20) {
                int value = t < 400 ? (int)t * 5000 / 400 : 5000;
                sample(&m, t, axis, direction * value);
                assert(m.state == MOTION_DEFAULT);
            }
            motion_init(&m);
            /* A few very short movements, well below deliberate-shake strength. */
            for (unsigned t = 0; t <= 4000; t += 10) {
                int value = t < 1000 && t % 200 == 100 ? direction * 6000 : 0;
                sample(&m, t, axis, value);
                assert(m.state == MOTION_DEFAULT);
            }
        }
    }

    struct motion m;
    motion_init(&m);
    for (unsigned t = 0; t <= 2000; t += 10) {
        /* Too-fast alternations cannot pass the >=100 ms half-cycle rule. */
        int value = t && t < 1200 ? (t / 60 & 1 ? -6000 : 6000) : 0;
        sample(&m, t, 0, value);
        assert(m.state == MOTION_DEFAULT);
    }
    motion_init(&m);
    for (unsigned t = 0; t <= 6000; t += 20) {
        /* Separate positive/negative X/Y/Z events cannot share one history. */
        unsigned phase = t / 200;
        unsigned axis = phase % 3;
        int value = phase < 6 ? (phase < 3 ? 6000 : -6000) : 0;
        sample(&m, t, axis, value);
        assert(m.state == MOTION_DEFAULT);
    }
}

static void lobe(struct motion *m, uint32_t *ms, unsigned axis, int value,
                 unsigned half_ms, unsigned dt)
{
    for (unsigned t = 0; t < half_ms; t += dt) {
        *ms += dt;
        sample(m, *ms, axis, value);
    }
}

static void oscillations(void)
{
    for (unsigned axis = 0; axis < 3; axis++) {
        for (unsigned dt = 17; dt <= 83; dt += 66) {
            for (unsigned wrap = 0; wrap < 2; wrap++) {
                struct motion m;
                uint32_t ms = wrap ? UINT32_MAX - 500 : 0;
                motion_init(&m);
                sample(&m, ms, axis, 0);
                for (unsigned i = 0; i < 4; i++) {
                    lobe(&m, &ms, axis, i & 1 ? -6000 : 6000, 200, dt);
                    assert(m.state == MOTION_DEFAULT); /* not enough cycles */
                }
                lobe(&m, &ms, axis, 6000, 200, dt);
                assert(m.state == MOTION_SQUINTING);
            }
        }
    }
    struct motion m;
    uint32_t ms = 0;
    motion_init(&m);
    sample(&m, ms, 0, 0);
    for (unsigned i = 0; i < 5; i++) {
        lobe(&m, &ms, 0, i & 1 ? -6000 : 6000, 100, 20);
    }
    assert(m.state == MOTION_DEFAULT); /* five lobes, but <600 ms span */

    motion_init(&m);
    ms = 0;
    sample(&m, ms, 0, 0);
    for (unsigned i = 0; i < 10; i++) {
        lobe(&m, &ms, 0, i & 1 ? -6000 : 6000, 700, 20);
        assert(m.state == MOTION_DEFAULT); /* pauses are too long to connect */
    }

    motion_init(&m);
    ms = 0;
    sample(&m, ms, 0, 0);
    for (unsigned i = 0; i < 10; i++) {
        lobe(&m, &ms, 0, i & 1 ? -6000 : 6000, 580, 20);
        assert(m.state == MOTION_DEFAULT); /* valid spacing, but >2200 ms for 5 lobes */
    }

    for (unsigned gap = 0; gap < 2; gap++) {
        motion_init(&m);
        ms = 0;
        sample(&m, ms, 0, 0);
        for (unsigned i = 0; i < 4; i++) {
            lobe(&m, &ms, 0, i & 1 ? -6000 : 6000, 200, 20);
        }
        assert(m.wobble[0].lobes == 4);
        /* Duplicate timestamps do not change any excursion timing/counts. */
        struct motion_wobble_axis before = m.wobble[0];
        sample(&m, ms, 0, 6000);
        assert(m.wobble[0].lobes == before.lobes);
        assert(m.wobble[0].candidate_since == before.candidate_since);
        assert(m.wobble[0].last_ms == before.last_ms);
        if (gap) {
            ms += 150;
            sample(&m, ms, 0, 0); /* unread interval rebaselines */
        } else {
            motion_missing(&m); /* explicit failed/not-ready sample */
        }
        for (unsigned axis = 0; axis < 3; axis++) assert(m.wobble[axis].lobes == 0);
        lobe(&m, &ms, 0, 6000, 200, 20);
        assert(m.state == MOTION_DEFAULT); /* cannot complete the old sequence */
    }
}

static void quiet_boundary(enum motion_state state, unsigned timeout, uint32_t start)
{
    struct motion m;
    motion_init(&m);
    sample(&m, start, 0, 0);
    m.state = state;
    for (unsigned elapsed = 20; elapsed <= timeout; elapsed += 20) {
        sample(&m, start + elapsed, 0, 0);
        assert(m.state == state);
    }
    assert(m.quiet_since == start + 20);
    sample(&m, start + 20 + timeout - 1, 0, 0);
    assert(m.state == state);
    sample(&m, start + 20 + timeout, 0, 0);
    assert(m.state == MOTION_DEFAULT);
}

static void quiet_reset(enum motion_state state, unsigned timeout)
{
    struct motion m;
    motion_init(&m);
    sample(&m, 0, 0, 0);
    m.state = state;
    unsigned ms;
    for (ms = 20; ms <= 1000; ms += 20) sample(&m, ms, 0, 0);
    sample(&m, ms, 0, 2000); /* modest movement breaks quiet, not a new gesture */
    assert(!m.quiet && m.state == state);
    do {
        ms += 20;
        sample(&m, ms, 0, 0);
    } while (!m.quiet);
    uint32_t since = m.quiet_since;
    for (ms += 20; ms < since + timeout; ms += 20) {
        sample(&m, ms, 0, 0);
        assert(m.state == state);
    }
    sample(&m, since + timeout, 0, 0);
    assert(m.state == MOTION_DEFAULT);

    /* Missing data also invalidates an almost complete quiet timer. */
    motion_init(&m);
    sample(&m, 0, 0, 0);
    m.state = state;
    for (ms = 20; ms <= timeout; ms += 20) sample(&m, ms, 0, 0);
    motion_missing(&m);
    since = ms;
    sample(&m, ms, 0, 0);
    for (ms += 20; ms < since + timeout; ms += 20) {
        sample(&m, ms, 0, 0);
        assert(m.state == state);
    }
    sample(&m, since + timeout, 0, 0);
    assert(m.state == MOTION_DEFAULT);
}

int main(void)
{
    handling();
    oscillations();
    for (unsigned wrap = 0; wrap < 2; wrap++) {
        uint32_t start = wrap ? UINT32_MAX - 500 : 0;
        quiet_boundary(MOTION_SQUINTING, 1200, start);
        quiet_boundary(MOTION_CONFUSED, 2000, start);
    }
    quiet_reset(MOTION_SQUINTING, 1200);
    quiet_reset(MOTION_CONFUSED, 2000);
    return 0;
}
