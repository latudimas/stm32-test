/* Host test: cc -std=c11 -Wall -Wextra -Werror -Isrc \
 * tests/mpu9250_test.c src/mpu9250.c -o /tmp/mpu9250-test */
#include <assert.h>
#include <string.h>
#include "mpu9250.h"
#include "i2c.h"

static uint8_t regs[2][256];
static int busy, nack, read_fail, calls, writes;
static uint8_t tx_addr, tx_reg;
static unsigned tx_count;
static uint32_t now;

uint32_t millis(void) { return now; }
void delay_ms(uint32_t ms) { now += ms; }
int i2c_write_poll(void) { return busy ? I2C_BUSY : I2C_DONE; }
void i2c_start(void) { tx_count = 0; }
void i2c_stop(void) {}
int i2c_write_byte(uint8_t b)
{
    if (nack) return 0;
    if (tx_count == 0) tx_addr = (uint8_t)((b >> 1) - 0x68);
    else if (tx_count == 1) tx_reg = b;
    else { assert(tx_addr < 2); regs[tx_addr][tx_reg++] = b; writes++; }
    tx_count++;
    return 1;
}
int i2c_read_regs(uint8_t addr, uint8_t reg, uint8_t *buf, uint32_t len)
{
    calls++;
    if (busy || read_fail) {
        if (len) buf[0] = 0xFF; /* simulate partial read corruption */
        return 0;
    }
    assert(addr == 0x68 || addr == 0x69);
    assert(len && reg + len <= 256);
    memcpy(buf, &regs[addr - 0x68][reg], len);
    return 1;
}

int main(void)
{
    struct mpu9250 dev = {0};
    assert(!mpu9250_init(&dev)); /* unknown identity: no writes */
    assert(writes == 0);
    regs[0][0x75] = 0x70; /* do not silently accept MPU6500 */
    assert(!mpu9250_init(&dev) && writes == 0);
    regs[1][0x75] = 0x71;
    assert(mpu9250_init(&dev) && dev.addr == 0x69 && dev.who == 0x71);
    assert(regs[1][0x6b] == 1 && regs[1][0x6c] == 7);
    assert(regs[1][0x19] == 9 && regs[1][0x1c] == 0 && regs[1][0x1d] == 3);
    assert(regs[1][0x37] == 0 && regs[1][0x38] == 1);

    struct mpu9250_sample sample;
    memset(&sample, 0xA5, sizeof sample);
    struct mpu9250_sample before;
    memcpy(&before, &sample, sizeof sample);
    assert(mpu9250_read(&dev, &sample) == 0); /* not ready */
    assert(memcmp(&sample, &before, sizeof sample) == 0);
    read_fail = 1;
    assert(mpu9250_read(&dev, &sample) == -1);
    assert(memcmp(&sample, &before, sizeof sample) == 0);
    read_fail = 0;
    const uint8_t data[] = {1, 0x80, 0x00, 0x7F, 0xFF, 0xFF, 0xFF};
    memcpy(&regs[1][0x3a], data, sizeof data);
    now = UINT32_MAX;
    assert(mpu9250_read(&dev, &sample) == 1);
    assert(sample.x == -32768 && sample.y == 32767 && sample.z == -1);
    assert(sample.ms == UINT32_MAX);
    assert(mpu9250_read(0, &sample) == -1);
    assert(mpu9250_read(&dev, 0) == -1);
    assert(!mpu9250_init(0));
    busy = 1;
    int old_calls = calls;
    assert(!mpu9250_init(&dev) && calls == old_calls);
    busy = 0;
    nack = 1;
    assert(!mpu9250_init(&dev));
    nack = 0;
    regs[0][0x75] = 0x71;
    assert(mpu9250_init(&dev) && dev.addr == 0x68);
    return 0;
}
