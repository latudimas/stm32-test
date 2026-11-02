#include "robot_eyes.h"
#include "ssd1306.h"

/* An eight-second loop: settle, blink, glance left, glance right, double
 * blink, return home. Time-based poses keep the speed independent of I2C.
 * Closed eyes remain visible as two thin horizontal eyelid lines. */
struct pose {
    uint16_t ms;
    int8_t x;
    uint8_t height;
};

static const struct pose poses[] = {
    {   0,   0, 36},
    {1100,   0, 36},
    {1190,   0,  2},
    {1250,   0,  2},
    {1400,   0, 36},
    {1900,   0, 36},
    {2200, -14, 32},
    {3300, -14, 32},
    {3800,  14, 32},
    {4900,  14, 32},
    {5200,   0, 36},
    {5800,   0, 36},
    {5890,   0,  2},
    {5950,   0,  2},
    {6100,   0, 36},
    {6280,   0, 36},
    {6370,   0,  2},
    {6430,   0,  2},
    {6580,   0, 36},
    {8000,   0, 36},
};

/* Integer smoothstep (0..256): ease into and out of each pose, no floats. */
static int32_t blend(int32_t a, int32_t b, uint32_t t)
{
    int32_t u = (int32_t)t;
    int32_t eased = u * u * (768 - 2 * u) / 65536;
    return a + (b - a) * eased / 256;
}

static void pixel(int32_t x, int32_t y)
{
    if (x >= 0 && x < (int32_t)SSD1306_WIDTH &&
        y >= 0 && y < (int32_t)SSD1306_HEIGHT) {
        ssd1306_fb[(uint32_t)y / 8u * SSD1306_WIDTH + (uint32_t)x] |=
            (uint8_t)(1u << ((uint32_t)y & 7u));
    }
}

static void eye(int32_t cx, int32_t height)
{
    const int32_t width = 30;
    int32_t radius = height / 2;
    if (radius > 8) {
        radius = 8;
    }
    int32_t left = cx - width / 2;
    int32_t top = ((int32_t)SSD1306_HEIGHT - height) / 2;

    for (int32_t y = 0; y < height; y++) {
        for (int32_t x = 0; x < width; x++) {
            /* Distance from the central cross, rounded off at the corners. */
            int32_t dx = x < radius ? radius - 1 - x :
                         (x >= width - radius ? x - (width - radius) : 0);
            int32_t dy = y < radius ? radius - 1 - y :
                         (y >= height - radius ? y - (height - radius) : 0);
            if (dx * dx + dy * dy <= radius * radius) {
                pixel(left + x, top + y);
            }
        }
    }
}

void robot_eyes_draw(uint32_t elapsed_ms)
{
    uint32_t phase = elapsed_ms % 8000u;
    uint32_t i = 0;
    while (phase >= poses[i + 1u].ms) {
        i++;
    }
    const struct pose *a = &poses[i];
    const struct pose *b = &poses[i + 1u];
    uint32_t t = (phase - a->ms) * 256u / (b->ms - a->ms);
    int32_t x = blend(a->x, b->x, t);
    int32_t height = blend(a->height, b->height, t);

    ssd1306_fill(0);
    eye(40 + x, height);
    eye(88 + x, height);
}
