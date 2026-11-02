/* Host test: cc -std=c11 -Wall -Wextra -Werror -Isrc \
 * tests/robot_eyes_test.c src/robot_eyes.c -o /tmp/robot-eyes-test && \
 * /tmp/robot-eyes-test */
#include <assert.h>
#include <string.h>
#include "robot_eyes.h"
#include "ssd1306.h"

uint8_t ssd1306_fb[SSD1306_FB_SIZE];

void ssd1306_fill(uint8_t pattern)
{
    memset(ssd1306_fb, pattern, sizeof ssd1306_fb);
}

static void bounds(uint32_t ms, int expected_x, int expected_height)
{
    robot_eyes_draw(ms);
    int min_x = 128, max_x = -1, min_y = 64, max_y = -1;
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 128; x++) {
            if (ssd1306_fb[y / 8 * 128 + x] & (1u << (y % 8))) {
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
            }
        }
    }
    assert(min_x == 27 + expected_x);
    assert(max_x == 100 + expected_x);
    assert(max_y - min_y + 1 == expected_height);
    assert(min_y + max_y == 63);
}

int main(void)
{
    bounds(0, 0, 42);
    /* Open-eye caps are narrow at the tips and full-width in the middle,
     * with matching top/bottom curves and left/right eyes. */
    for (int y = 11; y < 53; y++) {
        for (int x = 27; x < 53; x++) {
            int lit = (ssd1306_fb[y / 8 * 128 + x] >> (y % 8)) & 1;
            int mirror_y = 63 - y;
            assert(lit == ((ssd1306_fb[mirror_y / 8 * 128 + x] >>
                            (mirror_y % 8)) & 1));
            assert(lit == ((ssd1306_fb[y / 8 * 128 + x + 48] >>
                            (y % 8)) & 1));
        }
    }
    int tip_pixels = 0;
    for (int x = 27; x < 53; x++) {
        tip_pixels += (ssd1306_fb[128 + x] >> 3) & 1; /* y = 11 */
        assert(ssd1306_fb[4 * 128 + x] & 1u); /* y = 32 */
    }
    assert(tip_pixels > 0 && tip_pixels <= 12);
    bounds(1190, 0, 2);
    bounds(2200, -14, 38);
    bounds(3800, 14, 38);
    bounds(5890, 0, 2);
    bounds(6100, 0, 42);
    bounds(6370, 0, 2);
    bounds(8000, 0, 42);

    uint8_t first[SSD1306_FB_SIZE];
    for (uint32_t ms = 0; ms < 8000; ms++) {
        robot_eyes_draw(ms);
        memcpy(first, ssd1306_fb, sizeof first);
        /* Periodicity and independence from previous framebuffer contents. */
        ssd1306_fill(0xFF);
        robot_eyes_draw(ms + 8000);
        assert(memcmp(first, ssd1306_fb, sizeof first) == 0);
    }
    robot_eyes_draw(UINT32_MAX);
    return 0;
}
