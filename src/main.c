/*
 * Animated robot eyes on a 128x64 SSD1306: blink and look left/right.
 * Status log on USART1: tio -b 115200 /dev/cu.usbmodem1102
 *
 * PC13 LED (active low): 2 s blink = display OK, fast blink = display error,
 * off = I2C bus stuck.
 */
#include "clock.h"
#include "frame_schedule.h"
#include "i2c.h"
#include "mpu9250.h"
#include "robot_eyes.h"
#include "ssd1306.h"
#include "stm32f103.h"
#include "systick.h"
#include "uart.h"

#define LED_PIN 13u

#define BLINK_OK_MS   2000u
#define BLINK_ERR_MS  100u
#define POWER_UP_MS   100u /* let the OLED's supply settle */

#define TICK_MS  4000u
#define SENSOR_RETRY_MS 5000u
#define SENSOR_STALE_MS 500u

/* Read from the debugger to check the frame rate and errors. */
volatile uint32_t frames_drawn;
volatile uint32_t frames_completed;
volatile uint32_t frame_errors;
volatile uint32_t max_flush_ms;
volatile uint32_t sensor_samples, sensor_errors, sensor_not_ready;
volatile uint32_t max_sensor_read_ms;

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

static void log_sample(const struct mpu9250_sample *sample)
{
    /* CSV acquisition time + raw signed counts. No per-axis calibration yet. */
    uart_puts("accel,");
    uart_put_u32(sample->ms);
    uart_putc(',');
    uart_put_i32(sample->x);
    uart_putc(',');
    uart_put_i32(sample->y);
    uart_putc(',');
    uart_put_i32(sample->z);
    uart_putc('\n');
}

static void log_sensor(const struct mpu9250 *dev, int ok)
{
    log_result("mpu9250 init", ok);
    uart_puts("mpu address/identity (decimal): ");
    uart_put_u32(dev->addr);
    uart_putc('/');
    uart_put_u32(dev->who);
    uart_puts("; expected 104 or 105 / 113\n");
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
        uart_flush();
        for (;;) {
        }
    }

    int oled_ok = ssd1306_init();
    log_result("oled init", oled_ok);
    ssd1306_fill(0x00);
    /* Clear the whole panel once, including pixels outside the eye window. */
    int panel_cleared = ssd1306_flush();
    log_result("oled clear", panel_cleared);
    if (!panel_cleared) {
        frame_errors++;
    }

    struct mpu9250 sensor;
    int sensor_ok = mpu9250_init(&sensor);
    log_sensor(&sensor, sensor_ok);
    uart_puts("accel CSV: accel,ms,x_raw,y_raw,z_raw (16384 counts/g nominal)\n");

    /* Non-blocking loop: each job runs when its time comes, judged by millis().
     * (int32_t)(now - next) >= 0 means "now has reached next", wrap-safe. */
    uint32_t led_ms = oled_ok ? BLINK_OK_MS : BLINK_ERR_MS;
    uint32_t now = millis();
    uint32_t next_frame = now, next_led = now, next_tick = now;
    uint32_t last_animation_ms = now, animation_ms = 0;
    uint32_t tick = 0, frame_phase = 0;
    uint32_t flush_started_ms = now, last_log_ms = now;
    uint32_t last_log_frames = 0;
    uint32_t next_sensor_retry = now + SENSOR_RETRY_MS;
    uint32_t sensor_fail_streak = 0;
    uint32_t last_sensor_sample_ms = now;
    int transfer_pending = 0;
    int led = 0;

    for (;;) {
        uart_poll();
        now = millis();

        /* Only touch the framebuffer once the previous DMA transfer is over. */
        int status = ssd1306_flush_poll();
        if (transfer_pending && status != I2C_BUSY) {
            transfer_pending = 0;
            if (status == I2C_ERR) {
                frame_errors++;
            } else {
                frames_completed++;
                uint32_t duration = now - flush_started_ms;
                if (duration > max_flush_ms) {
                    max_flush_ms = duration;
                }
            }
        }
        /* Retry a failed startup clear, without starving LED/status jobs. */
        if (!panel_cleared && (int32_t)(now - next_frame) >= 0 && status != I2C_BUSY) {
            ssd1306_fill(0);
            panel_cleared = ssd1306_flush();
            if (!panel_cleared) {
                frame_errors++;
            }
            now = millis();
            frame_schedule_next(&next_frame, &frame_phase, now);
        }
        if (panel_cleared && (int32_t)(now - next_frame) >= 0 && status != I2C_BUSY) {
            /* The sensor and OLED share I2C1. Only acquire after the previous
             * flush completed, before starting another. No concurrent START. */
            if (!sensor_ok && (int32_t)(now - next_sensor_retry) >= 0) {
                sensor_ok = mpu9250_init(&sensor);
                log_sensor(&sensor, sensor_ok);
                sensor_fail_streak = 0;
                last_sensor_sample_ms = millis();
                next_sensor_retry = millis() + SENSOR_RETRY_MS;
            }
            struct mpu9250_sample sample;
            int fresh = 0;
            if (sensor_ok) {
                uint32_t read_start = millis();
                fresh = mpu9250_read(&sensor, &sample);
                uint32_t duration = millis() - read_start;
                if (duration > max_sensor_read_ms) {
                    max_sensor_read_ms = duration;
                }
                if (fresh < 0) {
                    sensor_errors++;
                    if (++sensor_fail_streak >= 3u) {
                        sensor_ok = 0;
                        next_sensor_retry = millis() + SENSOR_RETRY_MS;
                        uart_puts("mpu9250: read failures; default eyes continue\n");
                    }
                } else {
                    sensor_fail_streak = 0;
                    if (fresh) {
                        sensor_samples++;
                        last_sensor_sample_ms = sample.ms;
                    } else {
                        sensor_not_ready++;
                        if (millis() - last_sensor_sample_ms >= SENSOR_STALE_MS) {
                            sensor_ok = 0;
                            next_sensor_retry = millis() + SENSOR_RETRY_MS;
                            uart_puts("mpu9250: stale data; default eyes continue\n");
                        }
                    }
                }
            }
            now = millis();
            /* Accumulate elapsed time modulo the loop, including millis()
             * wrap and slow bit-bang frames, without changing animation speed. */
            animation_ms = (animation_ms + (uint32_t)(now - last_animation_ms)) % 8000u;
            last_animation_ms = now;
            robot_eyes_draw(animation_ms);

            flush_started_ms = millis();
            if (ssd1306_flush_region_start(ROBOT_EYES_X, ROBOT_EYES_PAGE,
                                           ROBOT_EYES_WIDTH, ROBOT_EYES_PAGES)) {
                frames_drawn++;
                transfer_pending = 1;
            } else {
                frame_errors++;
            }
            /* Use time after the start call: bit-bang may have blocked for
             * an entire transfer. Skip missed slots rather than catch up. */
            frame_schedule_next(&next_frame, &frame_phase, millis());
            if (fresh == 1) {
                log_sample(&sample); /* queue UART text while OLED DMA runs */
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
            uart_puts(", fps x10 ");
            uint32_t elapsed = now - last_log_ms;
            uart_put_u32(elapsed ? (frames_completed - last_log_frames) * 10000u / elapsed : 0);
            last_log_ms = now;
            last_log_frames = frames_completed;
            uart_puts(", max flush ms ");
            uart_put_u32(max_flush_ms);
            uart_puts(", frame errors ");
            uart_put_u32(frame_errors);
            uart_puts(", samples ");
            uart_put_u32(sensor_samples);
            uart_puts(", sensor errors ");
            uart_put_u32(sensor_errors);
            uart_puts(", not ready ");
            uart_put_u32(sensor_not_ready);
            uart_puts(", max read ms ");
            uart_put_u32(max_sensor_read_ms);
            uart_putc('\n');
            tick++;
        }
    }
}
