/*
 * 5x7 bitmap font, printable ASCII 0x20..0x7E.
 * Column-major: 5 bytes per glyph, one per column, bit 0 = top row, bit 7
 * unused (blank row). Same orientation as an SSD1306 page byte, so a glyph
 * column can be copied straight into the framebuffer.
 */
#ifndef FONT5X7_H
#define FONT5X7_H

#include <stdint.h>

#define FONT_FIRST 0x20u
#define FONT_LAST  0x7Eu
#define FONT_W     5u

extern const uint8_t font5x7[FONT_LAST - FONT_FIRST + 1u][FONT_W];

#endif
