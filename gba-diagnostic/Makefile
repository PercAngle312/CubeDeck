# Build the standalone GBA diagnostic ROM with devkitARM/libgba.
ifeq ($(DEVKITPRO),)
$(error DEVKITPRO is not set)
endif
ifeq ($(DEVKITARM),)
$(error DEVKITARM is not set)
endif

TARGET := cubedeck-gba-diagnostic
CC := $(DEVKITARM)/bin/arm-none-eabi-gcc
OBJCOPY := $(DEVKITARM)/bin/arm-none-eabi-objcopy
# gba.specs is passed exactly once. Recent devkitARM GCC versions reject
# loading the specs file twice because it defines the link spec internally.
CFLAGS := -O2 -Wall -Wextra -mthumb -mthumb-interwork -specs=gba.specs

.PHONY: all clean check-tools
all: check-tools $(TARGET).gba

check-tools:
	@test -x "$(CC)" || (echo "Missing compiler: $(CC)"; exit 1)
	@test -x "$(OBJCOPY)" || (echo "Missing objcopy: $(OBJCOPY)"; exit 1)

$(TARGET).elf: main.c
	$(CC) $(CFLAGS) $< -o $@

$(TARGET).gba: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f $(TARGET).elf $(TARGET).gba
