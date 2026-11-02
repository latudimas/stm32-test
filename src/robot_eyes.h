/* Grok-inspired robot face: two rounded eyes, glances and eyelid blinks. */
#ifndef ROBOT_EYES_H
#define ROBOT_EYES_H

#include <stdint.h>

/* Fixed page-aligned window enclosing every pose, including old positions.
 * Update these bounds if the eye dimensions or travel change. */
#define ROBOT_EYES_X     13u
#define ROBOT_EYES_PAGE   1u
#define ROBOT_EYES_WIDTH 102u
#define ROBOT_EYES_PAGES  6u

/* Render a complete frame at elapsed animation time (milliseconds).
 * Caller must wait until ssd1306_flush_poll() != I2C_BUSY before drawing. */
void robot_eyes_draw(uint32_t elapsed_ms);

#endif
