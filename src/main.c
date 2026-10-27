/*
 * Milestone 3: check the SSD1306 ACKs its I2C address (0x3C).
 * Runs on the reset clock: HSI 8 MHz (RM0008 §7.2), no clock setup needed.
 *
 * Result on the PC13 LED (active low):
 *   slow blink, 2 s on / 2 s off  -> 0x3C ACKed
 *   fast blink, ~0.1 s            -> no ACK (check wiring / address)
 *   LED stays off                 -> bus not idle (a line stuck low / no pull-ups)
 * The same results are in the globals below, readable from GDB/OpenOCD.
 */
#include "delay.h"
#include "i2c.h"
#include "stm32f103.h"

#define LED_PIN 13u
#define OLED_ADDR 0x3Cu

/* ~10 CPU cycles per delay() iteration at 8 MHz (measured) -> 1.6M iterations ~= 2 s. */
#define BLINK_SLOW 1600000u
#define BLINK_FAST 80000u

volatile uint32_t i2c_bus_ok;   /* 1 = SCL and SDA high at idle */
volatile uint32_t oled_ack;     /* 1 = device at OLED_ADDR ACKed */
volatile uint32_t i2c_found[4]; /* bitmap of every 7-bit address that ACKed */

static void led_init(void)
{
    /* RM0008 §7.3.7 RCC_APB2ENR: IOPCEN=1 — GPIOC registers are dead until clocked. */
    RCC_APB2ENR |= RCC_APB2ENR_IOPCEN;

    /* RM0008 §9.2.2 GPIOC_CRH: PC13 = bits [23:20] -> push-pull output, 2 MHz. */
    uint32_t crh = GPIO_CRH(GPIOC_BASE);
    crh &= ~(0xFu << ((LED_PIN - 8u) * 4u));
    crh |= (GPIO_OUT_PP_2MHZ << ((LED_PIN - 8u) * 4u));
    GPIO_CRH(GPIOC_BASE) = crh;
}

/* RM0008 §9.2.5 GPIOC_BSRR: BR13 (bit 29) drives PC13 low -> LED on. */
static void led_on(void)  { GPIO_BSRR(GPIOC_BASE) = 1u << (LED_PIN + 16u); }
/* RM0008 §9.2.5 GPIOC_BSRR: BS13 (bit 13) drives PC13 high -> LED off. */
static void led_off(void) { GPIO_BSRR(GPIOC_BASE) = 1u << LED_PIN; }

int main(void)
{
    led_init();
    led_off();
    i2c_init();

    i2c_bus_ok = i2c_bus_idle();
    if (!i2c_bus_ok) {
        for (;;) {
        }
    }

    oled_ack = i2c_probe(OLED_ADDR);

    /* Scan the valid 7-bit range too, so a module strapped to 0x3D (or a
     * wiring swap) is visible from the debugger. */
    for (uint8_t a = 0x08; a <= 0x77; a++) {
        if (i2c_probe(a)) {
            i2c_found[a / 32u] |= 1u << (a % 32u);
        }
    }

    uint32_t period = oled_ack ? BLINK_SLOW : BLINK_FAST;
    for (;;) {
        led_on();
        delay(period);
        led_off();
        delay(period);
    }
}
