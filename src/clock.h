/*
 * System clock: HSE 8 MHz crystal -> PLL x9 -> 72 MHz, with HSI 8 MHz fallback.
 * Everything that needs a clock rate reads it from these variables.
 */
#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>

extern uint32_t cpu_hz;   /* SYSCLK = AHB = core/SysTick clock */
extern uint32_t pclk1_hz; /* APB1: I2C, USART2/3, TIM2-4 (max 36 MHz) */
extern uint32_t pclk2_hz; /* APB2: GPIO, USART1 */

int clock_init(void); /* 1 = running at 72 MHz from HSE, 0 = HSE failed, still on HSI 8 MHz */

#endif
