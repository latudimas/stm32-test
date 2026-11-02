/* Grok-inspired robot face: two rounded eyes, glances and eyelid blinks. */
#ifndef ROBOT_EYES_H
#define ROBOT_EYES_H

#include <stdint.h>

/* Render a complete frame at elapsed animation time (milliseconds).
 * Caller must wait until ssd1306_flush_poll() != I2C_BUSY before drawing. */
void robot_eyes_draw(uint32_t elapsed_ms);

#endif
