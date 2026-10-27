/*
 * Clock tree setup, RM0008 §7.2 "Clocks":
 *
 *   HSE 8 MHz --> PLL x9 --> SYSCLK 72 MHz --> AHB  /1 = 72 MHz (core, SysTick)
 *                                          +-> APB2 /1 = 72 MHz (GPIO, USART1)
 *                                          +-> APB1 /2 = 36 MHz (I2C1, max 36 MHz)
 *
 * Must run first in main(): SysTick, UART and I2C timing depend on the result.
 */
#include "clock.h"
#include "stm32f103.h"

#define HSI_HZ 8000000u
#define HSE_HZ 8000000u /* Blue Pill crystal */

/* Busy-wait limit for the crystal to start (SysTick isn't running yet).
 * ~10 cycles per iteration at 8 MHz -> ~100 ms; a good crystal needs ~1-2 ms. */
#define HSE_STARTUP_LOOPS 80000u

/* Reset state: HSI, all prescalers /1 (RM0008 §7.3.2 RCC_CFGR reset value). */
uint32_t cpu_hz = HSI_HZ;
uint32_t pclk1_hz = HSI_HZ;
uint32_t pclk2_hz = HSI_HZ;

int clock_init(void)
{
    /* 1. RM0008 §7.3.1 RCC_CR: HSEON=1 — start the crystal oscillator, wait for HSERDY. */
    RCC_CR |= RCC_CR_HSEON;
    for (volatile uint32_t i = 0; !(RCC_CR & RCC_CR_HSERDY); i++) {
        if (i >= HSE_STARTUP_LOOPS) {
            RCC_CR &= ~RCC_CR_HSEON; /* give up, stay on HSI 8 MHz */
            return 0;
        }
    }

    /* 2. FLASH_ACR (RM0008 §3.3.3, PM0075 §3.1): 2 wait states for 48-72 MHz,
     * prefetch on. Must be set BEFORE raising SYSCLK, or the core fetches
     * instructions faster than flash can deliver them. */
    FLASH_ACR = (FLASH_ACR & ~FLASH_ACR_LATENCY_MASK) | FLASH_ACR_LATENCY_2 | FLASH_ACR_PRFTBE;

    /* 3. RM0008 §7.3.2 RCC_CFGR (PLL must be off while changing these):
     *    PLLSRC=1, PLLXTPRE=0 -> PLL input = HSE 8 MHz
     *    PLLMUL=0111          -> x9 = 72 MHz
     *    HPRE=0000            -> AHB  /1 = 72 MHz
     *    PPRE1=100            -> APB1 /2 = 36 MHz (36 MHz max, datasheet §5.3.1)
     *    PPRE2=000            -> APB2 /1 = 72 MHz */
    uint32_t cfgr = RCC_CFGR;
    cfgr &= ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMUL_MASK |
              RCC_CFGR_HPRE_MASK | RCC_CFGR_PPRE1_MASK | RCC_CFGR_PPRE2_MASK);
    cfgr |= RCC_CFGR_PLLSRC | RCC_CFGR_PLLMUL_X9 | RCC_CFGR_PPRE1_DIV2;
    RCC_CFGR = cfgr;

    /* 4. RM0008 §7.3.1 RCC_CR: PLLON=1, wait for PLLRDY (lock takes ~200 us). */
    RCC_CR |= RCC_CR_PLLON;
    while (!(RCC_CR & RCC_CR_PLLRDY)) {
    }

    /* 5. RM0008 §7.3.2 RCC_CFGR: SW=10 — switch SYSCLK to the PLL, wait for SWS=10. */
    RCC_CFGR = (RCC_CFGR & ~RCC_CFGR_SW_MASK) | RCC_CFGR_SW_PLL;
    while ((RCC_CFGR & RCC_CFGR_SWS_MASK) != RCC_CFGR_SWS_PLL) {
    }

    cpu_hz = HSE_HZ * 9u;
    pclk1_hz = cpu_hz / 2u;
    pclk2_hz = cpu_hz;
    return 1;
}
