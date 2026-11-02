/*
 * Bit-banged I2C. Lines are open-drain: writing 1 releases the line (the
 * pull-up makes it high), writing 0 pulls it low. Standard-mode timing
 * (UM10204), ~100 kHz.
 */
#include "i2c.h"
#include "stm32f103.h"
#include "systick.h"

#define SCL_PIN 6u
#define SDA_PIN 7u

/* Half bit period. 5 us -> SCL <= 100 kHz (standard mode: tLOW >= 4.7 us,
 * tHIGH >= 4.0 us); call overhead makes it a bit slower in practice. */
#define I2C_HALF_US 5u

const char i2c_driver[] = "bit-bang ~100 kHz";

/* RM0008 §9.2.5 GPIOB_BSRR: BSy (bit y) sets ODR -> line released (high via
 * pull-up); BRy (bit y+16) clears ODR -> line driven low. */
static void scl_low(void)  { GPIO_BSRR(GPIOB_BASE) = 1u << (SCL_PIN + 16u); }
static void sda_high(void) { GPIO_BSRR(GPIOB_BASE) = 1u << SDA_PIN; }
static void sda_low(void)  { GPIO_BSRR(GPIOB_BASE) = 1u << (SDA_PIN + 16u); }

/* RM0008 §9.2.3 GPIOB_IDR. In open-drain output mode the input Schmitt trigger
 * stays active, so IDR shows the real line level (§9.1.8). */
static int sda_read(void) { return (GPIO_IDR(GPIOB_BASE) >> SDA_PIN) & 1u; }
static int scl_read(void) { return (GPIO_IDR(GPIOB_BASE) >> SCL_PIN) & 1u; }

static int bus_error;

static void scl_high(void)
{
    /* RM0008 §9.2.5 GPIOB_BSRR: release SCL; honor clock stretching. */
    GPIO_BSRR(GPIOB_BASE) = 1u << SCL_PIN;
    uint32_t start = millis();
    while (!scl_read()) {
        if (millis() - start > 2u) {
            bus_error = 1;
            break;
        }
    }
}

void i2c_init(void)
{
    bus_error = 0;
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
    bus_error = 0;
    sda_high();
    scl_high();
    if (!sda_read()) {
        bus_error = 1;
    }
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

/* MSB first. SDA may only change while SCL is low. Clock stretching is
 * bounded by scl_high(), so a failed sensor cannot hang the display loop. */
int i2c_write_byte(uint8_t b)
{
    if (bus_error) {
        return 0;
    }
    for (int i = 7; i >= 0; i--) {
        if (b & (1u << i)) {
            sda_high();
        } else {
            sda_low();
        }
        delay_us(I2C_HALF_US);
        scl_high();
        if (bus_error) {
            scl_low();
            return 0;
        }
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
    return ack && !bus_error;
}

static uint8_t read_byte(int ack)
{
    uint8_t b = 0;
    sda_high();
    for (uint32_t i = 0; i < 8u; i++) {
        delay_us(I2C_HALF_US);
        scl_high();
        delay_us(I2C_HALF_US);
        b = (uint8_t)((b << 1) | sda_read());
        scl_low();
        if (bus_error) {
            return 0;
        }
    }
    if (ack) {
        sda_low();
    }
    delay_us(I2C_HALF_US);
    scl_high();
    delay_us(I2C_HALF_US);
    scl_low();
    sda_high();
    delay_us(I2C_HALF_US);
    return b;
}

int i2c_read_regs(uint8_t addr, uint8_t reg, uint8_t *buf, uint32_t len)
{
    if (addr > 0x7Fu || !buf || len == 0 || len > 256u ||
        i2c_write_poll() == I2C_BUSY) {
        return 0;
    }
    i2c_start();
    int ok = i2c_write_byte((uint8_t)(addr << 1)) && i2c_write_byte(reg);
    if (ok) {
        i2c_start(); /* repeated START; preserve register pointer */
        ok = i2c_write_byte((uint8_t)((addr << 1) | 1u));
    }
    for (uint32_t i = 0; ok && i < len; i++) {
        buf[i] = read_byte(i + 1u < len); /* NACK only the final byte */
        ok = !bus_error;
    }
    i2c_stop();
    return ok && !bus_error;
}

/* No DMA here: do the whole transfer now and hand the result to poll. */
static int async_result = I2C_DONE;

void i2c_write_async(const uint8_t *buf, uint32_t len)
{
    int ok = 1;
    for (uint32_t i = 0; ok && i < len; i++) {
        ok = i2c_write_byte(buf[i]);
    }
    i2c_stop();
    async_result = ok ? I2C_DONE : I2C_ERR;
}

int i2c_write_poll(void)
{
    return async_result;
}
