/* MPU9250 accelerometer acquisition; classification lives in motion.h. */
#ifndef MPU9250_H
#define MPU9250_H
#include <stdint.h>

struct mpu9250 {
    uint8_t addr;
    uint8_t who;
};
struct mpu9250_sample {
    uint32_t ms;
    int16_t x, y, z; /* uncalibrated raw counts, nominally 16384 counts/g */
};

/* Thread mode, only when the shared I2C bus is free. Init may wait 100 ms
 * after wake-up. Accepts WHO_AM_I=0x71 only; does not enable the magnetometer. */
int mpu9250_init(struct mpu9250 *dev);
/* 1 = new sample, 0 = no data ready, -1 = transfer error. Output is unchanged
 * unless a complete fresh sample was read. Timestamp is read-completion time. */
int mpu9250_read(const struct mpu9250 *dev, struct mpu9250_sample *sample);
#endif
