/* CBMC proof for frame_schedule_next(): checks every (deadline, phase, now),
 * not samples. Run with `make verify`; host only, no board needed.
 *
 * Spec: the 60 Hz grid starting at the old deadline steps 16, 17, 17 ms
 * (from phase 0). If the old deadline is still ahead of now, nothing changes.
 * Otherwise the new deadline is the first grid point after now: it lies on
 * the grid, it is in the future, and the grid point before it is not. */
#include "frame_schedule.h"

uint32_t nondet_u32(void); /* No body: CBMC treats the result as any value. */

static uint32_t interval(uint32_t phase)
{
    return phase == 0 ? 16u : 17u;
}

int main(void)
{
    uint32_t deadline = nondet_u32(), phase = nondet_u32(), now = nondet_u32();
    __CPROVER_assume(phase < 3u); /* Phase starts at zero and stays in 0..2. */

    uint32_t old_deadline = deadline, old_phase = phase;
    frame_schedule_next(&deadline, &phase, now);

    __CPROVER_assert(phase < 3u, "phase stays in 0..2");

    if ((int32_t)(now - old_deadline) < 0) {
        __CPROVER_assert(deadline == old_deadline && phase == old_phase,
                         "not due: deadline and phase unchanged");
        return 0;
    }

    __CPROVER_assert((int32_t)(deadline - now) > 0, "new deadline is in the future");
    __CPROVER_assert(deadline - now <= 17u, "at most one frame ahead");

    /* On the grid: whole 50 ms cycles plus the steps that moved phase along. */
    uint32_t partial = 0;
    for (uint32_t p = old_phase; p != phase; p = (p + 1u) % 3u)
        partial += interval(p);
    __CPROVER_assert((deadline - old_deadline - partial) % 50u == 0,
                     "new deadline lies on the original grid (no drift)");

    uint32_t previous = deadline - interval((phase + 2u) % 3u);
    __CPROVER_assert((int32_t)(now - previous) >= 0,
                     "the grid point before it is already due (first future one)");
    return 0;
}
