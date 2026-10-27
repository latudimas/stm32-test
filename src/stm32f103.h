/*
 * Hand-written register definitions for the parts of the STM32F103 we use.
 * Addresses: RM0008 §3.3 "Memory map"; offsets/bits from each peripheral chapter.
 */
#ifndef STM32F103_H
#define STM32F103_H

#include <stdint.h>

#define REG(addr) (*(volatile uint32_t *)(addr))

/* RCC — RM0008 §7.3, base 0x40021000 */
#define RCC_BASE        0x40021000u
#define RCC_APB2ENR     REG(RCC_BASE + 0x18) /* §7.3.7 APB2 peripheral clock enable */
#define RCC_APB2ENR_IOPBEN (1u << 3)         /* I/O port B clock enable */
#define RCC_APB2ENR_IOPCEN (1u << 4)         /* I/O port C clock enable */

/* GPIO — RM0008 §9.2. CRL = pins 0..7, CRH = pins 8..15, 4 bits per pin. */
#define GPIO_CRL(base)  REG((base) + 0x00) /* §9.2.1 port configuration register low */
#define GPIO_CRH(base)  REG((base) + 0x04) /* §9.2.2 port configuration register high */
#define GPIO_IDR(base)  REG((base) + 0x08) /* §9.2.3 input data register */
#define GPIO_ODR(base)  REG((base) + 0x0C) /* §9.2.4 output data register */
#define GPIO_BSRR(base) REG((base) + 0x10) /* §9.2.5 bit set/reset register */

#define GPIOB_BASE      0x40010C00u
#define GPIOC_BASE      0x40011000u

/* Pin config nibbles {CNF[1:0], MODE[1:0]} — RM0008 §9.1 Table 20/21. */
#define GPIO_OUT_PP_2MHZ 0x2u /* CNF=00 general-purpose push-pull,  MODE=10 2 MHz */
#define GPIO_OUT_OD_2MHZ 0x6u /* CNF=01 general-purpose open-drain, MODE=10 2 MHz */

#endif
