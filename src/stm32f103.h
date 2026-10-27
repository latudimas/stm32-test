/*
 * Hand-written register definitions for the parts of the STM32F103 we use.
 * Addresses: RM0008 §3.3 "Memory map"; offsets/bits from each peripheral chapter.
 */
#ifndef STM32F103_H
#define STM32F103_H

#include <stdint.h>

#define REG(addr) (*(volatile uint32_t *)(addr))

/* SysTick — Cortex-M3 core timer, PM0056 §4.5 (not in RM0008) */
#define STK_CTRL        REG(0xE000E010u) /* §4.5.1 control and status */
#define STK_LOAD        REG(0xE000E014u) /* §4.5.2 reload value (24-bit) */
#define STK_VAL         REG(0xE000E018u) /* §4.5.3 current value */
#define STK_CTRL_ENABLE    (1u << 0)
#define STK_CTRL_TICKINT   (1u << 1)     /* exception 15 on reaching 0 */
#define STK_CTRL_CLKSOURCE (1u << 2)     /* 1 = processor clock (AHB), 0 = AHB/8 */

/* NVIC — Cortex-M3 core interrupt controller, PM0056 §4.3. IRQ n is enabled by
 * bit (n % 32) of ISER[n / 32]; IRQ numbers per RM0008 §10.1.2 Table 63. */
#define NVIC_ISER0      REG(0xE000E100u) /* §4.3.2 set-enable, IRQ 0..31 */
#define DMA1_CH6_IRQn   16u

/* RCC — RM0008 §7.3, base 0x40021000 */
#define RCC_BASE        0x40021000u
#define RCC_CR          REG(RCC_BASE + 0x00) /* §7.3.1 clock control */
#define RCC_CR_HSEON    (1u << 16)
#define RCC_CR_HSERDY   (1u << 17)
#define RCC_CR_PLLON    (1u << 24)
#define RCC_CR_PLLRDY   (1u << 25)
#define RCC_CFGR        REG(RCC_BASE + 0x04) /* §7.3.2 clock configuration */
#define RCC_CFGR_SW_MASK    (3u << 0)        /* system clock switch */
#define RCC_CFGR_SW_PLL     (2u << 0)
#define RCC_CFGR_SWS_MASK   (3u << 2)        /* system clock switch status */
#define RCC_CFGR_SWS_PLL    (2u << 2)
#define RCC_CFGR_HPRE_MASK  (0xFu << 4)      /* AHB prescaler; 0 = /1 */
#define RCC_CFGR_PPRE1_MASK (7u << 8)        /* APB1 prescaler */
#define RCC_CFGR_PPRE1_DIV2 (4u << 8)
#define RCC_CFGR_PPRE2_MASK (7u << 11)       /* APB2 prescaler; 0 = /1 */
#define RCC_CFGR_PLLSRC     (1u << 16)       /* 1 = HSE (via PLLXTPRE) */
#define RCC_CFGR_PLLXTPRE   (1u << 17)       /* 1 = HSE/2 into PLL */
#define RCC_CFGR_PLLMUL_MASK (0xFu << 18)
#define RCC_CFGR_PLLMUL_X9  (7u << 18)       /* 0111 = x9 */
#define RCC_AHBENR      REG(RCC_BASE + 0x14) /* §7.3.6 AHB peripheral clock enable */
#define RCC_AHBENR_DMA1EN  (1u << 0)         /* DMA1 clock enable */
#define RCC_APB2ENR     REG(RCC_BASE + 0x18) /* §7.3.7 APB2 peripheral clock enable */
#define RCC_APB1ENR     REG(RCC_BASE + 0x1C) /* §7.3.8 APB1 peripheral clock enable */
#define RCC_APB1ENR_I2C1EN (1u << 21)        /* I2C1 clock enable */
#define RCC_APB2ENR_IOPAEN (1u << 2)         /* I/O port A clock enable */
#define RCC_APB2ENR_IOPBEN (1u << 3)         /* I/O port B clock enable */
#define RCC_APB2ENR_IOPCEN (1u << 4)         /* I/O port C clock enable */
#define RCC_APB2ENR_USART1EN (1u << 14)      /* USART1 clock enable */

/* Flash interface — FLASH_ACR, RM0008 §3.3.3 / PM0075 §3.1, base 0x40022000 */
#define FLASH_ACR       REG(0x40022000u)
#define FLASH_ACR_LATENCY_MASK (7u << 0)
#define FLASH_ACR_LATENCY_2    (2u << 0)     /* 2 wait states: 48 < SYSCLK <= 72 MHz */
#define FLASH_ACR_PRFTBE       (1u << 4)     /* prefetch buffer enable */

/* GPIO — RM0008 §9.2. CRL = pins 0..7, CRH = pins 8..15, 4 bits per pin. */
#define GPIO_CRL(base)  REG((base) + 0x00) /* §9.2.1 port configuration register low */
#define GPIO_CRH(base)  REG((base) + 0x04) /* §9.2.2 port configuration register high */
#define GPIO_IDR(base)  REG((base) + 0x08) /* §9.2.3 input data register */
#define GPIO_ODR(base)  REG((base) + 0x0C) /* §9.2.4 output data register */
#define GPIO_BSRR(base) REG((base) + 0x10) /* §9.2.5 bit set/reset register */

#define GPIOA_BASE      0x40010800u
#define GPIOB_BASE      0x40010C00u
#define GPIOC_BASE      0x40011000u

/* Pin config nibbles {CNF[1:0], MODE[1:0]} — RM0008 §9.1 Table 20/21. */
#define GPIO_OUT_PP_2MHZ 0x2u /* CNF=00 general-purpose push-pull,  MODE=10 2 MHz */
#define GPIO_OUT_OD_2MHZ 0x6u /* CNF=01 general-purpose open-drain, MODE=10 2 MHz */
#define GPIO_AF_PP_2MHZ  0xAu /* CNF=10 alternate-function push-pull, MODE=10 2 MHz */
#define GPIO_AF_OD_50MHZ 0xFu /* CNF=11 alternate-function open-drain, MODE=11 50 MHz */

/* I2C1 — RM0008 §26.6, base 0x40005400 (APB1) */
#define I2C1_BASE       0x40005400u
#define I2C1_CR1        REG(I2C1_BASE + 0x00) /* §26.6.1 control register 1 */
#define I2C1_CR2        REG(I2C1_BASE + 0x04) /* §26.6.2 control register 2 */
#define I2C1_DR         REG(I2C1_BASE + 0x10) /* §26.6.5 data register */
#define I2C1_SR1        REG(I2C1_BASE + 0x14) /* §26.6.6 status register 1 */
#define I2C1_SR2        REG(I2C1_BASE + 0x18) /* §26.6.7 status register 2 */
#define I2C1_CCR        REG(I2C1_BASE + 0x1C) /* §26.6.8 clock control */
#define I2C1_TRISE      REG(I2C1_BASE + 0x20) /* §26.6.9 rise time */
#define I2C_CR1_PE      (1u << 0)  /* peripheral enable */
#define I2C_CR1_START   (1u << 8)  /* generate START (cleared by hardware when sent) */
#define I2C_CR1_STOP    (1u << 9)  /* generate STOP (cleared by hardware when sent) */
#define I2C_CR1_SWRST   (1u << 15) /* software reset */
#define I2C_SR1_SB      (1u << 0)  /* START sent (EV5) */
#define I2C_SR1_ADDR    (1u << 1)  /* address sent and ACKed (EV6) */
#define I2C_SR1_BTF     (1u << 2)  /* byte transfer finished, incl. ACK bit */
#define I2C_SR1_TXE     (1u << 7)  /* DR empty */
#define I2C_SR1_BERR    (1u << 8)  /* bus error (misplaced START/STOP) */
#define I2C_SR1_ARLO    (1u << 9)  /* arbitration lost */
#define I2C_SR1_AF      (1u << 10) /* acknowledge failure (NACK) */
#define I2C_SR2_BUSY    (1u << 1)  /* bus busy (SDA or SCL low seen) */
#define I2C_CCR_FS      (1u << 15) /* 1 = fast mode (400 kHz) */
#define I2C_CR2_DMAEN   (1u << 11) /* DMA request on TXE/RXNE */

/* DMA1 — RM0008 §13.4, base 0x40020000 (AHB). I2C1_TX is hard-wired to
 * channel 6 (§13.3.7 Table 78 "Summary of DMA1 requests for each channel"). */
#define DMA1_BASE       0x40020000u
#define DMA1_ISR        REG(DMA1_BASE + 0x00) /* §13.4.1 interrupt status */
#define DMA1_IFCR       REG(DMA1_BASE + 0x04) /* §13.4.2 interrupt flag clear */
#define DMA1_CCR6       REG(DMA1_BASE + 0x6C) /* §13.4.3 channel 6 configuration */
#define DMA1_CNDTR6     REG(DMA1_BASE + 0x70) /* §13.4.4 channel 6 number of data */
#define DMA1_CPAR6      REG(DMA1_BASE + 0x74) /* §13.4.5 channel 6 peripheral address */
#define DMA1_CMAR6      REG(DMA1_BASE + 0x78) /* §13.4.6 channel 6 memory address */
#define DMA_ISR_TCIF6   (1u << 21) /* channel 6 transfer complete */
#define DMA_ISR_TEIF6   (1u << 23) /* channel 6 transfer error */
#define DMA_IFCR_CGIF6  (1u << 20) /* clear all channel 6 flags */
#define DMA_CCR_EN      (1u << 0)
#define DMA_CCR_TCIE    (1u << 1)  /* interrupt on transfer complete */
#define DMA_CCR_TEIE    (1u << 3)  /* interrupt on transfer error */
#define DMA_CCR_DIR     (1u << 4)  /* 1 = memory -> peripheral */
#define DMA_CCR_MINC    (1u << 7)  /* increment memory address */
#define DMA_CCR_PL_HIGH (2u << 12) /* channel priority */
/* PSIZE/MSIZE = 00 (8-bit), PINC = 0, CIRC = 0, MEM2MEM = 0: all left at zero. */

/* USART1 — RM0008 §27.6, base 0x40013800 */
#define USART1_BASE     0x40013800u
#define USART1_SR       REG(USART1_BASE + 0x00) /* §27.6.1 status register */
#define USART1_DR       REG(USART1_BASE + 0x04) /* §27.6.2 data register */
#define USART1_BRR      REG(USART1_BASE + 0x08) /* §27.6.3 baud rate register */
#define USART1_CR1      REG(USART1_BASE + 0x0C) /* §27.6.4 control register 1 */
#define USART_SR_TXE    (1u << 7)  /* transmit data register empty */
#define USART_CR1_TE    (1u << 3)  /* transmitter enable */
#define USART_CR1_UE    (1u << 13) /* USART enable */

#endif
