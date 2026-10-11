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
on readiness, SD mount/GPT and runtime write-audit checks. The physical result
below confirms one successful readiness run; repeated startup and interrupted
sessions remain separate hardware questions.

## User-reported hardware PASS — October 11, 2026

The user supplied photo `IMG_1413.jpg` after receiving the v8 ZIP built from
source `9bb1e29ec2cf5919f190597474b37ffc3be49402`, CI run
[38165076417](https://github.com/PercAngle312/CubeDeck/actions/runs/38165076417).
This result belongs to that delivered build, not this later documentation
commit or a subsequent rebuild. Delivered DOL SHA-256:
`cd52ed4b81cef9115574099ba835151257643e11c82e065bc6702a7985144c1d`.
The checksum was checked in the ZIP, not independently on the console.

The photographed real GameCube screen shows:

- Storage driver 1, card 1, mount 1, storage error 0 and errno 0.
- `sd:/cubedeck/joyboot-probe.gba` loaded: 3200 bytes.
- GPT error 0, mounted entry 1, LBA 2048.
- GBA port 2, stage READY, STATUS polls 1, SI refusals 0.
- SI raw result 0, pending 0, callback 0.
- **PASS: BIOS ready and session key received.**

This confirms SD file reading followed by RESET/STATUS/session-key readiness
on the user's physical setup for one run. No payload was sent. GBA program
execution, full upload, cartridge reading and interrupted-session reliability
have not been established by this result. The photographed screen explicitly
states no cartridge access, SD reads only and no storage writes.

For one focused hardware test, leave the v7 files in place, copy only the new
DOL to SD root, connect the link cable to controller port 2, turn on GBA with no
cartridge and wait at its boot screen. Launch v8 in Swiss and press A once.
Photograph the final PASS or failure screen. Do not insert a cartridge during
this test. No SD/save/memory-card/GCMCE writes or cartridge access occur.
