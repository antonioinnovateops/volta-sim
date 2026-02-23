PREFIX  := arm-none-eabi-
CC      := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
SIZE    := $(PREFIX)size

TARGET  := uart_hello

# Reuse startup and linker script from blinky
BLINKY  := ../blinky
SRCS    := main.c $(BLINKY)/startup_stm32f407.s
LDSCRIPT := $(BLINKY)/stm32f407.ld

CFLAGS  := -mcpu=cortex-m4 -mthumb -mfloat-abi=soft \
           -Os -Wall -Wextra -std=c11 \
           -ffunction-sections -fdata-sections -ffreestanding -nostdlib

LDFLAGS := -T$(LDSCRIPT) -mcpu=cortex-m4 -mthumb -mfloat-abi=soft \
           --specs=nosys.specs -Wl,--gc-sections -nostartfiles

.PHONY: all clean

all: $(TARGET).elf $(TARGET).bin
	$(SIZE) $(TARGET).elf

$(TARGET).elf: $(SRCS) $(LDSCRIPT)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRCS) -o $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f $(TARGET).elf $(TARGET).bin
