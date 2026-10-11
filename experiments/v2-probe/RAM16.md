# Opt-in 16 KiB synthetic RAM beta

**Not physical-hardware validated.** This is a transport beta, with no GBA
emulator, cartridge access, save operations or GameCube storage backend.
The proven diagnostics, ACK/multiframe targets and separate 1 KiB artifacts
remain preserved. The default 1 KiB module behavior is unchanged.

## Separate paired targets

- `make -f experiments/v2-probe/Makefile ram16-gamecube` → `cubedeck-v2-ram16.dol`
- `make -f experiments/v2-probe/Makefile ram16-gba` → `cubedeck-v2-ram16.gba`
- CI artifacts: `cubedeck-v2-ram16-gamecube` and `cubedeck-v2-ram16-gba`.

Download the pair from the same passing CI run. A runs one image; Y runs
four fresh sessions; B interrupts. The GBA companion must be on Port 2.
Readiness signature `43535236` and transfer prefix `B2CD0000` distinguish
this pair from the 1 KiB, three-packet recovery and startup pairs.

## Integrity and memory

This opt-in configuration reuses the tested bulk state machine with 128
ordered chunks of 128 bytes (16,384 bytes total). This fits the eight-bit
fixture sequence key; 256 chunks of 64 bytes would not. Each frame is 37
logical words transmitted as tagged low/high halves. Session-bound packet
ACK/NACK, two retries per chunk, deliberate single NACK on chunk 2 and
idempotent duplicate acceptance are retained.

The synthetic payload formula is unchanged from [BULK.md](BULK.md). Its
whole-image IEEE CRC-32 is **`E7638E98`**, independently calculated using
Python zlib. The final ACK requires frame/session/sequence/payload validation
and a CRC over the stored 16,256-byte prefix plus the last decoded chunk.
Accepted whole-image CRC remains zero until all 128 chunks are accepted.
Cyan and the GameCube success counters require final acceptance.

The GBA worker snapshot is static in `.ewram`, keeping 16 KiB off the
small IWRAM stack. Final-prefix copying remains a bounded IRQ-excluded
operation, now 16,256 bytes; whole-image CRC computation is outside IRQ
exclusion. The existing 12-frame verdict gap remains. This larger copy's
physical interrupt latency still needs console testing; this fixture is
not a general-purpose streaming transport. Snapshot publication checks
frame epoch/session/sequence, so RESET invalidates outstanding validation.

SI callbacks retain the tested single-owner adapter: B cannot cancel an
outstanding SI transfer, and a fresh session waits for callback draining.
Watchdog/data errors remain terminal and require relaunch. This beta does
not promise automatic recovery from a rebooted peer or electrical faults.

## Software checks

The common packet matrix runs against both sizes. The 16 KiB configuration
covers every stored-prefix byte, every final-frame bit and all 65,535 IDs,
plus bounded retries, missing work, stale publication and replay. The
adapter matrix retains exhaustive 1 KiB operation coverage. For 16 KiB it
samples startup operations and each frame's first send/read, midpoint,
last word and verdict send/read, including the retransmitted chunk.
This bounds CI work while covering early, middle and final sequence values.
Address/undefined-behavior sanitizers run in CI. Local runs disable leak
scanning due to restricted process task-directory access.

Host-test results, cross-compilation and emulator execution must be reported
separately. No physical validation is implied by any of those checks.

Local sanitizer results: 18,041 packet/image fault cases, all 65,535 IDs,
and two full adapter sessions with 3,632 sampled SI fault sites passed.
CI also checks that the linked GBA snapshot symbol is in EWRAM.
