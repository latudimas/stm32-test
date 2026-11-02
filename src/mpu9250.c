/* MPU-9250 Register Map RM-MPU-9250A-00 rev 1.6:
 * Register names/addresses below follow its register descriptions.
 * No gyro/magnetometer acquisition.
 */
#include "mpu9250.h"
#include "i2c.h"
#include "systick.h"

#define WHO_AM_I       0x75u
#define PWR_MGMT_1     0x6Bu
#define PWR_MGMT_2     0x6Cu
#define SMPLRT_DIV     0x19u
#define CONFIG         0x1Au
#define ACCEL_CONFIG   0x1Cu
#define ACCEL_CONFIG2  0x1Du
#define INT_PIN_CFG    0x37u
#define INT_ENABLE     0x38u
#define INT_STATUS     0x3Au

static int write_reg(uint8_t addr, uint8_t reg, uint8_t value)
{
    if (i2c_write_poll() == I2C_BUSY) {
        return 0;
    }
    i2c_start();
    int ok = i2c_write_byte((uint8_t)(addr << 1)) &&
             i2c_write_byte(reg) && i2c_write_byte(value);
    i2c_stop();
    return ok;
}

int mpu9250_init(struct mpu9250 *dev)
{
    if (!dev || i2c_write_poll() == I2C_BUSY) {
        return 0;
    }
    dev->addr = 0;
    dev->who = 0;
    for (uint8_t addr = 0x68; addr <= 0x69; addr++) {
        uint8_t who;
        if (i2c_read_regs(addr, WHO_AM_I, &who, 1)) {
            dev->addr = addr;
            dev->who = who;
            if (who == 0x71) {
                break;
            }
        }
    }
    if (dev->who != 0x71) {
        return 0;
    }
    uint8_t addr = dev->addr;
    /* PWR_MGMT_1: clear sleep/cycle and use clock source 1. */
    if (!write_reg(addr, PWR_MGMT_1, 0x01)) {
        return 0;
    }
    delay_ms(100); /* power-up settling, outside the animation loop at boot */
    /* Standby gyro, enable all accel axes; 100 Hz internal rate, 41 Hz DLPF,
     * +/-2 g with self-test off. Poll data-ready; no physical INT connection. */
    if (!write_reg(addr, PWR_MGMT_2, 0x07) ||
        !write_reg(addr, CONFIG, 0x03) ||
        !write_reg(addr, SMPLRT_DIV, 9) ||
        !write_reg(addr, ACCEL_CONFIG, 0) ||
        !write_reg(addr, ACCEL_CONFIG2, 0x03) ||
        !write_reg(addr, INT_PIN_CFG, 0) ||
        !write_reg(addr, INT_ENABLE, 1)) {
        return 0;
    }
    /* Read back settings. Exercises hardware receiver endings of lengths
     * 1, 2 and 3 before the normal seven-byte status+acceleration bursts. */
    uint8_t power[2], rate[2], accel[3], interrupt[2];
    return i2c_read_regs(addr, PWR_MGMT_1, power, 2) &&
           i2c_read_regs(addr, SMPLRT_DIV, rate, 2) &&
           i2c_read_regs(addr, ACCEL_CONFIG, accel, 3) &&
           i2c_read_regs(addr, INT_PIN_CFG, interrupt, 2) &&
           interrupt[0] == 0 && interrupt[1] == 1 &&
           power[0] == 1 && power[1] == 7 && rate[0] == 9 && rate[1] == 3 &&
           accel[0] == 0 && accel[1] == 3;
}

static int16_t signed_word(const uint8_t *p)
{
    int32_t value = ((uint32_t)p[0] << 8) | p[1];
    if (value >= 32768) {
        value -= 65536;
    }
    return (int16_t)value;
}

int mpu9250_read(const struct mpu9250 *dev, struct mpu9250_sample *sample)
{
    if (!dev || !sample || dev->who != 0x71 ||
        (dev->addr != 0x68 && dev->addr != 0x69)) {
        return -1;
    }
    uint8_t data[7];
    /* INT_STATUS clears data-ready; the following six bytes are XYZ.
     * One burst keeps axis values coherent. Internal 100 Hz samples are not
     * queued: this bring-up stage deliberately reads latest-only at frame rate. */
    if (!i2c_read_regs(dev->addr, INT_STATUS, data, sizeof data)) {
        return -1;
    }
    if (!(data[0] & 1u)) {
        return 0;
    }
    sample->x = signed_word(&data[1]);
    sample->y = signed_word(&data[3]);
    sample->z = signed_word(&data[5]);
    sample->ms = millis();
    return 1;
}
