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

# Isolated startup/recovery fixture; no proven ACK or production input changes.
.PHONY: startup-gamecube startup-gba
startup-gamecube: $(OUT)/cubedeck-v2-startup.dol
startup-gba: $(OUT)/cubedeck-v2-startup.gba

$(OUT)/cubedeck-v2-startup.elf: experiments/v2-probe/gamecube-startup.c protocol/joybus_recovery.c experiments/v2-probe/startup_fixture.h include/cubedeck/joybus_recovery.h | $(OUT)
	$(PPC) -O2 -Wall -Wextra -Werror -mogc -mcpu=750 -meabi -mhard-float \
	  -I$(DEVKITPRO)/libogc/include -Iinclude \
	  experiments/v2-probe/gamecube-startup.c protocol/joybus_recovery.c \
	  -L$(DEVKITPRO)/libogc/lib/cube -logc -lm -o $@

$(OUT)/cubedeck-v2-startup.dol: $(OUT)/cubedeck-v2-startup.elf
	$(ELF2DOL) $< $@

$(OUT)/cubedeck-v2-startup-arm.elf: experiments/v2-probe/gba-startup.c experiments/v2-probe/startup_fixture.h | $(OUT)
	$(ARM) -O2 -Wall -Wextra -Werror -mthumb -mthumb-interwork \
	  -I$(DEVKITPRO)/libgba/include -Iinclude -specs=gba.specs \
	  experiments/v2-probe/gba-startup.c -L$(DEVKITPRO)/libgba/lib -lgba -o $@

$(OUT)/cubedeck-v2-startup.gba: $(OUT)/cubedeck-v2-startup-arm.elf
	$(ARMOBJ) -O binary $< $@
	$(GBAFIX) $@ -tCDBKSTARTUP -cCDSR -m01

# New recovery + CRC/ACK fixture; all earlier artifact filenames remain distinct.
RECOVERY := experiments/v2-probe/recovery_packet.c
RECOVERY_HDR := experiments/v2-probe/recovery_packet.h
.PHONY: recovery-gamecube recovery-gba
recovery-gamecube: $(OUT)/cubedeck-v2-recovery.dol
recovery-gba: $(OUT)/cubedeck-v2-recovery.gba

$(OUT)/cubedeck-v2-recovery.elf: experiments/v2-probe/gamecube-recovery.c experiments/v2-probe/gamecube-startup.c protocol/joybus_recovery.c include/cubedeck/joybus_recovery.h $(PROTO) $(RECOVERY) $(RECOVERY_HDR) experiments/v2-probe/startup_fixture.h | $(OUT)
	$(PPC) -O2 -Wall -Wextra -Werror -mogc -mcpu=750 -meabi -mhard-float \
	  -I$(DEVKITPRO)/libogc/include -Iinclude \
	  experiments/v2-probe/gamecube-recovery.c protocol/joybus_recovery.c $(PROTO) $(RECOVERY) \
	  -L$(DEVKITPRO)/libogc/lib/cube -logc -lm -o $@

$(OUT)/cubedeck-v2-recovery.dol: $(OUT)/cubedeck-v2-recovery.elf
	$(ELF2DOL) $< $@

$(OUT)/cubedeck-v2-recovery-arm.elf: experiments/v2-probe/gba-recovery.c $(PROTO) $(RECOVERY) $(RECOVERY_HDR) | $(OUT)
	$(ARM) -O2 -Wall -Wextra -Werror -mthumb -mthumb-interwork \
	  -I$(DEVKITPRO)/libgba/include -Iinclude -specs=gba.specs \
	  experiments/v2-probe/gba-recovery.c $(PROTO) $(RECOVERY) \
	  -L$(DEVKITPRO)/libgba/lib -lgba -o $@

$(OUT)/cubedeck-v2-recovery.gba: $(OUT)/cubedeck-v2-recovery-arm.elf
	$(ARMOBJ) -O binary $< $@
	$(GBAFIX) $@ -tCDBKRECOVERY -cCDRC -m01

# Separate 1 KiB RAM-only transfer: does not link the three-packet fixture.
BULK := experiments/v2-probe/bulk_packet.c
BULK_HDR := experiments/v2-probe/bulk_packet.h experiments/v2-probe/bulk_adapter.h
.PHONY: bulk-gamecube bulk-gba
bulk-gamecube: $(OUT)/cubedeck-v2-bulk.dol
bulk-gba: $(OUT)/cubedeck-v2-bulk.gba

$(OUT)/cubedeck-v2-bulk.elf: experiments/v2-probe/gamecube-bulk.c experiments/v2-probe/gamecube-startup.c protocol/joybus_recovery.c include/cubedeck/joybus_recovery.h $(PROTO) $(BULK) $(BULK_HDR) experiments/v2-probe/startup_fixture.h | $(OUT)
	$(PPC) -O2 -Wall -Wextra -Werror -mogc -mcpu=750 -meabi -mhard-float \
	  -I$(DEVKITPRO)/libogc/include -Iinclude \
	  experiments/v2-probe/gamecube-bulk.c protocol/joybus_recovery.c $(PROTO) $(BULK) \
	  -L$(DEVKITPRO)/libogc/lib/cube -logc -lm -o $@

$(OUT)/cubedeck-v2-bulk.dol: $(OUT)/cubedeck-v2-bulk.elf
	$(ELF2DOL) $< $@

$(OUT)/cubedeck-v2-bulk-arm.elf: experiments/v2-probe/gba-bulk.c experiments/v2-probe/gba-recovery.c $(PROTO) $(BULK) $(BULK_HDR) | $(OUT)
	$(ARM) -O2 -Wall -Wextra -Werror -mthumb -mthumb-interwork \
	  -I$(DEVKITPRO)/libgba/include -Iinclude -specs=gba.specs \
	  experiments/v2-probe/gba-bulk.c $(PROTO) $(BULK) \
	  -L$(DEVKITPRO)/libgba/lib -lgba -o $@

$(OUT)/cubedeck-v2-bulk.gba: $(OUT)/cubedeck-v2-bulk-arm.elf
	$(ARMOBJ) -O binary $< $@
	$(GBAFIX) $@ -tCDBK1KRAM -cCDBR -m01

# Separate opt-in 16 KiB RAM-only transfer: does not link the three-packet fixture.
BULK := experiments/v2-probe/bulk_packet.c
BULK_HDR := experiments/v2-probe/bulk_packet.h experiments/v2-probe/bulk_adapter.h
.PHONY: ram16-gamecube ram16-gba
ram16-gamecube: $(OUT)/cubedeck-v2-ram16.dol
ram16-gba: $(OUT)/cubedeck-v2-ram16.gba

$(OUT)/cubedeck-v2-ram16.elf: experiments/v2-probe/gamecube-ram16.c experiments/v2-probe/gamecube-bulk.c experiments/v2-probe/gamecube-startup.c protocol/joybus_recovery.c include/cubedeck/joybus_recovery.h $(PROTO) $(BULK) $(BULK_HDR) experiments/v2-probe/startup_fixture.h | $(OUT)
	$(PPC) -DCUBEDECK_RAM16_TRANSFER -O2 -Wall -Wextra -Werror -mogc -mcpu=750 -meabi -mhard-float \
	  -I$(DEVKITPRO)/libogc/include -Iinclude \
	  experiments/v2-probe/gamecube-ram16.c protocol/joybus_recovery.c $(PROTO) $(BULK) \
	  -L$(DEVKITPRO)/libogc/lib/cube -logc -lm -o $@

$(OUT)/cubedeck-v2-ram16.dol: $(OUT)/cubedeck-v2-ram16.elf
	$(ELF2DOL) $< $@

$(OUT)/cubedeck-v2-ram16-arm.elf: experiments/v2-probe/gba-ram16.c experiments/v2-probe/gba-bulk.c experiments/v2-probe/gba-recovery.c $(PROTO) $(BULK) $(BULK_HDR) | $(OUT)
	$(ARM) -DCUBEDECK_RAM16_TRANSFER -O2 -Wall -Wextra -Werror -mthumb -mthumb-interwork \
	  -I$(DEVKITPRO)/libgba/include -Iinclude -specs=gba.specs \
	  experiments/v2-probe/gba-ram16.c $(PROTO) $(BULK) \
	  -L$(DEVKITPRO)/libgba/lib -lgba -o $@

$(OUT)/cubedeck-v2-ram16.gba: $(OUT)/cubedeck-v2-ram16-arm.elf
	$(ARMOBJ) -O binary $< $@
	$(GBAFIX) $@ -tCDBK16KRAM -cCD16 -m01
