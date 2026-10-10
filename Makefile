# CubeDeck - Nintendo GameCube homebrew starter
# Requires devkitPPC, libogc, and the devkitPro GameCube toolchain.

ifeq ($(DEVKITPRO),)
$(error DEVKITPRO is not set. Install devkitPro/devkitPPC and source the environment)
endif
ifeq ($(DEVKITPPC),)
$(error DEVKITPPC is not set. Install devkitPPC and source the environment)
endif

TARGET  := cubedeck
BUILD   := build
SOURCES := src
INCLUDES :=

CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
OFILES := $(CFILES:.c=.o)

VPATH := $(SOURCES)

CC      := $(DEVKITPPC)/bin/powerpc-eabi-gcc
OBJCOPY := $(DEVKITPPC)/bin/powerpc-eabi-objcopy
ELF2DOL := $(DEVKITPRO)/tools/bin/elf2dol

CFLAGS  := -O2 -Wall -Wextra -Wno-unused-parameter -mogc -mcpu=750 -meabi -mhard-float -I$(DEVKITPRO)/libogc/include $(INCLUDES)
LDFLAGS := -mogc -mcpu=750 -meabi -mhard-float -L$(DEVKITPRO)/libogc/lib/cube -logc -lm

.PHONY: all clean check-tools
all: check-tools $(TARGET).dol

check-tools:
	@test -x "$(CC)" || (echo "Missing compiler: $(CC)"; exit 1)
	@test -x "$(ELF2DOL)" || (echo "Missing elf2dol: $(ELF2DOL). Install devkitPro tools."; exit 1)

$(TARGET).elf: $(OFILES)
	$(CC) $(OFILES) $(LDFLAGS) -o $@

$(TARGET).dol: $(TARGET).elf
	$(ELF2DOL) $< $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OFILES) $(TARGET).elf $(TARGET).dol
