/*
 * Milestone 2: blink the Blue Pill's onboard LED on PC13 (active low).
 * Runs on the reset clock: HSI 8 MHz (RM0008 §7.2), no clock setup needed.
 */
#include <stdint.h>

#define REG(addr) (*(volatile uint32_t *)(addr))

/* RCC — RM0008 §7.3, base 0x40021000 (§3.3 memory map) */
#define RCC_BASE        0x40021000u
#define RCC_APB2ENR     REG(RCC_BASE + 0x18) /* §7.3.7 APB2 peripheral clock enable */
#define RCC_APB2ENR_IOPCEN (1u << 4)         /* I/O port C clock enable */

/* GPIOC — RM0008 §9.2, base 0x40011000 (§3.3 memory map) */
#define GPIOC_BASE      0x40011000u
#define GPIOC_CRH       REG(GPIOC_BASE + 0x04) /* §9.2.2 config register high (pins 8..15) */
#define GPIOC_BSRR      REG(GPIOC_BASE + 0x10) /* §9.2.5 bit set/reset register */

#define LED_PIN 13u

/* ~10 CPU cycles per delay() iteration at 8 MHz (measured) -> 1.6M iterations ~= 2 s. */
#define BLINK_DELAY 1600000u

static void delay(uint32_t n)
{
    /* volatile keeps the compiler from deleting the empty loop. */
    for (volatile uint32_t i = 0; i < n; i++) {
    }
}

int main(void)
{
    /* RM0008 §7.3.7 RCC_APB2ENR: IOPCEN=1 — GPIOC registers are dead until clocked. */
    RCC_APB2ENR |= RCC_APB2ENR_IOPCEN;

    /* RM0008 §9.2.2 GPIOC_CRH: PC13 = bits [23:20] = CNF13[1:0] MODE13[1:0].
     * MODE13=0b10 (output, 2 MHz), CNF13=0b00 (general-purpose push-pull) -> 0x2. */
    uint32_t crh = GPIOC_CRH;
    crh &= ~(0xFu << ((LED_PIN - 8u) * 4u));
    crh |= (0x2u << ((LED_PIN - 8u) * 4u));
    GPIOC_CRH = crh;

    for (;;) {
        /* RM0008 §9.2.5 GPIOC_BSRR: BR13 (bit 29) drives PC13 low -> LED on. */
        GPIOC_BSRR = 1u << (LED_PIN + 16u);
        delay(BLINK_DELAY);

        /* RM0008 §9.2.5 GPIOC_BSRR: BS13 (bit 13) drives PC13 high -> LED off. */
        GPIOC_BSRR = 1u << LED_PIN;
        delay(BLINK_DELAY);
    }
}
