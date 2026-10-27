/*
 * Milestone 6: HELLO WORLD on the SSD1306, with debug output on USART1.
 * Runs on the reset clock: HSI 8 MHz (RM0008 §7.2), no clock setup needed.
 *
 * Serial log (PA9 -> debugger RXD): tio -b 115200 /dev/cu.usbmodem1102
 *
 * Result on the PC13 LED (active low):
 *   slow blink, 2 s on / 2 s off  -> init + flush ACKed; text should be on screen
 *   fast blink, ~0.1 s            -> a byte was NACKed
 *   LED stays off                 -> bus not idle (a line stuck low / no pull-ups)
 * The same results are in the globals below, readable from GDB/OpenOCD.
 */
#include "delay.h"
#include "i2c.h"
#include "ssd1306.h"
#include "stm32f103.h"
#include "uart.h"

#define LED_PIN 13u

/* ~10 CPU cycles per delay() iteration at 8 MHz (measured) -> 1.6M iterations ~= 2 s. */
#define BLINK_SLOW 1600000u
#define BLINK_FAST 80000u
#define POWER_UP_DELAY 80000u /* ~100 ms for the OLED's supply to settle */

#define MESSAGE "HELLO WORLD"
/* 11 chars x 6 px = 66 px wide -> x = (128 - 66) / 2 = 31; page 3 = y 24..31,
 * the text row closest to the vertical middle. */
#define MESSAGE_X ((SSD1306_WIDTH - (sizeof MESSAGE - 1u) * 6u) / 2u)
#define MESSAGE_PAGE 3u

volatile uint32_t i2c_bus_ok; /* 1 = SCL and SDA high at idle */
volatile uint32_t oled_init_ok;  /* 1 = every init command byte ACKed */
volatile uint32_t oled_flush_ok; /* 1 = whole framebuffer ACKed */

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

static void log_result(const char *what, uint32_t ok)
{
    uart_puts(what);
    uart_puts(ok ? ": ok\n" : ": FAIL\n");
}

static void log_chip_info(void)
{
    const volatile uint32_t *uid = (const volatile uint32_t *)UID_BASE;

    uart_puts("flash: ");
    uart_put_u32(FLASH_SIZE_KB);
    uart_puts(" KB\nuid: ");
    for (int i = 0; i < 3; i++) {
        uart_put_hex32(uid[i]);
        uart_putc(i < 2 ? ' ' : '\n');
    }
}

int main(void)
{
    led_init();
    led_off();
    uart_init();
    uart_puts("\n== stm32-test boot ==\n");
    log_chip_info();

    delay(POWER_UP_DELAY);
    i2c_init();

    i2c_bus_ok = i2c_bus_idle();
    log_result("i2c bus idle", i2c_bus_ok);
    if (!i2c_bus_ok) {
        for (;;) {
        }
    }

    oled_init_ok = ssd1306_init();
    log_result("oled init", oled_init_ok);
    ssd1306_fill(0x00);
    ssd1306_draw_string(MESSAGE_X, MESSAGE_PAGE, MESSAGE);
    oled_flush_ok = ssd1306_flush();
    log_result("oled flush", oled_flush_ok);

    uint32_t period = (oled_init_ok && oled_flush_ok) ? BLINK_SLOW : BLINK_FAST;
    for (uint32_t tick = 0;; tick++) {
        uart_puts("tick ");
        uart_put_u32(tick);
        uart_putc('\n');
        led_on();
        delay(period);
        led_off();
        delay(period);
    }
}
