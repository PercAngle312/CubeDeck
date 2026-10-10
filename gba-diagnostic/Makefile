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
GBAFIX := $(DEVKITPRO)/tools/bin/gbafix
# libgba is installed separately from devkitARM, so specify both its headers
# and library. Keep gba.specs to configure the GBA startup/runtime.
CFLAGS := -O2 -Wall -Wextra -mthumb -mthumb-interwork -I$(DEVKITPRO)/libgba/include -I../include -specs=gba.specs
LDFLAGS := -L$(DEVKITPRO)/libgba/lib
LDLIBS := -lgba

.PHONY: all clean check-tools
all: check-tools $(TARGET).gba

check-tools:
	@test -x "$(CC)" || (echo "Missing compiler: $(CC)"; exit 1)
	@test -x "$(OBJCOPY)" || (echo "Missing objcopy: $(OBJCOPY)"; exit 1)
	@test -x "$(GBAFIX)" || (echo "Missing gbafix: $(GBAFIX)"; exit 1)
	@test -f "$(DEVKITPRO)/libgba/include/gba.h" || (echo "Missing libgba header: $(DEVKITPRO)/libgba/include/gba.h"; exit 1)
	@test -f "$(DEVKITPRO)/libgba/lib/libgba.a" || (echo "Missing libgba library: $(DEVKITPRO)/libgba/lib/libgba.a"; exit 1)

$(TARGET).elf: main.c ../protocol/transport_protocol.c ../include/cubedeck/transport_protocol.h
	$(CC) $(CFLAGS) main.c ../protocol/transport_protocol.c $(LDFLAGS) $(LDLIBS) -o $@

$(TARGET).gba: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	$(GBAFIX) $@ -tCUBEDECKDIAG -cCDBG -m01

clean:
	rm -f $(TARGET).elf $(TARGET).gba
