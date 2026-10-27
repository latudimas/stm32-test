/*
 * I2C master on PB6 (SCL) / PB7 (SDA). Two interchangeable implementations,
 * chosen in the Makefile (I2C=hw or I2C=bitbang):
 *   i2c_hw.c       hardware I2C1, 400 kHz fast mode
 *   i2c_bitbang.c  GPIO bit-banging, ~100 kHz standard mode
 * Relies on the OLED module's onboard pull-ups.
 */
#ifndef I2C_H
#define I2C_H

#include <stdint.h>

extern const char i2c_driver[]; /* human-readable name of the linked driver */

void i2c_init(void);           /* needs systick_init() first (timeouts / delays) */
int i2c_bus_idle(void);        /* 1 if the bus is free */
void i2c_start(void);
void i2c_stop(void);
int i2c_write_byte(uint8_t b); /* returns 1 on ACK, 0 on NACK/error */
int i2c_probe(uint8_t addr7);  /* START, addr+W, STOP; 1 if a device ACKs */

#endif
