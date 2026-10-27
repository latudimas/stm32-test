/*
 * Bit-banged I2C. Lines are open-drain: writing 1 releases the line (the
 * pull-up makes it high), writing 0 pulls it low. Timing per I2C spec
 * (UM10204) standard mode; we run well below 100 kHz.
 */
#include "i2c.h"
#include "stm32f103.h"
#include "systick.h"

#define SCL_PIN 6u
#define SDA_PIN 7u

/* Half bit period. 5 us -> SCL <= 100 kHz (standard mode: tLOW >= 4.7 us,
 * tHIGH >= 4.0 us); call overhead makes it a bit slower in practice. */
#define I2C_HALF_US 5u

/* RM0008 §9.2.5 GPIOB_BSRR: BSy (bit y) sets ODR -> line released (high via
 * pull-up); BRy (bit y+16) clears ODR -> line driven low. */
static void scl_high(void) { GPIO_BSRR(GPIOB_BASE) = 1u << SCL_PIN; }
static void scl_low(void)  { GPIO_BSRR(GPIOB_BASE) = 1u << (SCL_PIN + 16u); }
static void sda_high(void) { GPIO_BSRR(GPIOB_BASE) = 1u << SDA_PIN; }
static void sda_low(void)  { GPIO_BSRR(GPIOB_BASE) = 1u << (SDA_PIN + 16u); }

/* RM0008 §9.2.3 GPIOB_IDR. In open-drain output mode the input Schmitt trigger
 * stays active, so IDR shows the real line level (§9.1.8). */
static int sda_read(void) { return (GPIO_IDR(GPIOB_BASE) >> SDA_PIN) & 1u; }
static int scl_read(void) { return (GPIO_IDR(GPIOB_BASE) >> SCL_PIN) & 1u; }

void i2c_init(void)
{
    /* RM0008 §7.3.7 RCC_APB2ENR: IOPBEN=1 — clock GPIOB. */
    RCC_APB2ENR |= RCC_APB2ENR_IOPBEN;

    /* Release both lines before switching to output, so the bus never glitches low. */
    sda_high();
    scl_high();

    /* RM0008 §9.2.1 GPIOB_CRL: PB6 bits [27:24], PB7 bits [31:28]
     * = general-purpose open-drain output, 2 MHz. */
    uint32_t crl = GPIO_CRL(GPIOB_BASE);
    crl &= ~((0xFu << (SCL_PIN * 4u)) | (0xFu << (SDA_PIN * 4u)));
    crl |= (GPIO_OUT_OD_2MHZ << (SCL_PIN * 4u)) | (GPIO_OUT_OD_2MHZ << (SDA_PIN * 4u));
    GPIO_CRL(GPIOB_BASE) = crl;

    delay_us(I2C_HALF_US);

    /* Bus recovery: a slave left mid-byte (e.g. by a reset during a transfer)
     * may be waiting for clocks. 9 SCL pulses with SDA released let it finish
     * the byte, then a STOP returns it to idle (UM10204 §3.1.16). */
    for (int i = 0; i < 9; i++) {
        scl_low();
        delay_us(I2C_HALF_US);
        scl_high();
        delay_us(I2C_HALF_US);
    }
    scl_low(); /* i2c_stop() expects SCL low, else its sda_low() is a START */
    delay_us(I2C_HALF_US);
    i2c_stop();
}

int i2c_bus_idle(void)
{
    return scl_read() && sda_read();
}

/* START: SDA falls while SCL is high. */
void i2c_start(void)
{
    sda_high();
    scl_high();
    delay_us(I2C_HALF_US);
    sda_low();
    delay_us(I2C_HALF_US);
    scl_low();
    delay_us(I2C_HALF_US);
}

/* STOP: SDA rises while SCL is high. */
void i2c_stop(void)
{
    sda_low();
    delay_us(I2C_HALF_US);
    scl_high();
    delay_us(I2C_HALF_US);
    sda_high();
    delay_us(I2C_HALF_US);
}

/* MSB first. SDA may only change while SCL is low. The SSD1306 never
 * stretches the clock, so we don't wait for SCL to actually go high. */
int i2c_write_byte(uint8_t b)
{
    for (int i = 7; i >= 0; i--) {
        if (b & (1u << i)) {
            sda_high();
        } else {
            sda_low();
        }
        delay_us(I2C_HALF_US);
        scl_high();
        delay_us(I2C_HALF_US);
        scl_low();
    }

    /* 9th clock: release SDA; the slave pulls it low to ACK. */
    sda_high();
    delay_us(I2C_HALF_US);
    scl_high();
    delay_us(I2C_HALF_US);
    int ack = !sda_read();
    scl_low();
    delay_us(I2C_HALF_US);
    return ack;
}

int i2c_probe(uint8_t addr7)
{
    i2c_start();
    int ack = i2c_write_byte((uint8_t)(addr7 << 1)); /* R/W bit 0 = write */
    i2c_stop();
    return ack;
}
