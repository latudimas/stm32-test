/*
 * SSD1306 driver. Command reference: SSD1306 datasheet §9 "Command Table" and
 * §10 "Command Descriptions"; init sequence follows the datasheet's
 * "Software Initialization" flow chart plus the charge-pump app note
 * (internal charge pump: 8Dh 14h before AFh).
 */
#include "ssd1306.h"
#include "font5x7.h"
#include "i2c.h"

/* I2C control byte (datasheet §8.1.5.2): Co=0 (no more control bytes follow,
 * the rest of the transfer is a stream), D/C# selects command or GDDRAM data. */
#define CTRL_CMD  0x00u
#define CTRL_DATA 0x40u

uint8_t ssd1306_fb[SSD1306_FB_SIZE];

/* One I2C transaction: START, address+W, control byte, payload, STOP.
 * Stops at the first NACK. */
static int send(uint8_t control, const uint8_t *buf, uint32_t len)
{
    i2c_start();
    int ok = i2c_write_byte((uint8_t)(SSD1306_ADDR << 1)) && i2c_write_byte(control);
    for (uint32_t i = 0; ok && i < len; i++) {
        ok = i2c_write_byte(buf[i]);
    }
    i2c_stop();
    return ok;
}

static const uint8_t init_cmds[] = {
    0xAE,       /* display OFF while configuring */
    0xD5, 0x80, /* display clock: divide ratio 1, oscillator freq default */
    0xA8, 0x3F, /* multiplex ratio: 64 rows (63 = 0x3F) */
    0xD3, 0x00, /* display offset: 0 */
    0x40,       /* display start line: 0 */
    0x8D, 0x14, /* charge pump: enable (module has no external VCC supply) */
    0x20, 0x00, /* memory addressing mode: horizontal (auto-wrap to next page) */
    0xA1,       /* segment remap: column 127 -> SEG0 (un-mirrors X on these modules) */
    0xC8,       /* COM scan direction: remapped, COM63 -> COM0 (un-flips Y) */
    0xDA, 0x12, /* COM pins: alternative config, no left/right remap (128x64 panels) */
    0x81, 0xCF, /* contrast */
    0xD9, 0xF1, /* pre-charge period: phase1 = 1, phase2 = 15 DCLK (charge pump) */
    0xDB, 0x40, /* VCOMH deselect level */
    0xA4,       /* display follows GDDRAM (A5 would force all pixels on) */
    0xA6,       /* normal, not inverted */
    0xAF,       /* display ON */
};

int ssd1306_init(void)
{
    return send(CTRL_CMD, init_cmds, sizeof init_cmds);
}

void ssd1306_fill(uint8_t pattern)
{
    for (uint32_t i = 0; i < SSD1306_FB_SIZE; i++) {
        ssd1306_fb[i] = pattern;
    }
}

int ssd1306_flush_start(void)
{
    /* Reset the write window to the full screen so the 1024 bytes land at
     * column 0, page 0 and wrap through all 8 pages (horizontal mode). */
    static const uint8_t window[] = {
        0x21, 0, SSD1306_WIDTH - 1,         /* column address: 0..127 */
        0x22, 0, SSD1306_HEIGHT / 8u - 1,   /* page address: 0..7 */
    };
    if (!send(CTRL_CMD, window, sizeof window)) {
        return 0;
    }

    /* Header bytes go out synchronously; the 1024-byte body is handed to
     * i2c_write_async(), which also sends the STOP when it's done. */
    i2c_start();
    if (!i2c_write_byte((uint8_t)(SSD1306_ADDR << 1)) || !i2c_write_byte(CTRL_DATA)) {
        i2c_stop();
        return 0;
    }
    i2c_write_async(ssd1306_fb, SSD1306_FB_SIZE);
    return 1;
}

int ssd1306_flush_poll(void)
{
    return i2c_write_poll();
}

int ssd1306_flush(void)
{
    if (!ssd1306_flush_start()) {
        return 0;
    }
    int r;
    while ((r = ssd1306_flush_poll()) == I2C_BUSY) {
    }
    return r == I2C_DONE;
}

void ssd1306_draw_char(uint32_t x, uint32_t page, char c)
{
    if (page >= SSD1306_HEIGHT / 8u) {
        return;
    }
    uint8_t ch = (uint8_t)c;
    if (ch < FONT_FIRST || ch > FONT_LAST) {
        ch = '?';
    }
    const uint8_t *glyph = font5x7[ch - FONT_FIRST];

    /* A glyph column and a framebuffer byte have the same layout (bit 0 = top),
     * so drawing on a page boundary is a straight copy. */
    uint8_t *row = &ssd1306_fb[page * SSD1306_WIDTH];
    for (uint32_t col = 0; col < FONT_W && x + col < SSD1306_WIDTH; col++) {
        row[x + col] = glyph[col];
    }
}

void ssd1306_draw_string(uint32_t x, uint32_t page, const char *s)
{
    /* 5 columns of glyph + 1 blank column between characters. */
    for (; *s && x < SSD1306_WIDTH; s++, x += FONT_W + 1u) {
        ssd1306_draw_char(x, page, *s);
    }
}
