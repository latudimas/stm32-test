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
       src/uart.c src/systick.c src/clock.c src/robot_eyes.c src/mpu9250.c src/motion.c
OBJS = $(SRCS:src/%.c=$(BUILD)/%.o)
LDSCRIPT = stm32f103.ld

# -fno-tree-loop-distribute-patterns: stop GCC turning the .data/.bss loops in
# Reset_Handler into memcpy/memset calls, which don't exist with -nostdlib.
CFLAGS = -mcpu=cortex-m3 -mthumb -std=c11 -Os -g -Wall -Wextra \
         -ffreestanding -fno-tree-loop-distribute-patterns \
         -ffunction-sections -fdata-sections -MMD -MP
LDFLAGS = -nostdlib -T $(LDSCRIPT) -Wl,--gc-sections \
          -Wl,-Map=$(TARGET).map -Wl,--no-warn-rwx-segments

OPENOCD = openocd -f interface/stlink.cfg -f target/stm32f1x.cfg

.PHONY: all flash openocd gdb test verify clean FORCE

all: $(TARGET).elf $(TARGET).bin

-include $(OBJS:.o=.d)

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

# Host-only tests (HOST_CC defaults to cc), with ASan/UBSan. No flashing.
test:
	sh tests/run.sh

# Host-only CBMC proofs: properties hold for every input, not just samples.
# --unwinding-assertions fails the proof if a loop could run past --unwind.
CBMC_FLAGS = -Isrc --unwind 4 --unwinding-assertions --bounds-check \
             --pointer-check --signed-overflow-check --div-by-zero-check \
             --undefined-shift-check
verify:
	cbmc $(CBMC_FLAGS) verify/frame_schedule_proof.c

clean:
	rm -rf $(BUILD)
