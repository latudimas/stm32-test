/*
 * HELLO WORLD on the SSD1306, with debug output on USART1.
 * Clock: HSE 8 MHz -> PLL -> 72 MHz (clock.c), falls back to HSI 8 MHz.
 *
 * Serial log (PA9 -> debugger RXD): tio -b 115200 /dev/cu.usbmodem1102
 *
 * Result on the PC13 LED (active low):
 *   slow blink, 2 s on / 2 s off  -> init + flush ACKed; text should be on screen
 *   fast blink, ~0.1 s            -> a byte was NACKed
 *   LED stays off                 -> bus not idle (a line stuck low / no pull-ups)
 * The same results are in the globals below, readable from GDB/OpenOCD.
 */
#include "clock.h"
#include "i2c.h"
#include "ssd1306.h"
#include "stm32f103.h"
#include "systick.h"
#include "uart.h"

#define LED_PIN 13u

#define BLINK_SLOW_MS 2000u
#define BLINK_FAST_MS 100u
#define POWER_UP_MS   100u /* let the OLED's supply settle */

#define MESSAGE "HELLO WORLD"
/* 11 chars x 6 px = 66 px wide. Page 3 = y 24..31, the text row closest to
 * the vertical middle. */
#define MESSAGE_W ((int32_t)(sizeof MESSAGE - 1u) * SSD1306_CHAR_W)
#define MESSAGE_X ((SSD1306_WIDTH - (uint32_t)MESSAGE_W) / 2u) /* centred: 31 */
#define MESSAGE_PAGE 3u
#define TICK_X    MESSAGE_X
#define TICK_PAGE 6u

/* Running text: move 1 px left every frame. 30 ms/frame (~33 fps) leaves room
 * for the 23 ms DMA flush; one pass (128 + 66 px) takes ~5.8 s. */
#define FRAME_MS 30u
#define TICK_MS  4000u

volatile uint32_t i2c_bus_ok; /* 1 = SCL and SDA high at idle */
volatile uint32_t oled_init_ok;  /* 1 = every init command byte ACKed */
volatile uint32_t oled_flush_ok; /* 1 = whole framebuffer ACKed */
volatile uint32_t frames_drawn;  /* scroll frames handed to the display */
volatile uint32_t frame_errors;  /* frames whose transfer failed */

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
    /* Clock first: SysTick, UART and I2C timing are derived from it. */
    int hse_ok = clock_init();
    led_init();
    led_off();
    systick_init();
    uart_init();
    uart_puts("\n== stm32-test boot ==\n");
    uart_puts(hse_ok ? "clock: HSE+PLL " : "clock: HSE FAILED, HSI ");
    uart_put_u32(cpu_hz / 1000000u);
    uart_puts(" MHz\n");
    log_chip_info();

    delay_ms(POWER_UP_MS);
    i2c_init();

    uart_puts("i2c: ");
    uart_puts(i2c_driver);
    uart_putc('\n');

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

    /* Background flush: count how much other work the CPU gets done while
     * the 1024 bytes are on the wire (0 with the bit-bang driver). */
    uint32_t t0 = millis();
    uint32_t cpu_free_loops = 0;
    int r = I2C_ERR;
    if (ssd1306_flush_start()) {
        while ((r = ssd1306_flush_poll()) == I2C_BUSY) {
            cpu_free_loops++; /* stand-in for real work */
        }
    }
    uint32_t flush_ms = millis() - t0;
    oled_flush_ok = (r == I2C_DONE);
    log_result("oled flush", oled_flush_ok);
    uart_puts("flush time: ");
    uart_put_u32(flush_ms);
    uart_puts(" ms, cpu free loops during flush: ");
    uart_put_u32(cpu_free_loops);
    uart_putc('\n');

    /* Non-blocking main loop: each job runs when its time comes, judged by
     * millis(), so scrolling, LED and logging all keep going together.
     * (int32_t)(now - next) >= 0 is "now has reached next", wrap-safe. */
    uint32_t led_ms = (oled_init_ok && oled_flush_ok) ? BLINK_SLOW_MS : BLINK_FAST_MS;
    uint32_t now = millis();
    uint32_t next_frame = now, next_led = now, next_tick = now;
    int32_t scroll_x = SSD1306_WIDTH; /* start just off the right edge */
    uint32_t tick = 0;
    int led = 0;
    char tick_text[16];
    format_tick(tick, tick_text);

    for (;;) {
        now = millis();

        /* Scroll frame. Only touch the framebuffer once the previous DMA
         * transfer is over (poll != I2C_BUSY); otherwise try again next pass. */
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
