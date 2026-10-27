# Bare-metal STM32F103 build. Cross tools named explicitly: make's default CC is
# the host `cc`.
CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size
GDB     = arm-none-eabi-gdb

BUILD = build
TARGET = $(BUILD)/firmware
SRCS = src/startup.c src/main.c src/i2c.c src/ssd1306.c src/font5x7.c src/uart.c
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

.PHONY: all flash openocd gdb clean

all: $(TARGET).elf $(TARGET).bin

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJS) $(LDSCRIPT)
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
