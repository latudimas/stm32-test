/* Host test: link with src/ssd1306.c and src/font5x7.c, -Isrc. */
#include <assert.h>
#include <string.h>
#include "ssd1306.h"
#include "i2c.h"

static int status = I2C_DONE;
static int ack = 1;
static uint8_t bytes[32];
static uint32_t count, payload_len;
static const uint8_t *payload;

void i2c_start(void) {}
void i2c_stop(void) {}
int i2c_write_byte(uint8_t b)
{
    assert(count < sizeof bytes);
    bytes[count++] = b;
    return ack;
}
void i2c_write_async(const uint8_t *buf, uint32_t len)
{
    payload = buf;
    payload_len = len;
    status = I2C_BUSY;
}
int i2c_write_poll(void) { return status; }

int main(void)
{
    for (uint32_t i = 0; i < SSD1306_FB_SIZE; i++) {
        ssd1306_fb[i] = (uint8_t)(i * 7u + i / 128u);
    }
    assert(ssd1306_flush_region_start(13, 1, 102, 6));
    const uint8_t expected[] = {0x78, 0, 0x21, 13, 114, 0x22, 1, 6, 0x78, 0x40};
    assert(count == sizeof expected);
    assert(memcmp(bytes, expected, sizeof expected) == 0);
    assert(payload_len == 612);
    for (uint32_t p = 0; p < 6; p++) {
        assert(memcmp(payload + p * 102, ssd1306_fb + (p + 1) * 128 + 13, 102) == 0);
    }
    uint8_t saved[612];
    memcpy(saved, payload, sizeof saved);
    /* Rejected calls must neither overwrite staging memory nor touch I2C. */
    ssd1306_fill(0xFF);
    assert(!ssd1306_flush_region_start(0, 0, 128, 8));
    assert(!ssd1306_flush_start());
    assert(memcmp(payload, saved, sizeof saved) == 0);
    assert(count == sizeof expected);
    status = I2C_DONE;
    assert(!ssd1306_flush_region_start(128, 0, 1, 1));
    assert(!ssd1306_flush_region_start(0, 8, 1, 1));
    assert(!ssd1306_flush_region_start(0, 0, 0, 1));
    assert(!ssd1306_flush_region_start(0, 0, 1, 0));
    assert(!ssd1306_flush_region_start(13, 1, UINT32_MAX, 1));
    assert(!ssd1306_flush_region_start(13, 1, 1, UINT32_MAX));
    assert(count == sizeof expected);
    count = 0;
    assert(ssd1306_flush_region_start(127, 7, 1, 1));
    assert(payload_len == 1 && payload[0] == 0xFF);
    status = I2C_ERR;
    count = 0;
    assert(ssd1306_flush_start());
    assert(payload == ssd1306_fb && payload_len == 1024);
    status = I2C_DONE;
    count = 0;
    ack = 0;
    assert(!ssd1306_flush_region_start(13, 1, 102, 6));
    assert(status == I2C_DONE);
    return 0;
}
