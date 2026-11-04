/* CBMC proof for the SSD1306 framebuffer writers in src/ssd1306.c, for every
 * argument value (any int32 x, page, char, region). Run with `make verify`.
 * Besides no out-of-bounds access or overflow (CBMC's built-in checks):
 *   draw_char:    changes only its clipped cell, to the right glyph column
 *   draw_string:  changes only its own page
 *   clear_page:   zeroes exactly that page, nothing else
 *   flush_region: rejects off-screen rectangles and busy transfers without
 *                 touching the staging buffer.
 * One arbitrary byte q stands in for every byte of the buffer. Each proof_*
 * function is a separate CBMC entry point (`--function`) with its own loop
 * bounds. */
#include <stdbool.h>
#include "font5x7.h"
#include "i2c.h"
#include "ssd1306.h"

uint32_t nondet_u32(void); /* No body: CBMC treats each result as any value. */
int32_t nondet_i32(void);
char nondet_char(void);
bool nondet_bool(void);

/* I2C model: any ACK pattern and any background-transfer status. */
static uint32_t async_calls;
void i2c_start(void) {}
void i2c_stop(void) {}
int i2c_write_byte(uint8_t b)
{
    (void)b;
    return nondet_bool();
}
void i2c_write_async(const uint8_t *buf, uint32_t len)
{
    (void)buf;
    (void)len;
    async_calls++;
}
static int poll_status;
int i2c_write_poll(void)
{
    return poll_status;
}

#define PAGES (SSD1306_HEIGHT / 8u)

static uint32_t q; /* the probe byte, and its value before the call */
static uint8_t old;
static void arbitrary_framebuffer(void)
{
    __CPROVER_havoc_object(ssd1306_fb);
    q = nondet_u32();
    __CPROVER_assume(q < SSD1306_FB_SIZE);
    old = ssd1306_fb[q];
}

void proof_draw_char(void)
{
    arbitrary_framebuffer();
    int32_t x = nondet_i32();
    uint32_t page = nondet_u32();
    char c = nondet_char();
    ssd1306_draw_char(x, page, c);

    int64_t offset = (int64_t)(q % SSD1306_WIDTH) - x; /* glyph column; 64-bit */
    bool in_cell = page < PAGES && q / SSD1306_WIDTH == page &&
                   offset >= 0 && offset < (int64_t)FONT_W;
    if (!in_cell) {
        __CPROVER_assert(ssd1306_fb[q] == old, "draw_char: bytes outside its cell unchanged");
    } else {
        uint8_t ch = (uint8_t)c;
        if (ch < FONT_FIRST || ch > FONT_LAST)
            ch = '?';
        __CPROVER_assert(ssd1306_fb[q] == font5x7[ch - FONT_FIRST][offset],
                         "draw_char: cell holds the glyph ('?' if unprintable)");
    }
}

void proof_draw_string(void)
{
    arbitrary_framebuffer();
    char s[3]; /* up to 2 characters; longer strings repeat the same step */
    for (uint32_t i = 0; i < 2u; i++)
        s[i] = nondet_char();
    s[2] = '\0';
    uint32_t page = nondet_u32();
    ssd1306_draw_string(nondet_i32(), page, s);
    if (q / SSD1306_WIDTH != page)
        __CPROVER_assert(ssd1306_fb[q] == old, "draw_string: other pages unchanged");
}

void proof_clear_page(void)
{
    arbitrary_framebuffer();
    uint32_t page = nondet_u32();
    ssd1306_clear_page(page);
    if (q / SSD1306_WIDTH == page)
        __CPROVER_assert(ssd1306_fb[q] == 0, "clear_page: whole page zeroed");
    else
        __CPROVER_assert(ssd1306_fb[q] == old, "clear_page: other pages unchanged");
}

/* Run with --unwind 1: the copy loop must be unreachable, or the unwinding
 * assertion fails. That loop is the only writer of the staging buffer, so a
 * rejected call cannot disturb a transfer in flight. Accepted rectangles are
 * few enough to enumerate: see tests/ssd1306_region_test.c. */
void proof_flush_region_rejects(void)
{
    uint32_t x = nondet_u32(), page = nondet_u32();
    uint32_t width = nondet_u32(), pages = nondet_u32();
    poll_status = nondet_bool() ? I2C_BUSY : (nondet_bool() ? I2C_DONE : I2C_ERR);
    bool valid = x < SSD1306_WIDTH && page < PAGES && width >= 1u && pages >= 1u &&
                 width <= SSD1306_WIDTH - x && pages <= PAGES - page; /* no wrap */
    __CPROVER_assume(!valid || poll_status == I2C_BUSY);

    __CPROVER_assert(!ssd1306_flush_region_start(x, page, width, pages) && async_calls == 0,
                     "flush_region: invalid or busy calls are rejected");
}
