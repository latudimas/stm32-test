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
#define RCC_APB2ENR     REG(RCC_BASE + 0x18) /* §7.3.7 APB2 peripheral clock enable */
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

/* USART1 — RM0008 §27.6, base 0x40013800 */
#define USART1_BASE     0x40013800u
#define USART1_SR       REG(USART1_BASE + 0x00) /* §27.6.1 status register */
#define USART1_DR       REG(USART1_BASE + 0x04) /* §27.6.2 data register */
#define USART1_BRR      REG(USART1_BASE + 0x08) /* §27.6.3 baud rate register */
#define USART1_CR1      REG(USART1_BASE + 0x0C) /* §27.6.4 control register 1 */
#define USART_SR_TXE    (1u << 7)  /* transmit data register empty */
#define USART_CR1_TE    (1u << 3)  /* transmitter enable */
#define USART_CR1_UE    (1u << 13) /* USART enable */

/* Device electronic signature — RM0008 §30.1 */
#define FLASH_SIZE_KB   (*(volatile uint16_t *)0x1FFFF7E0u) /* §30.1.1 flash size in KB */
#define UID_BASE        0x1FFFF7E8u                         /* §30.1.2 96-bit unique ID */

#endif
