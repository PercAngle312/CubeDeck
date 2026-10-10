# Optional experimental test binaries. The production Makefile is untouched.
# Run make -f experiments/v2-probe/Makefile gamecube or gba at repository root.
DEVKITPRO ?= /opt/devkitpro
PPC := $(DEVKITPRO)/devkitPPC/bin/powerpc-eabi-gcc
ARM := $(DEVKITPRO)/devkitARM/bin/arm-none-eabi-gcc
ARMOBJ := $(DEVKITPRO)/devkitARM/bin/arm-none-eabi-objcopy
ELF2DOL := $(DEVKITPRO)/tools/bin/elf2dol
GBAFIX := $(DEVKITPRO)/tools/bin/gbafix
OUT := experiments/v2-probe/out
PROTO := protocol/transport_protocol.c protocol/session_guard.c

.PHONY: gamecube gba clean
gamecube: $(OUT)/cubedeck-v2-probe.dol
gba: $(OUT)/cubedeck-v2-probe.gba

$(OUT):
	mkdir -p $(OUT)

$(OUT)/cubedeck-v2-probe.elf: experiments/v2-probe/gamecube.c $(PROTO) | $(OUT)
	$(PPC) -O2 -Wall -Wextra -Wno-unused-parameter -mogc -mcpu=750 -meabi -mhard-float \
	  -I$(DEVKITPRO)/libogc/include -Iinclude \
	  $^ -L$(DEVKITPRO)/libogc/lib/cube -logc -lm -o $@

$(OUT)/cubedeck-v2-probe.dol: $(OUT)/cubedeck-v2-probe.elf
	$(ELF2DOL) $< $@

$(OUT)/cubedeck-v2-probe-arm.elf: experiments/v2-probe/gba.c $(PROTO) | $(OUT)
	$(ARM) -O2 -Wall -Wextra -mthumb -mthumb-interwork \
	  -I$(DEVKITPRO)/libgba/include -Iinclude -specs=gba.specs \
	  $^ -L$(DEVKITPRO)/libgba/lib -lgba -o $@

$(OUT)/cubedeck-v2-probe.gba: $(OUT)/cubedeck-v2-probe-arm.elf
	$(ARMOBJ) -O binary $< $@
	$(GBAFIX) $@ -tCDBKV2PROBE -cCDV2 -m01

clean:
	rm -rf $(OUT)
