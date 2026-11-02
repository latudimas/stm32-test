/* Host-only fake MMIO for the thread-polled TX queue. */
#include <assert.h>
#include <stdint.h>
#include <string.h>

#define STM32F103_H /* replace the target MMIO definitions in this test only */
static uint32_t rcc, mock_crh, brr, cr1, sr, dr;
#define RCC_APB2ENR rcc
#define RCC_APB2ENR_IOPAEN (1u << 2)
#define RCC_APB2ENR_USART1EN (1u << 14)
#define GPIO_CRH(base) mock_crh
#define GPIOA_BASE 0u
#define GPIO_AF_PP_2MHZ 0xAu
#define USART1_BRR brr
#define USART1_CR1 cr1
#define USART1_SR sr
#define USART1_DR dr
#define USART_CR1_UE (1u << 13)
#define USART_CR1_TE (1u << 3)
#define USART_SR_TXE (1u << 7)

uint32_t pclk2_hz = 72000000u;
#include "../src/uart.c"

static void expect(const char *s)
{
    while (*s) {
        sr = USART_SR_TXE;
        uart_poll();
        assert(dr == (uint8_t)*s++);
    }
    dr = 0xFFFF;
    uart_poll();
    assert(dr == 0xFFFF); /* empty queue does not write */
    sr = 0;
}

int main(void)
{
    uart_init();
    assert(brr == 625 && (rcc & RCC_APB2ENR_USART1EN));
    sr = 0; /* UART busy: formatting must enqueue without waiting */
    uart_put_i32(INT32_MIN);
    uart_putc(',');
    uart_put_u32(UINT32_MAX);
    uart_puts(",0\n");
    expect("-2147483648,4294967295,0\r\n");
    for (int i = 0; i < 100; i++) { /* force head and tail to wrap repeatedly */
        uart_puts("accel,12345,-123,456,-789\n");
        expect("accel,12345,-123,456,-789\r\n");
    }
    uart_flush(); /* empty queue */
    return 0;
}
