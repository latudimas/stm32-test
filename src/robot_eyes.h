/* Grok-inspired robot face: two rounded eyes, glances and eyelid blinks. */
#ifndef ROBOT_EYES_H
#define ROBOT_EYES_H

#include <stdint.h>
#include "motion.h"
#include "ssd1306.h"

/* Fixed page-aligned window enclosing every pose, including old positions.
 * Update these bounds if the eye dimensions or travel change. */
#define ROBOT_EYES_X     13u
#define ROBOT_EYES_PAGE   1u
#define ROBOT_EYES_WIDTH 102u
#define ROBOT_EYES_PAGES  6u

/* Render a complete frame at elapsed animation time (milliseconds).
 * Caller must wait until ssd1306_flush_poll() != I2C_BUSY before drawing. */
void robot_eyes_draw(uint32_t elapsed_ms);

/* Stateful renderer: smooth 180 ms ordered cross-dissolves on state changes.
 * Keep the context across frames; same wait-before-drawing rule as above. */
struct robot_eyes {
    enum motion_state state;
    uint32_t last_ms, elapsed_ms, transition_ms;
    uint8_t transitioning;
    uint8_t previous[SSD1306_FB_SIZE];
};
void robot_eyes_init(struct robot_eyes *eyes, uint32_t now);
void robot_eyes_animate(struct robot_eyes *eyes, enum motion_state state, uint32_t now);

#endif
