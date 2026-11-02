#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "robot_eyes.h"

uint8_t ssd1306_fb[SSD1306_FB_SIZE];
void ssd1306_fill(uint8_t pattern) { memset(ssd1306_fb, pattern, sizeof ssd1306_fb); }

static int lit(const uint8_t *fb, unsigned x, unsigned y)
{
    return (fb[y / 8 * SSD1306_WIDTH + x] >> (y & 7)) & 1;
}
static void check_window(void)
{
    unsigned count = 0;
    for (unsigned y = 0; y < SSD1306_HEIGHT; y++) {
        for (unsigned x = 0; x < SSD1306_WIDTH; x++) {
            if (lit(ssd1306_fb, x, y)) {
                assert(x >= ROBOT_EYES_X && x < ROBOT_EYES_X + ROBOT_EYES_WIDTH);
                assert(y >= ROBOT_EYES_PAGE * 8 && y < (ROBOT_EYES_PAGE + ROBOT_EYES_PAGES) * 8);
                count++;
            }
        }
    }
    assert(count > 0);
}

int main(int argc, char **argv)
{
    struct robot_eyes eyes;
    uint8_t saved[SSD1306_FB_SIZE];
    for (enum motion_state state = MOTION_DEFAULT; state <= MOTION_CONFUSED; state++) {
        robot_eyes_draw(0);
        robot_eyes_init(&eyes, 0);
        for (unsigned ms = 0; ms < 8000; ms += 8) {
            robot_eyes_animate(&eyes, state, ms);
            check_window();
        }
    }
    robot_eyes_init(&eyes, 0);
    robot_eyes_animate(&eyes, MOTION_DEFAULT, 0);
    memcpy(saved, ssd1306_fb, sizeof saved);
    assert(lit(saved, 40, 11));
    robot_eyes_animate(&eyes, MOTION_CONFUSED, 0);
    assert(memcmp(saved, ssd1306_fb, sizeof saved) == 0); /* no jump at transition start */
    robot_eyes_animate(&eyes, MOTION_CONFUSED, 180);
    assert(!lit(ssd1306_fb, 40, 11)); /* no tall rounded eye behind the spiral */
    memcpy(saved, ssd1306_fb, sizeof saved);
    robot_eyes_animate(&eyes, MOTION_CONFUSED, 1180);
    assert(memcmp(saved, ssd1306_fb, sizeof saved) == 0); /* one revolution/second */
    unsigned different = 0;
    for (unsigned ms = 1196; ms < 2196; ms += 16) {
        robot_eyes_animate(&eyes, MOTION_CONFUSED, ms);
        if (memcmp(saved, ssd1306_fb, sizeof saved)) different++;
        memcpy(saved, ssd1306_fb, sizeof saved);
    }
    assert(different >= 55); /* true intermediate rotation phases, not a tiny sprite loop */

    for (unsigned phase_ms = 125; phase_ms < 1000; phase_ms += 125) {
        robot_eyes_init(&eyes, 0);
        robot_eyes_animate(&eyes, MOTION_CONFUSED, 0);
        robot_eyes_animate(&eyes, MOTION_CONFUSED, 1000 + phase_ms);
        memcpy(saved, ssd1306_fb, sizeof saved);
        robot_eyes_animate(&eyes, MOTION_CONFUSED, 3000 - phase_ms);
        for (unsigned y = 8; y < 56; y++) {
            for (unsigned x = 20; x < 61; x++) {
                assert(lit(saved, x, y) == lit(ssd1306_fb, x + 48, y));
            }
        }
    }

    /* Interrupted transitions, return-home phase reset and millis wrap. */
    uint32_t start = UINT32_MAX - 100;
    robot_eyes_init(&eyes, start);
    robot_eyes_animate(&eyes, MOTION_DEFAULT, start);
    robot_eyes_animate(&eyes, MOTION_SQUINTING, start + 10);
    robot_eyes_animate(&eyes, MOTION_SQUINTING, start + 80);
    memcpy(saved, ssd1306_fb, sizeof saved);
    robot_eyes_animate(&eyes, MOTION_CONFUSED, start + 80);
    assert(memcmp(saved, ssd1306_fb, sizeof saved) == 0);
    robot_eyes_animate(&eyes, MOTION_CONFUSED, start + 300);
    check_window();
    robot_eyes_animate(&eyes, MOTION_DEFAULT, start + 400);
    robot_eyes_animate(&eyes, MOTION_DEFAULT, start + 580);
    memcpy(saved, ssd1306_fb, sizeof saved);
    robot_eyes_draw(180);
    assert(memcmp(saved, ssd1306_fb, sizeof saved) == 0);

    /* Optional three-panel PGM preview: default / squint / dizzy. */
    if (argc == 2) {
        uint8_t panels[3][SSD1306_FB_SIZE];
        for (unsigned state = 0; state < 3; state++) {
            robot_eyes_draw(0);
            robot_eyes_init(&eyes, 0);
            robot_eyes_animate(&eyes, (enum motion_state)state, 0);
            robot_eyes_animate(&eyes, (enum motion_state)state, 200);
            memcpy(panels[state], ssd1306_fb, SSD1306_FB_SIZE);
        }
        FILE *f = fopen(argv[1], "wb");
        assert(f);
        fprintf(f, "P5\n384 64\n255\n");
        for (unsigned y = 0; y < 64; y++) {
            for (unsigned panel = 0; panel < 3; panel++) {
                for (unsigned x = 0; x < 128; x++) {
                    fputc(lit(panels[panel], x, y) ? 255 : 0, f);
                }
            }
        }
        fclose(f);
    }
    return 0;
}
