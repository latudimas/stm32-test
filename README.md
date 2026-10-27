# stm32-test

"HELLO WORLD" scrolling across a 0.96" SSD1306 OLED, driven by an STM32F103
Blue Pill, written from scratch in C. No HAL, no CMSIS, no Arduino, no display
library, no C library: every register is set by hand from the reference manual.

## What's inside
- Startup code, vector table and linker script
- 72 MHz clock from the 8 MHz crystal (HSE + PLL), HSI fallback
- SysTick timebase: `millis()`, `delay_ms()`, `delay_us()`
- I2C two ways: hardware I2C1 at 400 kHz, or bit-banged GPIO
- DMA + interrupt screen updates (full 1 KB frame in ~23 ms, CPU free meanwhile)
- SSD1306 driver with a 5x7 ASCII font and clipped text drawing
- USART1 status log over the debugger's USB-UART bridge

## Hardware
| Part | Notes |
|---|---|
| STM32F103 Blue Pill | 64 KB flash, 20 KB RAM, 8 MHz crystal |
| SSD1306 128x64 OLED | 4-pin I2C, address `0x3C` |
| ST-Link V2.1 probe | e.g. WeAct Mini Debugger (SWD + USB-UART) |

### Wiring
| From | To (Blue Pill) |
|---|---|
| OLED SCL | PB6 |
| OLED SDA | PB7 |
| OLED VCC / GND | 3.3 / GND |
| Probe 3V3 / SWDIO / SWCLK / GND | 3.3 / DIO / CLK / GND |
| Probe RXD | PA9 (USART1 TX) |

Both BOOT jumpers at 0. Power the board from the probe or USB, not both.

## Build and flash
Requires [Nix](https://nixos.org) with flakes; the dev shell provides the ARM
toolchain, make, OpenOCD and tio.

```sh
direnv allow          # or: nix develop
make flash            # build and program over SWD
tio -b 115200 /dev/cu.usbmodem1102   # watch the log (port name varies)
```

`make I2C=bitbang flash` uses the bit-banged I2C driver instead.
`make openocd` + `make gdb` (two terminals) for debugging.

## Status LED (PC13)
- 2 s blink: running, display OK
- fast blink: display error
- off: I2C bus stuck

## More
- `CONTEXT.md`: design notes, register readouts and milestone history
- `AGENTS.md`: layout and coding rules
