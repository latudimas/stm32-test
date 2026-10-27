/*
 * "HELLO WORLD" scrolling right-to-left on a 128x64 SSD1306, with a tick
 * counter below it. Status log on USART1: tio -b 115200 /dev/cu.usbmodem1102
 *
 * PC13 LED (active low): 2 s blink = display OK, fast blink = display error,
 * off = I2C bus stuck.
 */
#include "clock.h"
#include "i2c.h"
#include "ssd1306.h"
#include "stm32f103.h"
#include "systick.h"
#include "uart.h"

#define LED_PIN 13u

#define BLINK_OK_MS   2000u
#define BLINK_ERR_MS  100u
#define POWER_UP_MS   100u /* let the OLED's supply settle */

#define MESSAGE      "HELLO WORLD"
#define MESSAGE_W    ((int32_t)(sizeof MESSAGE - 1u) * SSD1306_CHAR_W) /* 66 px */
#define MESSAGE_PAGE 3u    /* y 24..31, closest text row to the middle */
#define TICK_X       ((SSD1306_WIDTH - (uint32_t)MESSAGE_W) / 2u)
#define TICK_PAGE    6u

/* 1 px per frame; 30 ms leaves room for the ~23 ms DMA flush (~33 fps). */
#define FRAME_MS 30u
#define TICK_MS  4000u

/* Read from the debugger to check the frame rate and errors. */
volatile uint32_t frames_drawn;
volatile uint32_t frame_errors;

static void led_init(void)
{
    /* RM0008 §7.3.7 RCC_APB2ENR: IOPCEN=1 — clock GPIOC. */
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

static void log_result(const char *what, int ok)
{
    uart_puts(what);
    uart_puts(ok ? ": ok\n" : ": FAIL\n");
}

/* "TICK 123" into out (needs 16 bytes). */
static void format_tick(uint32_t n, char *out)
{
    const char *prefix = "TICK ";
    while (*prefix) {
        *out++ = *prefix++;
    }
    char digits[10];
    int k = 0;
    do {
        digits[k++] = (char)('0' + n % 10u);
        n /= 10u;
    } while (n);
    while (k) {
        *out++ = digits[--k];
    }
    *out = '\0';
}

int main(void)
{
    /* Clock first: SysTick, UART and I2C timing are derived from it. */
    int hse_ok = clock_init();
    led_init();
    led_off();
    systick_init();
    uart_init();
    uart_puts("\n== stm32-test boot ==\n");
    uart_puts(hse_ok ? "clock: HSE+PLL " : "clock: HSE FAILED, HSI ");
    uart_put_u32(cpu_hz / 1000000u);
    uart_puts(" MHz\ni2c: ");
    uart_puts(i2c_driver);
    uart_putc('\n');

    delay_ms(POWER_UP_MS);
    i2c_init();

    int bus_ok = i2c_bus_idle();
    log_result("i2c bus idle", bus_ok);
    if (!bus_ok) {
        for (;;) {
        }
    }

    int oled_ok = ssd1306_init();
    log_result("oled init", oled_ok);
    ssd1306_fill(0x00);

    /* Non-blocking loop: each job runs when its time comes, judged by millis().
     * (int32_t)(now - next) >= 0 means "now has reached next", wrap-safe. */
    uint32_t led_ms = oled_ok ? BLINK_OK_MS : BLINK_ERR_MS;
    uint32_t now = millis();
    uint32_t next_frame = now, next_led = now, next_tick = now;
    int32_t scroll_x = SSD1306_WIDTH; /* start just off the right edge */
    uint32_t tick = 0;
    int led = 0;
    char tick_text[16];
    format_tick(tick, tick_text);

    for (;;) {
        now = millis();

        /* Only touch the framebuffer once the previous DMA transfer is over. */
        int status = ssd1306_flush_poll();
        if ((int32_t)(now - next_frame) >= 0 && status != I2C_BUSY) {
            if (status == I2C_ERR) {
                frame_errors++;
            }
            next_frame = now + FRAME_MS;

            ssd1306_clear_page(MESSAGE_PAGE);
            ssd1306_draw_string(scroll_x, MESSAGE_PAGE, MESSAGE);
            if (--scroll_x < -MESSAGE_W) {
                scroll_x = SSD1306_WIDTH; /* fully gone on the left: re-enter right */
            }
            ssd1306_clear_page(TICK_PAGE);
            ssd1306_draw_string(TICK_X, TICK_PAGE, tick_text);

            if (ssd1306_flush_start()) {
                frames_drawn++;
            } else {
                frame_errors++;
            }
        }

        if ((int32_t)(now - next_led) >= 0) {
            next_led += led_ms;
            led = !led;
            if (led) {
                led_on();
            } else {
                led_off();
            }
        }

        if ((int32_t)(now - next_tick) >= 0) {
            next_tick += TICK_MS;
            uart_puts("tick ");
            uart_put_u32(tick);
            uart_puts(" at ");
            uart_put_u32(now);
            uart_puts(" ms, frames ");
            uart_put_u32(frames_drawn);
            uart_puts(", frame errors ");
            uart_put_u32(frame_errors);
            uart_putc('\n');
            format_tick(++tick, tick_text); /* drawn by the next frame */
        }
    }
}
