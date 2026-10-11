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

# Distinct opt-in multi-frame fixtures: never overwrite the cyan-verified probe.
.PHONY: multiframe-gamecube multiframe-gba
multiframe-gamecube: $(OUT)/cubedeck-v2-multiframe.dol
multiframe-gba: $(OUT)/cubedeck-v2-multiframe.gba

$(OUT)/cubedeck-v2-multiframe.elf: experiments/v2-probe/gamecube-multiframe.c $(PROTO) | $(OUT)
	$(PPC) -O2 -Wall -Wextra -Wno-unused-parameter -mogc -mcpu=750 -meabi -mhard-float \
	  -I$(DEVKITPRO)/libogc/include -Iinclude \
	  $^ -L$(DEVKITPRO)/libogc/lib/cube -logc -lm -o $@

$(OUT)/cubedeck-v2-multiframe.dol: $(OUT)/cubedeck-v2-multiframe.elf
	$(ELF2DOL) $< $@

$(OUT)/cubedeck-v2-multiframe-arm.elf: experiments/v2-probe/gba-multiframe.c $(PROTO) | $(OUT)
	$(ARM) -O2 -Wall -Wextra -mthumb -mthumb-interwork \
	  -I$(DEVKITPRO)/libgba/include -Iinclude -specs=gba.specs \
	  $^ -L$(DEVKITPRO)/libgba/lib -lgba -o $@

$(OUT)/cubedeck-v2-multiframe.gba: $(OUT)/cubedeck-v2-multiframe-arm.elf
	$(ARMOBJ) -O binary $< $@
	$(GBAFIX) $@ -tCDBK3FRAMES -cCDV3 -m01

# Opt-in ACK/NACK experiment; isolated from the verified three-frame pair.
.PHONY: ack-gamecube ack-gba
ack-gamecube: $(OUT)/cubedeck-v2-ack.dol
ack-gba: $(OUT)/cubedeck-v2-ack.gba

$(OUT)/cubedeck-v2-ack.elf: experiments/v2-probe/gamecube-ack.c $(PROTO) experiments/v2-probe/ack_control.h | $(OUT)
	$(PPC) -O2 -Wall -Wextra -Wno-unused-parameter -mogc -mcpu=750 -meabi -mhard-float \
	  -I$(DEVKITPRO)/libogc/include -Iinclude \
	  experiments/v2-probe/gamecube-ack.c $(PROTO) -L$(DEVKITPRO)/libogc/lib/cube -logc -lm -o $@

$(OUT)/cubedeck-v2-ack.dol: $(OUT)/cubedeck-v2-ack.elf
	$(ELF2DOL) $< $@

$(OUT)/cubedeck-v2-ack-arm.elf: experiments/v2-probe/gba-ack.c $(PROTO) experiments/v2-probe/ack_control.h | $(OUT)
	$(ARM) -O2 -Wall -Wextra -mthumb -mthumb-interwork \
	  -I$(DEVKITPRO)/libgba/include -Iinclude -specs=gba.specs \
	  experiments/v2-probe/gba-ack.c $(PROTO) -L$(DEVKITPRO)/libgba/lib -lgba -o $@

$(OUT)/cubedeck-v2-ack.gba: $(OUT)/cubedeck-v2-ack-arm.elf
	$(ARMOBJ) -O binary $< $@
	$(GBAFIX) $@ -tCDBKACKTEST -cCDAK -m01

# Separate unverified startup timing build; unchanged verified ACK target above.
.PHONY: ack-sync-gamecube
ack-sync-gamecube: $(OUT)/cubedeck-v2-ack-sync.dol

$(OUT)/cubedeck-v2-ack-sync.elf: experiments/v2-probe/gamecube-ack-sync.c $(PROTO) experiments/v2-probe/ack_control.h | $(OUT)
	$(PPC) -O2 -Wall -Wextra -Wno-unused-parameter -mogc -mcpu=750 -meabi -mhard-float \
	  -I$(DEVKITPRO)/libogc/include -Iinclude \
	  experiments/v2-probe/gamecube-ack-sync.c $(PROTO) -L$(DEVKITPRO)/libogc/lib/cube -logc -lm -o $@

$(OUT)/cubedeck-v2-ack-sync.dol: $(OUT)/cubedeck-v2-ack-sync.elf
	$(ELF2DOL) $< $@
