/* Run from repository root; replay fixtures are operator-labelled recordings. */
#include <assert.h>
#include <stdio.h>
#include "motion.h"

static void replay(const char *path, enum motion_state expected)
{
    FILE *f = fopen(path, "r");
    assert(f);
    char header[80];
    assert(fgets(header, sizeof header, f));
    struct motion m;
    motion_init(&m);
    unsigned ms, count[3] = {0}, seen = 0;
    int x, y, z;
    while (fscanf(f, "%u,%d,%d,%d", &ms, &x, &y, &z) == 4) {
        motion_update(&m, ms, (int16_t)x, (int16_t)y, (int16_t)z);
        assert(m.state <= expected); /* quiet never reacts; wobble never confuses */
        count[m.state]++;
        seen |= 1u << m.state;
    }
    assert(feof(f));
    fclose(f);
    assert(seen & (1u << expected));
    printf("%s: default=%u squinting=%u confused=%u\n", path, count[0], count[1], count[2]);
}

static void alternating(struct motion *m, uint32_t *ms, int amplitude, uint32_t dt)
{
    for (int i = 0; i < 40; i++) {
        *ms += dt;
        motion_update(m, *ms, (int16_t)(i & 1 ? amplitude : -amplitude), 0, 14000);
    }
}

int main(void)
{
    replay("tests/data/quiet.csv", MOTION_DEFAULT);
    replay("tests/data/wobble.csv", MOTION_SQUINTING);
    replay("tests/data/shake.csv", MOTION_CONFUSED);
    struct motion m;
    motion_init(&m);
    uint32_t ms = 0;
    motion_update(&m, ms, 0, 0, 14000);
    alternating(&m, &ms, 6000, 20);
    assert(m.state == MOTION_SQUINTING);
    alternating(&m, &ms, 15000, 20);
    assert(m.state == MOTION_CONFUSED);
    alternating(&m, &ms, 6000, 20);
    assert(m.state == MOTION_CONFUSED); /* never downgrade during weaker motion */
    motion_missing(&m);
    assert(!m.quiet && m.wobble_ms == 0 && m.shake_ms == 0);
    motion_poll(&m, ms + 499);
    assert(m.state == MOTION_CONFUSED);
    motion_poll(&m, ms + 500);
    assert(m.state == MOTION_DEFAULT && !m.initialized);

    /* Quiet proof begins only on a fresh quiet sample, including at time zero. */
    motion_init(&m);
    motion_update(&m, 0, 0, 0, 14000);
    m.state = MOTION_CONFUSED;
    for (ms = 20; ms <= 1920; ms += 100) {
        motion_update(&m, ms, 0, 0, 14000);
    }
    motion_update(&m, 2019, 0, 0, 14000);
    assert(m.quiet_since == 20 && m.state == MOTION_CONFUSED);
    motion_update(&m, 2020, 0, 0, 14000);
    assert(m.state == MOTION_DEFAULT);

    /* Missing samples and a >100 ms gap cannot complete quiet/evidence. */
    m.state = MOTION_CONFUSED;
    motion_missing(&m);
    motion_update(&m, 2030, 0, 0, 14000);
    assert(m.quiet_since == 2030);
    motion_update(&m, 2400, 0, 0, 14000);
    assert(!m.quiet && m.state == MOTION_CONFUSED);
    motion_update(&m, 2410, 0, 0, 14000);
    assert(m.quiet_since == 2410);
    motion_update(&m, 2420, 12000, 0, 14000);
    assert(!m.quiet); /* renewed motion breaks the quiet timer */

    /* A duplicate timestamp cannot add evidence. */
    unsigned evidence = m.wobble_ms;
    motion_update(&m, 2420, -12000, 0, 14000);
    assert(m.wobble_ms == evidence);

    /* One isolated full-range spike must not select a reaction at 60/12 Hz. */
    for (unsigned dt = 17; dt <= 83; dt += 66) {
        motion_init(&m);
        motion_update(&m, 0, 0, 0, 14000);
        motion_update(&m, dt, 32767, 0, 14000);
        for (ms = 2 * dt; ms < 1000; ms += dt) {
            motion_update(&m, ms, 0, 0, 14000);
            assert(m.state == MOTION_DEFAULT);
        }
    }

    /* Slow orientation change: no shake or squint (linear gravity-vector sweep). */
    motion_init(&m);
    for (ms = 0; ms <= 10000; ms += 20) {
        motion_update(&m, ms, (int16_t)(16384u * ms / 10000u), 0,
                      (int16_t)(16384u - 16384u * ms / 10000u));
        assert(m.state == MOTION_DEFAULT);
    }

    /* Low-rate confirmation and stale fallback remain time-based. */
    motion_init(&m);
    ms = UINT32_MAX - 300;
    motion_update(&m, ms, 0, 0, 14000);
    alternating(&m, &ms, 15000, 83);
    assert(m.state == MOTION_CONFUSED);
    motion_poll(&m, ms + 500);
    assert(m.state == MOTION_DEFAULT);

    /* Quiet timer also works over uint32_t wrap. */
    motion_init(&m);
    ms = UINT32_MAX - 1000;
    motion_update(&m, ms, 0, 0, 14000);
    m.state = MOTION_CONFUSED;
    for (unsigned elapsed = 20; elapsed <= 2000; elapsed += 20) {
        motion_update(&m, ms + elapsed, 0, 0, 14000);
        assert(m.state == MOTION_CONFUSED);
    }
    motion_update(&m, ms + 2020, 0, 0, 14000);
    assert(m.state == MOTION_DEFAULT);

    /* Signed extremes and maximum accepted dt must not overflow. */
    motion_init(&m);
    for (ms = 0; ms < 5000; ms += 100) {
        int16_t v = ms % 200 ? INT16_MIN : INT16_MAX;
        motion_update(&m, ms, v, v, v);
    }
    return 0;
}
