/*
 * Hardware I2C1 master on PB6 (SCL) / PB7 (SDA), fast mode 400 kHz, polled.
 * Same API as i2c_bitbang.c (pick one in the Makefile: I2C=hw / I2C=bitbang).
 *
 * Master transmitter sequence, RM0008 §26.3.3 ("Transfer sequence diagram for
 * master transmitter"):
 *   START -> EV5 (SB) -> addr -> EV6 (ADDR, clear by SR1+SR2 read)
 *   -> data -> EV8 (TXE) ... -> EV8_2 (BTF) -> STOP
 *
 * Every byte waits for BTF (byte + ACK bit done) or AF (NACK), so each
 * i2c_write_byte() reports its own ACK like the bit-bang driver. While BTF is
 * set the peripheral stretches SCL, so an interrupt (SysTick) between bytes
 * only pauses the bus — this sidesteps ES0340's "events must be managed before
 * the current byte is transferred" timing issue.
 *
 * Every wait has a timeout so a stuck bus can't hang the firmware.
 *
 * Bulk data (i2c_write_async) goes through DMA1 channel 6: each time I2C1
 * raises TXE, the DMA controller copies the next byte from RAM into I2C1_DR
 * by itself, so the CPU is free until the transfer ends.
 */
#include "i2c.h"
#include "clock.h"
#include "stm32f103.h"
#include "systick.h"

#define SCL_PIN 6u
#define SDA_PIN 7u

#define I2C_HZ      400000u
#define TIMEOUT_MS  2u /* a 400 kHz byte takes ~23 us; 2 ms means something is wrong */

const char i2c_driver[] = "hw I2C1 400 kHz";

/* Set while a START has been sent but the address byte hasn't. */
static int expect_addr;

/* DMA transfer state for i2c_write_async()/i2c_write_poll(). */
enum { DMA_IDLE, DMA_SENDING, DMA_DRAINING };
static int dma_state = DMA_IDLE;
static uint32_t dma_start_ms;
static uint32_t dma_timeout_ms;

static void set_pins(uint32_t cfg)
{
    /* RM0008 §9.2.1 GPIOB_CRL: PB6 bits [27:24], PB7 bits [31:28]. */
    uint32_t crl = GPIO_CRL(GPIOB_BASE);
    crl &= ~((0xFu << (SCL_PIN * 4u)) | (0xFu << (SDA_PIN * 4u)));
    crl |= (cfg << (SCL_PIN * 4u)) | (cfg << (SDA_PIN * 4u));
    GPIO_CRL(GPIOB_BASE) = crl;
}

/* RM0008 §9.2.5 GPIOB_BSRR — only used while the pins are plain GPIO. */
static void scl_set(int high)
{
    GPIO_BSRR(GPIOB_BASE) = high ? 1u << SCL_PIN : 1u << (SCL_PIN + 16u);
}
static void sda_set(int high)
{
    GPIO_BSRR(GPIOB_BASE) = high ? 1u << SDA_PIN : 1u << (SDA_PIN + 16u);
}

/* Bus recovery done in GPIO mode, before handing the pins to I2C1:
 * a slave stuck mid-byte gets 9 clocks to finish, then a STOP (UM10204 §3.1.16).
 * Also the first half of ES0340's "analog filter may lock BUSY flag" workaround. */
static void bus_recover(void)
{
    sda_set(1);
    scl_set(1);
    set_pins(GPIO_OUT_OD_2MHZ);
    delay_us(5);
    for (int i = 0; i < 9; i++) {
        scl_set(0);
        delay_us(5);
        scl_set(1);
        delay_us(5);
    }
    /* STOP: SDA low while SCL low, raise SCL, then raise SDA. */
    scl_set(0);
    delay_us(5);
    sda_set(0);
    delay_us(5);
    scl_set(1);
    delay_us(5);
    sda_set(1);
    delay_us(5);
}

/* Wait until any bit of `mask` is set in SR1 or a NACK/bus error shows up.
 * Returns the SR1 value seen, or 0 on timeout. */
static uint32_t wait_sr1(uint32_t mask)
{
    uint32_t start = millis();
    for (;;) {
        /* RM0008 §26.6.6 I2C_SR1 */
        uint32_t sr1 = I2C1_SR1;
        if (sr1 & (mask | I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO)) {
            return sr1;
        }
        if (millis() - start > TIMEOUT_MS) {
            return 0;
        }
    }
}

/* Clear error flags (rc_w0: write 0 to clear, RM0008 §26.6.6). Returns 1 if one was set. */
static int clear_errors(uint32_t sr1)
{
    uint32_t err = sr1 & (I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO);
    if (err) {
        I2C1_SR1 = ~err & 0xFFFFu;
    }
    return err != 0;
}

void i2c_init(void)
{
    /* RM0008 §7.3.7 RCC_APB2ENR: IOPBEN=1; §7.3.8 RCC_APB1ENR: I2C1EN=1;
     * §7.3.6 RCC_AHBENR: DMA1EN=1. */
    RCC_APB2ENR |= RCC_APB2ENR_IOPBEN;
    RCC_APB1ENR |= RCC_APB1ENR_I2C1EN;
    RCC_AHBENR |= RCC_AHBENR_DMA1EN;

    /* RM0008 §13.4.5 DMA_CPAR6: the channel always writes into I2C1_DR. */
    DMA1_CPAR6 = (uint32_t)&I2C1_DR;

    bus_recover();

    /* Pins to I2C1: RM0008 §9.1.11 Table 27 — I2C SCL/SDA = alternate-function
     * open-drain. 50 MHz keeps edges sharp enough for 400 kHz. */
    set_pins(GPIO_AF_OD_50MHZ);

    /* RM0008 §26.6.1 I2C_CR1: SWRST pulse — clears any stuck BUSY state left
     * by the reset or the pin switch (second half of the ES0340 workaround). */
    I2C1_CR1 = I2C_CR1_SWRST;
    I2C1_CR1 = 0;

    /* RM0008 §26.6.2 I2C_CR2: FREQ = APB1 clock in MHz (36). */
    I2C1_CR2 = pclk1_hz / 1000000u;

    /* RM0008 §26.6.8 I2C_CCR, fast mode, DUTY=0: tLOW = 2*CCR, tHIGH = CCR
     * (in PCLK1 periods) -> f = PCLK1 / (3 * CCR) = 36 MHz / (3 * 30) = 400 kHz. */
    I2C1_CCR = I2C_CCR_FS | (pclk1_hz / (3u * I2C_HZ));

    /* RM0008 §26.6.9 I2C_TRISE: max SCL rise time in PCLK1 periods + 1.
     * Fast mode allows 300 ns -> 300 ns * 36 MHz = 10.8 -> 10 + 1 = 11. */
    I2C1_TRISE = (pclk1_hz / 1000000u) * 300u / 1000u + 1u;

    /* RM0008 §26.6.1 I2C_CR1: PE=1 — enable. */
    I2C1_CR1 = I2C_CR1_PE;
}

int i2c_bus_idle(void)
{
    /* RM0008 §26.6.7 I2C_SR2: BUSY is set while the peripheral sees SCL/SDA low. */
    return !(I2C1_SR2 & I2C_SR2_BUSY);
}

void i2c_start(void)
{
    /* RM0008 §26.6.1 I2C_CR1: START=1, then EV5: wait for SR1.SB. */
    I2C1_CR1 |= I2C_CR1_START;
    expect_addr = wait_sr1(I2C_SR1_SB) & I2C_SR1_SB;
}

void i2c_stop(void)
{
    /* RM0008 §26.6.1 I2C_CR1: STOP=1. Hardware clears it once the STOP is on
     * the bus; wait for that so the next START can't collide with it. */
    I2C1_CR1 |= I2C_CR1_STOP;
    uint32_t start = millis();
    while ((I2C1_CR1 & I2C_CR1_STOP) && millis() - start <= TIMEOUT_MS) {
    }
    expect_addr = 0;
}

int i2c_write_byte(uint8_t b)
{
    if (expect_addr) {
        /* EV5 -> EV6: writing DR (after the SR1 read in wait_sr1) clears SB
         * and sends the address. ADDR = slave ACKed, AF = no slave. */
        expect_addr = 0;
        I2C1_DR = b;
        uint32_t sr1 = wait_sr1(I2C_SR1_ADDR);
        if (!sr1 || clear_errors(sr1)) {
            return 0;
        }
        (void)I2C1_SR2; /* EV6: ADDR clears on SR1 read then SR2 read */
        return 1;
    }

    /* EV8: DR empty -> load next byte, then wait for it (and its ACK bit). */
    uint32_t sr1 = wait_sr1(I2C_SR1_TXE);
    if (!sr1 || clear_errors(sr1)) {
        return 0;
    }
    I2C1_DR = b;
    sr1 = wait_sr1(I2C_SR1_BTF);
    return sr1 && !clear_errors(sr1);
}

int i2c_probe(uint8_t addr7)
{
    i2c_start();
    int ack = i2c_write_byte((uint8_t)(addr7 << 1)); /* R/W bit 0 = write */
    i2c_stop();
    return ack;
}

/* Master transmitter with DMA, RM0008 §26.3.7 "DMA requests":
 * DMA fills DR on every TXE; when the DMA count reaches 0 (TC) the last byte
 * is only in DR/the shift register, so wait for BTF before STOP (EV8_2). */
void i2c_write_async(const uint8_t *buf, uint32_t len)
{
    /* RM0008 §13.4.3 DMA_CCR6: channel off while it's being set up. */
    DMA1_CCR6 = 0;
    /* RM0008 §13.4.2 DMA_IFCR: clear old channel 6 flags. */
    DMA1_IFCR = DMA_IFCR_CGIF6;
    /* RM0008 §13.4.6 DMA_CMAR6 / §13.4.4 DMA_CNDTR6: source and byte count. */
    DMA1_CMAR6 = (uint32_t)buf;
    DMA1_CNDTR6 = len;
    /* RM0008 §13.4.3 DMA_CCR6: memory -> peripheral, memory address
     * increments, 8-bit on both sides, high priority, then enable. */
    DMA1_CCR6 = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_PL_HIGH;
    DMA1_CCR6 |= DMA_CCR_EN;

    /* RM0008 §26.6.2 I2C_CR2: DMAEN=1 — TXE now raises DMA requests. TXE is
     * already set (previous byte done), so the first request fires at once. */
    I2C1_CR2 |= I2C_CR2_DMAEN;

    /* 400 kHz = ~23 us per byte; allow double that plus margin. */
    dma_start_ms = millis();
    dma_timeout_ms = 2u + len / 20u;
    dma_state = DMA_SENDING;
}

static int dma_finish(int result)
{
    /* RM0008 §26.6.2 I2C_CR2: DMAEN=0; §13.4.3 DMA_CCR6: EN=0. Only EN is
     * cleared: writing all of CCR while the channel is enabled was observed to
     * leave the other bits set anyway, and i2c_write_async() rewrites them. */
    I2C1_CR2 &= ~I2C_CR2_DMAEN;
    DMA1_CCR6 &= ~DMA_CCR_EN;
    DMA1_IFCR = DMA_IFCR_CGIF6;
    clear_errors(I2C1_SR1);
    i2c_stop();
    dma_state = DMA_IDLE;
    return result;
}

int i2c_write_poll(void)
{
    if (dma_state == DMA_IDLE) {
        return I2C_DONE;
    }

    /* RM0008 §26.6.6 I2C_SR1 / §13.4.1 DMA_ISR */
    uint32_t sr1 = I2C1_SR1;
    if ((sr1 & (I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO)) || (DMA1_ISR & DMA_ISR_TEIF6) ||
        millis() - dma_start_ms > dma_timeout_ms) {
        return dma_finish(I2C_ERR);
    }

    if (dma_state == DMA_SENDING && (DMA1_ISR & DMA_ISR_TCIF6)) {
        dma_state = DMA_DRAINING; /* all bytes handed to I2C1, last one still on the wire */
    }
    if (dma_state == DMA_DRAINING && (sr1 & I2C_SR1_BTF)) {
        return dma_finish(I2C_DONE);
    }
    return I2C_BUSY;
}
