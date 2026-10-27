/*
 * Bit-banged I2C master on PB6 (SCL) / PB7 (SDA), standard mode (<100 kHz).
 * Relies on the OLED module's onboard pull-ups.
 */
#ifndef I2C_H
#define I2C_H

#include <stdint.h>

void i2c_init(void);
int i2c_bus_idle(void);        /* 1 if SCL and SDA both read high */
void i2c_start(void);
void i2c_stop(void);
int i2c_write_byte(uint8_t b); /* returns 1 on ACK, 0 on NACK */
int i2c_probe(uint8_t addr7);  /* START, addr+W, STOP; 1 if a device ACKs */

#endif
