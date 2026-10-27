# CONTEXT.md — Bare-metal STM32 "Hello World" on SSD1306 OLED

## Goal
Print `HELLO WORLD` on a 0.96" SSD1306 OLED driven by an STM32F103 "Blue Pill",
written **from scratch**: no Arduino, no STM32 HAL/LL, no display library.
Everything (startup, clock, GPIO, I2C, SSD1306 driver, font) is hand-written
against the reference manual.

The point is learning: every byte that touches hardware should be understood.

## Hardware

| Part | Details |
|---|---|
| Target MCU | STM32F103 Blue Pill (user believes **CBT6**, 128 KB flash; confirm chip marking — if C8T6, flash is 64 KB). Cortex-M3, 20 KB RAM, 8 MHz HSE crystal. Possible clone chip (CKS32/CS32). |
| Display | 0.96" OLED, 128×64, SSD1306, **4-pin I2C** (VCC, GND, SCL, SDA). I2C address **0x3C** (7-bit). Onboard pull-ups assumed. |
| Debug probe | **WeAct Studio Mini Debugger, ST-Link V2.1 variant** (STM32F103CBT6 on probe). SWD + SWO + USB-UART bridge, USB-C. |
| Board USB | Blue Pill micro-USB is **power only**; not used for flashing (ROM bootloader is UART-only; no DFU bootloader installed). |
| Breadboard | Blue Pill and OLED on one breadboard. |

### Boot jumpers
Both BOOT0 and BOOT1 jumpers in the **0** position (boot from flash).

## Wiring

### OLED → Blue Pill (all on the left header, bottom)
| OLED | Blue Pill | Function |
|---|---|---|
| SCL | **PB6** | I2C1_SCL |
| SDA | **PB7** | I2C1_SDA |
| GND | GND | ground |
| VCC | **3.3** | power (use 3.3 V, not 5 V) |

⚠️ OLED pin order varies between modules (GND-VCC-SCL-SDA vs VCC-GND-SCL-SDA). Follow the silkscreen.

### Debugger → Blue Pill SWD header (match labels, not position)
| Debugger | Blue Pill |
|---|---|
| 3V3 | 3.3 |
| SWDIO | DIO |
| SWCLK | CLK |
| GND | GND |
| NRST (optional, recommended) | R / RES (right header) |
| TXD (optional) | PA10 (USART1_RX) |
| RXD (optional) | PA9 (USART1_TX) |

Power the Blue Pill from the debugger's 3V3 **or** its USB, not both.

## Tooling decisions

### Language — OPEN, recommendation: C first, Rust second
- **C**: 1:1 with RM0008, every example online is C, toolchain is just `arm-none-eabi-gcc`. Downside: everything manual, easy to forget `volatile`.
- **Rust**: `no_std` (no `std` on bare metal — only `core`). Best tooling (`cargo` + `probe-rs`). Truly crate-free means hand-written vector table (`#[link_section]`), `#[panic_handler]`, lots of `unsafe`. Pragmatic middle ground: `cortex-m-rt` + PAC only (no HAL).
- Note: "std lib" is not available in any language here. In C, avoid newlib; use `-nostdlib -ffreestanding` and write tiny `memset`/`memcpy` if the compiler emits calls to them.

### Build system
- **C** → Makefile (`arm-none-eabi-gcc`, custom linker script, `objcopy` to `.bin`).
- **Rust** → Cargo, target `thumbv7m-none-eabi`, linker script via `.cargo/config.toml`.

## Tooling

### Common (both languages)
| Tool | Purpose |
|---|---|
| `openocd` | Talks to the ST-Link over SWD: flash, reset, GDB server on `:3333` |
| `arm-none-eabi-gdb` (or `gdb-multiarch`) | Step through code, inspect registers/RAM |
| `stlink` tools (`st-info`, `st-flash`) | Optional quick probe check / raw `.bin` flashing (`st-info --probe`) |
| `picocom` / `minicom` / `screen` | Serial terminal for the debugger's USB-UART bridge (115200 8N1), milestone 6 |
| STM32F103 `.svd` file | Optional: named peripheral registers in GDB/IDE (e.g. via `cortex-debug` in VS Code) |

### C toolchain
| Tool | Purpose |
|---|---|
| `arm-none-eabi-gcc` (ARM GNU Toolchain / `gcc-arm-embedded`) | Compiler + linker. Flags: `-mcpu=cortex-m3 -mthumb -ffreestanding -nostdlib -Os -g -Wall` |
| `arm-none-eabi-objcopy` | `.elf` → `.bin` / `.hex` |
| `arm-none-eabi-size`, `arm-none-eabi-objdump` | Check flash/RAM usage, disassemble, verify vector table at `0x08000000` |
| `make` | Build system |

### Rust toolchain
| Tool | Purpose |
|---|---|
| `rustup` + target `thumbv7m-none-eabi` | `rustup target add thumbv7m-none-eabi` |
| `probe-rs` (`probe-rs-tools`) | Flash + run + RTT logging: `probe-rs run --chip STM32F103CB` (replaces OpenOCD for Rust) |
| `cargo-binutils` + `llvm-tools` component | `cargo objdump`, `cargo size`, `cargo objcopy` (`rustup component add llvm-tools`) |
| `flip-link` (optional) | Stack-overflow protection linker wrapper |

### Nix devShell (user already uses Nix flakes)
Provide a `flake.nix` with a devShell. Package names (verify against current nixpkgs):
- C: `gcc-arm-embedded`, `gnumake`, `openocd`, `stlink`, `picocom`
- Rust: `rustup` (or a rust-overlay toolchain with the `thumbv7m-none-eabi` target + `llvm-tools`), `probe-rs-tools`, `cargo-binutils`, `flip-link`

### OS setup notes
- **Linux / NixOS**: needs udev rules for ST-Link (`services.udev.packages = [ pkgs.stlink pkgs.openocd ];` on NixOS) and the user in the `dialout` group for `/dev/ttyACM*`.
- **macOS**: no driver needed; ST-Link appears as USB device, UART as `/dev/cu.usbmodem*`.
- Verify the ST-Link V2.1 firmware is recent; update with STM32CubeProgrammer / ST-Link Upgrade utility only if OpenOCD complains.

### Flash / debug
- `openocd -f interface/stlink.cfg -f target/stm32f1x.cfg`
- If error "unexpected idcode" → clone chip: add `-c "set CPUTAPID 0"` before the target cfg.
- Flash: `openocd ... -c "program build/firmware.elf verify reset exit"`
- Debug: `arm-none-eabi-gdb build/firmware.elf` → `target extended-remote :3333`
- Rust alternative: `probe-rs run --chip STM32F103C8` (or `STM32F103CB`).

## Firmware components to write

1. **Linker script**: FLASH @ `0x08000000` (64K safe default, 128K if CBT6 confirmed), RAM @ `0x20000000`, 20K. Sections: `.isr_vector`, `.text`, `.rodata`, `.data` (load in flash, copy to RAM), `.bss`.
2. **Startup**: vector table (initial SP + Reset_Handler + default handlers), Reset_Handler copies `.data`, zeroes `.bss`, calls `main`.
3. **Clock**: start on HSI 8 MHz (simplest). Optional later: HSE 8 MHz → PLL ×9 = 72 MHz (needs FLASH latency = 2 wait states).
4. **GPIO**: enable `RCC_APB2ENR.IOPBEN`; PB6/PB7 as **alternate-function open-drain, 50 MHz** (CRL bits) for hardware I2C, or **general-purpose open-drain** for bit-banged I2C.
5. **I2C** — two options:
   - **Bit-banged** (recommended first): simplest, easy to debug, immune to F103 I2C errata.
   - **Hardware I2C1** via registers (`RCC_APB1ENR.I2C1EN`, CR2 FREQ, CCR, TRISE, start/addr/data/stop sequence). Beware F103 I2C errata (bus can lock busy → need SWRST recovery).
6. **SSD1306 driver**: control byte `0x00` = command, `0x40` = data. Standard init sequence (display off, clock div, multiplex 0x3F, offset, start line, charge pump 0x14, memory mode horizontal, segment remap, COM scan dir, COM pins 0x12, contrast, precharge, VCOMH, display on). Framebuffer `uint8_t fb[128*64/8]` (1024 bytes), flush whole buffer.
7. **Font**: 5×7 (or 5×8) bitmap, column-major, only the glyphs needed (`H E L O W D` + space), or full ASCII 0x20–0x7E if cheap.
8. **main**: init clock → GPIO → I2C → SSD1306 → clear → draw "HELLO WORLD" → flush → loop forever.

## Milestones (each verifiable)
1. **Toolchain + probe**: OpenOCD detects Cortex-M3. ✅ when "Cortex-M3 r1p1 processor detected".
2. **Blink PC13** (onboard LED, active low) → proves startup, linker script, RCC, GPIO.
3. **I2C ACK**: send address 0x3C, check ACK (blink pattern or inspect in GDB) → proves wiring + I2C.
4. **Display on**: init sequence, fill screen white → proves SSD1306 init.
5. **Hello world**: render text from font into framebuffer.
6. (Optional) USART1 debug prints through the debugger's UART bridge.
7. (Optional) Port to Rust.

## Key references
- RM0008 — STM32F101/102/103 reference manual (RCC, GPIO, I2C, memory map)
- STM32F103x8/xB datasheet (pinout, AF mapping)
- ES0340 — STM32F10xx errata (I2C issues)
- PM0056 — Cortex-M3 programming manual (vector table, NVIC)
- SSD1306 datasheet (command table, init sequence)

## Conventions for Claude Code
- No vendor HAL/LL, no Arduino, no display libraries. CMSIS device headers are acceptable **only** if user agrees; default is hand-defined register addresses/structs.
- Every register write gets a comment citing the RM0008 section/register name.
- Keep the project small and readable; prefer clarity over cleverness.
- Ask before picking C vs Rust if still undecided.
