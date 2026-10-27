# Bare-metal STM32F103 build. Cross tools named explicitly: make's default CC is
# the host `cc`.
CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size
GDB     = arm-none-eabi-gdb

BUILD = build
TARGET = $(BUILD)/firmware
# I2C driver: hw (I2C1 peripheral, 400 kHz) or bitbang (GPIO). e.g. `make I2C=bitbang flash`
I2C ?= hw
SRCS = src/startup.c src/main.c src/i2c_$(I2C).c src/ssd1306.c src/font5x7.c \
       src/uart.c src/systick.c src/clock.c
OBJS = $(SRCS:src/%.c=$(BUILD)/%.o)
LDSCRIPT = stm32f103.ld

# -fno-tree-loop-distribute-patterns: stop GCC turning the .data/.bss loops in
# Reset_Handler into memcpy/memset calls, which don't exist with -nostdlib.
CFLAGS = -mcpu=cortex-m3 -mthumb -std=c11 -Os -g -Wall -Wextra \
         -ffreestanding -fno-tree-loop-distribute-patterns \
         -ffunction-sections -fdata-sections
LDFLAGS = -nostdlib -T $(LDSCRIPT) -Wl,--gc-sections \
          -Wl,-Map=$(TARGET).map -Wl,--no-warn-rwx-segments

OPENOCD = openocd -f interface/stlink.cfg -f target/stm32f1x.cfg

.PHONY: all flash openocd gdb clean FORCE

all: $(TARGET).elf $(TARGET).bin

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# Rewritten only when I2C changes, so switching drivers forces a relink even
# if the chosen driver's .o is older than the last firmware.elf.
$(BUILD)/config: FORCE | $(BUILD)
	@echo "I2C=$(I2C)" | cmp -s - $@ || echo "I2C=$(I2C)" > $@

$(TARGET).elf: $(OBJS) $(LDSCRIPT) $(BUILD)/config
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@
	$(SIZE) $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD):
	mkdir -p $@

flash: $(TARGET).elf
	$(OPENOCD) -c "program $< verify reset exit"

# Debug: `make openocd` in one terminal, `make gdb` in another.
openocd:
	$(OPENOCD)

gdb: $(TARGET).elf
	$(GDB) $< -ex "target extended-remote :3333" -ex "monitor reset halt"

clean:
	rm -rf $(BUILD)
