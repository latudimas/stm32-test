# AGENTS.md

Bare-metal C firmware for an STM32F103 Blue Pill (64 KB flash, 20 KB RAM) driving
a 128x64 SSD1306 OLED over I2C. Background and hardware notes: `CONTEXT.md`.

## Build & flash
Tools come from the Nix flake (`direnv allow`, or `nix develop`).

```sh
make                  # build/firmware.elf (hardware I2C1 + DMA)
make I2C=bitbang      # bit-banged I2C instead
make test             # host regressions with ASan/UBSan (no board access)
make flash            # program via ST-Link + OpenOCD, verify, reset
make openocd          # GDB server on :3333; then `make gdb`
tio -b 115200 /dev/cu.usbmodem1102   # serial log (only one reader at a time)
```

## Layout
- `src/startup.c`, `stm32f103.ld`: vector table, reset handler, memory map
- `src/stm32f103.h`: hand-written register definitions
- `src/clock.c`: HSE -> PLL -> 72 MHz; rates in `cpu_hz` / `pclk1_hz` / `pclk2_hz`
- `src/systick.c`: `millis()`, `delay_ms()`, `delay_us()`
- `src/i2c_hw.c` / `src/i2c_bitbang.c`: same API from `src/i2c.h`
- `src/ssd1306.c`, `src/font5x7.c`: display driver, framebuffer, text
- `src/mpu9250.c`: accelerometer identity/configuration and fresh-sample reads
- `src/uart.c`: queued USART1 TX log on PA9; service `uart_poll()` in main
- `src/robot_eyes.c`, `src/frame_schedule.h`: eye animation and 60 Hz deadlines
- `src/motion.c`: oscillatory wobble/shake classifier; 1.2 s squint / 2 s dizzy quiet return
- `src/main.c`: motion-reactive eyes + accelerometer acquisition/logging

## Rules
- No vendor HAL/LL, CMSIS, Arduino or display libraries; no C library (`-nostdlib`).
- Every register write gets a comment citing its RM0008 (or PM0056) section.
- Derive timing from `cpu_hz` / `pclk*_hz`, never hard-coded clock values.
- Don't modify `ssd1306_fb` while `ssd1306_flush_poll()` returns `I2C_BUSY`.
- Keep it small and readable; build both `I2C=hw` and `I2C=bitbang` with no warnings.
