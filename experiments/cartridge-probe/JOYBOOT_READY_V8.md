# Joyboot BIOS readiness v8 — separate opt-in preflight

The hardware-verified SD2SP2 v7 build remains the storage baseline. The new
`cubedeck-joyboot-ready-v8.dol` uses its unchanged read-only GPT/FAT mount and
existing `sd:/cubedeck/joyboot-probe.gba` fixture, then starts only when A is
pressed. The ZIP contains the new DOL, instructions and checksums; it leaves
the working v7 DOL and probe file in place.

## Narrow hardware question

Can the real GBA BIOS complete RESET → boot-ready STATUS → session-key READ
through port 2 after CubeDeck reads the SD file? The STATUS flag and command
sequence follow the [FIX94 sender reference](https://github.com/FIX94/gba-link-cable-rom-sender/blob/master/source/main.c);
no asset bytes from that source are included. PASS requires all three steps,
including the BIOS STATUS boot-ready flag. No WRITE command, client key, header,
payload or cartridge command is sent. A PASS is readiness, not upload or GBA
execution. GBA may remain at its boot screen.

This is a prerequisite to the separate upload experiment. The current v7
storage fixture has its logo removed and is not BIOS-bootable. The existing
cartridge-reading payload is also a broader milestone. v8 avoids bundling
Nintendo assets and checks synchronization before tackling those dependencies.

## Bounds and ownership

The new controller permits one start per DOL. Every submitted callback has an
180-frame watchdog; the entire preflight has a 720-frame cap. Unready STATUS
is polled at most 20 times with a 15-frame delay. SI submission refusals can
retry within the current deadline, which refusals never extend. SI callback
errors, stale tokens, callbacks at/after their deadline and exhausted limits
fail terminally. A timeout does not cancel SI or free/reuse its buffers. The
static controller persists for the DOL lifetime; a failed run requires reboot.

The screen reports stage, failure stage/code, STATUS attempts, SI refusals,
last callback result/reply and active/callback flags. Error codes: 1 invalid
argument, 2 timeout, 3 SI callback error, 4 token mismatch, 5 boot-ready poll cap,
6 submission/wait deadline expired without a pending transfer.

## Automated and physical checks

Host tests simulate RESET/STATUS/READ ordering, delayed BIOS readiness, a normal
key read, SI errors, wrong tokens, late/missing callbacks, retained pending
buffers, forbidden restart, busy submission bounds, STATUS attempt exhaustion
and tick wraparound. They assert every submitted command is RESET, STATUS or
READ. CI runs ASan/UBSan and cross-compiles the distinct DOL; packaging depends
on readiness, SD mount/GPT and runtime write-audit checks. Real SI timing and
GBA readiness remain unverified until the user tests v8.

For one focused hardware test, leave the v7 files in place, copy only the new
DOL to SD root, connect the link cable to controller port 2, turn on GBA with no
cartridge and wait at its boot screen. Launch v8 in Swiss and press A once.
Photograph the final PASS or failure screen. Do not insert a cartridge during
this test. No SD/save/memory-card/GCMCE writes or cartridge access occur.
