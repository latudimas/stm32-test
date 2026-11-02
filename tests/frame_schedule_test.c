#include <assert.h>
#include "frame_schedule.h"

int main(void)
{
    uint32_t next = 0, phase = 0;
    for (uint32_t frame = 0; frame < 600; frame++) {
        uint32_t now = next;
        frame_schedule_next(&next, &phase, now);
        assert(next - now == (frame % 3 == 0 ? 16u : 17u));
    }
    assert(next == 10000 && phase == 0);
    next = UINT32_MAX - 7u;
    phase = 0;
    frame_schedule_next(&next, &phase, next);
    assert(next == 8 && phase == 1);
    frame_schedule_next(&next, &phase, 108);
    assert(next == 125 && phase == 2);
    frame_schedule_next(&next, &phase, 109);
    assert(next == 125 && phase == 2); /* Not due: leave unchanged. */
    frame_schedule_next(&next, &phase, 1000000109);
    assert((int32_t)(next - 1000000109u) > 0);
    assert(next - 1000000109u <= 17u);
    return 0;
}
