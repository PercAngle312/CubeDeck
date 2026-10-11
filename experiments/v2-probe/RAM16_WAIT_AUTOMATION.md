# 16 KiB verdict-wait emulator automation gate

This is the next validation objective for the isolated `ram16-wait` pair.
The existing CI run 38104731194 passed all 30 reported jobs,
including the four-session and interrupted-transfer **host SI simulation**.
That result is not a new Dolphin run. Existing single-image Dolphin evidence
is in `RAM16_WAIT_EMULATOR.md`.

## Why this is not yet an unattended GitHub Actions test

The previous emulator evidence used Dolphin 2407 with integrated mGBA,
a locally provided GBA BIOS, a qualified local evdev guard, Xvfb,
and pipe-injected controller input. Merely cross-compiling DOL/GBA or
installing the distribution's Dolphin package would not reproduce it.

Do **not** commit or publish BIOS files, user dumps, emulator save
containers, memory cards, proprietary Nintendo code or generated binaries.
Do not print BIOS contents into workflow logs. No automatic workflow should
download unofficial BIOS images.

## Reproducible validation prerequisites

1. Use a known version of Dolphin with **GBA (Integrated)** and the
   documented emulator qualifications in `EMULATOR.md`. Log its precise
   binary revision and any local emulator patch.
2. Supply a legally dumped 16,384-byte BIOS locally, outside the repository.
   Validate its byte length without logging its contents. An unattended
   runner requires a separately authorized, confidential input mechanism;
   never make an artifact containing the BIOS.
3. Obtain `cubedeck-v2-ram16-wait.dol` and matching
   `cubedeck-v2-ram16-wait.gba` from the **same** passing build.
   Record SHA-256 of the two generated files; do not mix earlier variants.
4. Run with a disposable isolated Dolphin profile. Configure GameCube
   controller Port 1, integrated GBA Port 2, software graphics, NoAudio,
   disabled memory-card slots and serial storage, GBA threads off.
5. Inject controller buttons through a verified deterministic input adapter;
   record commands, frame/counter observations and screenshots. Do not
   interpret a timeout or blue/green GBA screen as successful transfer.

## Required scenarios

| Scenario | Required terminal evidence |
| --- | --- |
| A once | Sessions 1/1, chunks 128/128, bytes 16384/16384, whole-image CRC E7638E98, GBA cyan, no data/startup errors, SI not owned |
| Y four sessions | Sessions 4/4 with four distinct session IDs, terminal 128/128, CRC E7638E98 and cyan |
| B after early chunks, then A | No old session counted; SI drained; new session completes with golden CRC |
| B around midpoint, then A | Same, with no stale-ACK or stale-frame acceptance |
| B near final chunk, then A | Same, including correct last-chunk checksum |
| Wrong companion | Refusal before transfer: chunks 0/128 and no image acceptance |
| GBA reboot/disconnect during transfer | Bounded terminal error, no unsafe automatic resubmission |

Capture the displayed `Pending polls`, `Retries`, last retry sequence,
retry cause and raw reply for each completed run. The expected deliberate
chunk-2 NACK normally produces retry cause 1; do not assert a fixed total
of pending queries across emulator schedules. Do not mark a scenario passed
from GBA color alone. Independently verify the GameCube counters.

## CI rollout criteria

- Keep emulator runs opt-in, not on every source push: emulator builds and
  dependencies may be expensive, and the BIOS/input preconditions are not met.
- First establish a **non-secret** preflight that checks Dolphin revision,
  integrated GBA support, artifact integrity and controller automation.
- Then run an isolated single-session smoke test using a legally provided
  local BIOS; after recorded screenshots/counters, automate Y and the three
  interruption windows.
- Publish only logs, checksums and screenshots with no proprietary inputs.
- Do not merge PR #2 on the strength of portable simulations alone if the
  intended acceptance criterion is physical or new emulator execution.

## Current disposition

The repository source and all existing known-good binaries remain untouched
by this plan. Four-session and interrupted transfer coverage have passed
portable CI simulation, but **new Dolphin repeated-session execution is
unverified**. Physical hardware validation is also pending.
