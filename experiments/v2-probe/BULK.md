# Opt-in 1 KiB synthetic RAM transfer

**Not hardware validated.** This advances the transport experiment from
three sample packets to a stored **1,024-byte RAM image**. It implements no
cartridge reads/writes, save access, GameCube storage backend, or execution
of transferred data. Original diagnostics, framed probes, multiframe and
ACK/NACK source files remain unchanged; the three-packet recovery module
and its artifact names remain separate.

## Separate pair

- `cubedeck-v2-bulk-gamecube` → `cubedeck-v2-bulk.dol`
- `cubedeck-v2-bulk-gba` → `cubedeck-v2-bulk.gba`

Build only by explicitly selecting `bulk-gamecube` or `bulk-gba` with
`make -f experiments/v2-probe/Makefile`. Download both from the same passing
CI run. This companion has a distinct startup signature and transfer-ID
prefix, so it cannot silently pair with the startup or recovery GBA.

A runs one image; Y runs four fresh sessions; B interrupts. Every fresh
session starts with RESET, STATUS and bounded application readiness reads.
The controller and single-owner SI adapter retain their fail-closed timeout
behavior. SI errors and rejected submissions stop the operation. B does not
cancel a pending hardware transfer; buffers remain owned until a callback
is consumed. A may begin again only after a timely interruption drain;
data errors/watchdogs require relaunch.

## Integrity and acceptance

The image comprises 16 ordered 64-byte chunks. Each CDGU frame carries the
session ID, opcode 1, sequence 1..16, payload and CRC. Each logical frame is
21 words; tagged low/high halves prevent arbitrary CRC/data bits from
colliding with session/frame/query controls. This remains a synthetic
fixture, not the final streaming wire API.

Payload byte at offset `n` is:

```
((n * 73 + (n >> 3) * 19 + 0x5A) ^ (n >> 1)) & 0xFF
```

The whole 1 KiB image's IEEE CRC-32 is **`6E99AE3A`**, independently derived
with Python's zlib. The GBA checks the exact expected payload and frame CRC
for every chunk before acceptance. Chunk 2 deliberately receives one NACK;
the GameCube retries it. Retry budget is two per chunk. Duplicate accepted
chunks receive ACK without changing RAM or advancing the sequence twice.

Before acknowledging chunk 16, the worker snapshots the already-stored
960-byte RAM prefix under brief IRQ exclusion. It validates frame/session/
sequence/payload, then computes the CRC over that snapshot plus the final
64 bytes **outside** IRQ exclusion. Final acceptance requires the golden
whole-image CRC. Publication checks the same frame epoch, session and
sequence; RESET/frame restart invalidates stale worker results. The last
chunk is copied into RAM only on successful publication. Cyan requires all
16 chunks accepted; the GameCube displays 16/16, 1024/1024, retries and the
accepted whole-image CRC only after the final ACK.

RESET logically invalidates old image bytes and clears the accepted CRC.
It need not clear the RAM array in the serial IRQ: all 15 prefix chunks
must be newly accepted in the current session before the final CRC snapshot
can use them. Prefix corruption causes bounded final NACKs and failure;
this fixture does not silently repair an earlier accepted RAM chunk.
There are no dynamically allocated buffers or unbounded retry loops.

## Portable validation

- `tests/test_bulk_packets.c`: 1,977 corruption/replay/interruption cases,
  all 65,535 usable IDs, every frame bit, all wire-half fault positions,
  bounded missing-worker retries, stale worker publication, and corruption
  of each of the 960 previously stored prefix bytes. Valid final duplicates
  cannot rewrite the image or erase its CRC.
- `tests/test_bulk_adapter.c`: two full images through the real experimental
  adapter plus four injected faults at each of 1,501 SI operation positions
  (6,004 sites). Interruption, watchdog, callback error and rejected
  submission verify bounded failure, pending-buffer preservation and fresh
  RESET/session recovery.
- Address/undefined-behavior sanitizers are enabled in CI. Local checks used
  those sanitizers with leak scanning disabled because this runner cannot
  inspect process task directories; CI keeps the normal sanitizer settings.
- Existing recovery packet/adapter tests still pass; runtime write audit
  includes the new fixture. Cross-compilation and physical execution are
  separate results, reported only after their respective checks finish.

The preceding three-packet recovery and startup pairs have
[actual Dolphin execution evidence](EMULATOR.md). That evidence does not
by itself validate this new 1 KiB pair. Physical testing remains pending
until the user has hardware available again.

The exact 1 KiB CI pair now has [Dolphin execution evidence](BULK_EMULATOR.md)
for one image, four fresh sessions, startup/packet/partial-image interruption
and an emulated GBA reboot. This does not change its physical validation status.
