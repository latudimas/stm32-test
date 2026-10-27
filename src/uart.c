/*
 * USART1 TX on PA9. Clocked from APB2 (pclk2_hz); call after clock_init().
 */
#include "uart.h"
#include "clock.h"
#include "stm32f103.h"

#define TX_PIN 9u

#define BAUD 115200u

void uart_init(void)
{
    /* RM0008 §7.3.7 RCC_APB2ENR: IOPAEN=1, USART1EN=1 — clock GPIOA and USART1. */
    RCC_APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;

    /* RM0008 §9.2.2 GPIOA_CRH: PA9 = bits [7:4] -> alternate-function push-pull,
     * 2 MHz, so USART1_TX drives the pin (§9.1.11 Table 24: USART TX = AF push-pull). */
    uint32_t crh = GPIO_CRH(GPIOA_BASE);
    crh &= ~(0xFu << ((TX_PIN - 8u) * 4u));
    crh |= (GPIO_AF_PP_2MHZ << ((TX_PIN - 8u) * 4u));
    GPIO_CRH(GPIOA_BASE) = crh;

    /* RM0008 §27.3.4: baud = fPCLK / (16 * USARTDIV); BRR holds USARTDIV as
     * 12.4 fixed point, i.e. BRR = fPCLK / baud (rounded).
     *   72 MHz / 115200 = 625.0  -> BRR = 625 (0x271), exact
     *    8 MHz / 115200 = 69.44  -> BRR = 69, +0.64% (HSE-failed fallback; UART tolerates ~2%)
     * RM0008 §27.6.3 USART_BRR. */
    USART1_BRR = (pclk2_hz + BAUD / 2u) / BAUD;

    /* RM0008 §27.6.4 USART_CR1: UE=1, TE=1. M=0 (8 data bits), PCE=0 (no
     * parity); CR2 STOP=00 (1 stop bit) is the reset value. */
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE;
}

static void tx(uint8_t b)
{
    /* RM0008 §27.6.1 USART_SR: wait for TXE (DR moved to shift register). */
    while (!(USART1_SR & USART_SR_TXE)) {
    }
    /* RM0008 §27.6.2 USART_DR: writing starts the next frame. */
    USART1_DR = b;
}

void uart_putc(char c)
{
    if (c == '\n') {
        tx('\r'); /* terminals expect CRLF */
    }
    tx((uint8_t)c);
}

void uart_puts(const char *s)
{
    for (; *s; s++) {
        uart_putc(*s);
    }
}

void uart_put_u32(uint32_t v)
{
    char buf[10]; /* 4294967295 has 10 digits */
    int n = 0;
    do {
        buf[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n) {
        uart_putc(buf[--n]);
    }
}

void uart_put_hex32(uint32_t v)
{
    for (int shift = 28; shift >= 0; shift -= 4) {
        uart_putc("0123456789abcdef"[(v >> shift) & 0xFu]);
    }
}

