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
# The library is installed outside devkitARM's default include search path.
# gba.specs is passed exactly once.
CFLAGS := -O2 -Wall -Wextra -mthumb -mthumb-interwork -I$(DEVKITPRO)/libgba/include -specs=gba.specs

.PHONY: all clean check-tools
all: check-tools $(TARGET).gba

check-tools:
	@test -x "$(CC)" || (echo "Missing compiler: $(CC)"; exit 1)
	@test -x "$(OBJCOPY)" || (echo "Missing objcopy: $(OBJCOPY)"; exit 1)
	@test -f "$(DEVKITPRO)/libgba/include/gba.h" || (echo "Missing libgba header: $(DEVKITPRO)/libgba/include/gba.h"; exit 1)
	@test -f "$(DEVKITPRO)/libgba/lib/libgba.a" || (echo "Missing libgba library: $(DEVKITPRO)/libgba/lib/libgba.a"; exit 1)

$(TARGET).elf: main.c
	$(CC) $(CFLAGS) $< -o $@

$(TARGET).gba: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f $(TARGET).elf $(TARGET).gba
