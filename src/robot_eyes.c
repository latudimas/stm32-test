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
    {   0,   0, 42},
    {1100,   0, 42},
    {1190,   0,  2},
    {1250,   0,  2},
    {1400,   0, 42},
    {1900,   0, 42},
    {2200, -14, 38},
    {3300, -14, 38},
    {3800,  14, 38},
    {4900,  14, 38},
    {5200,   0, 42},
    {5800,   0, 42},
    {5890,   0,  2},
    {5950,   0,  2},
    {6100,   0, 42},
    {6280,   0, 42},
    {6370,   0,  2},
    {6430,   0,  2},
    {6580,   0, 42},
    {8000,   0, 42},
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
    const int32_t width = 26;
    /* Half-width rounding makes open eyes capsule-shaped; limit by height
     * so the same shape still squashes cleanly into a closed eyelid. */
    int32_t radius = height / 2;
    if (radius > width / 2) {
        radius = width / 2;
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

static void rounded_eyes(uint32_t elapsed_ms, int squint)
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
    if (squint) {
        height = 2 + (height - 2) * 10 / 40; /* 12 px open, 2 px blink */
        x /= 3; /* small searching glances, not the default's full travel */
    }

    ssd1306_fill(0);
    eye(40 + x, height);
    eye(88 + x, height);
}

void robot_eyes_draw(uint32_t elapsed_ms)
{
    rounded_eyes(elapsed_ms, 0);
}

/* Quarter-wave sine, Q8, 128 angular steps/revolution. Generated from
 * round(256*sin(i*pi/64)), i=0..32. No runtime floating point or trig library. */
static const int16_t sine_quarter[] = {
    0, 13, 25, 38, 50, 62, 74, 86, 98, 109, 121, 132, 142, 152, 162, 172,
    181, 190, 198, 206, 213, 220, 226, 231, 237, 241, 245, 248, 251, 253,
    255, 256, 256,
};

static int32_t sine(uint32_t angle)
{
    angle &= 127u;
    uint32_t i = angle & 63u;
    int32_t value = sine_quarter[i <= 32u ? i : 64u - i];
    return angle < 64u ? value : -value;
}

static void stroke(int32_t x, int32_t y)
{
    pixel(x, y);
    pixel(x + 1, y);
    pixel(x, y + 1);
    pixel(x + 1, y + 1);
}

static void line(int32_t x, int32_t y, int32_t end_x, int32_t end_y)
{
    int32_t dx = end_x > x ? end_x - x : x - end_x;
    int32_t dy = end_y > y ? y - end_y : end_y - y;
    int32_t sx = x < end_x ? 1 : -1, sy = y < end_y ? 1 : -1;
    int32_t error = dx + dy;
    for (;;) {
        stroke(x, y);
        if (x == end_x && y == end_y) {
            break;
        }
        int32_t twice = 2 * error;
        if (twice >= dy) {
            error += dy;
            x += sx;
        }
        if (twice <= dx) {
            error += dx;
            y += sy;
        }
    }
}

static void spiral(int32_t cx, uint32_t phase)
{
    int32_t previous_x = cx + sine(phase + 32u) / 256;
    int32_t previous_y = 31 + sine(phase) / 256;
    /* Two Archimedean turns, radius 1..18 px; joined with a 2-pixel stroke.
     * All rotations fit in the existing 612-byte OLED update rectangle. */
    for (uint32_t k = 1; k <= 128u; k++) {
        int32_t radius_q8 = 256 + 34 * (int32_t)k;
        uint32_t angle = phase + 2u * k;
        int32_t x = cx + radius_q8 * sine(angle + 32u) / 65536;
        int32_t y = 31 + radius_q8 * sine(angle) / 65536;
        line(previous_x, previous_y, x, y);
        previous_x = x;
        previous_y = y;
    }
}

void robot_eyes_init(struct robot_eyes *eyes, uint32_t now)
{
    eyes->state = MOTION_DEFAULT;
    eyes->last_ms = now;
    eyes->elapsed_ms = 0;
    eyes->transition_ms = now;
    eyes->transitioning = 0;
}

void robot_eyes_animate(struct robot_eyes *eyes, enum motion_state state, uint32_t now)
{
    if (state != MOTION_SQUINTING && state != MOTION_CONFUSED) {
        state = MOTION_DEFAULT;
    }
    if (state != eyes->state) {
        /* Snapshot the visible pose, including any interrupted transition. */
        for (uint32_t i = 0; i < SSD1306_FB_SIZE; i++) {
            eyes->previous[i] = ssd1306_fb[i];
        }
        eyes->state = state;
        eyes->elapsed_ms = 0;
        eyes->transition_ms = now;
        eyes->transitioning = 1;
    } else {
        /* 8000 is also a multiple of the one-second spiral period. */
        eyes->elapsed_ms = (eyes->elapsed_ms + (uint32_t)(now - eyes->last_ms)) % 8000u;
    }
    eyes->last_ms = now;
    if (state == MOTION_CONFUSED) {
        ssd1306_fill(0); /* spirals replace the eyes entirely, with no outlines */
        uint32_t phase = (eyes->elapsed_ms % 1000u) * 128u / 1000u;
        spiral(40, phase);
        spiral(88, (128u - phase) & 127u); /* opposite rotation */
    } else {
        rounded_eyes(eyes->elapsed_ms, state == MOTION_SQUINTING);
    }

    uint32_t elapsed = now - eyes->transition_ms;
    if (eyes->transitioning && elapsed < 180u) {
        /* Ordered dissolve is a monochrome crossfade without flashing the
         * whole screen. Page height is a multiple of the 4x4 Bayer pattern. */
        static const uint8_t bayer[4][4] = {
            {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5},
        };
        uint32_t step = elapsed * 16u / 180u;
        uint8_t masks[4] = {0, 0, 0, 0};
        for (uint32_t x = 0; x < 4; x++) {
            for (uint32_t bit = 0; bit < 8; bit++) {
                if (bayer[bit & 3u][x] < step) {
                    masks[x] |= (uint8_t)(1u << bit);
                }
            }
        }
        for (uint32_t i = 0; i < SSD1306_FB_SIZE; i++) {
            uint8_t mask = masks[i & 3u];
            ssd1306_fb[i] = (ssd1306_fb[i] & mask) | (eyes->previous[i] & (uint8_t)~mask);
        }
    } else {
        eyes->transitioning = 0;
    }
}
