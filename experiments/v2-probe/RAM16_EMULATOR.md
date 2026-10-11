# 16 KiB RAM beta: Dolphin execution record

**Emulator execution only; physical hardware validation remains pending.**

## Exact tested pair

Unmodified artifacts from [Actions run 38102277785](https://github.com/PercAngle312/CubeDeck/actions/runs/38102277785),
source commit `ce91b96694e583f8780df1fc4a60f71b77661785` (52/52 CI jobs passed):

| Artifact | File | SHA-256 |
| --- | --- | --- |
| cubedeck-v2-ram16-gamecube | cubedeck-v2-ram16.dol | `69a779757e1cbf65350c60015e346efed9665c60193bae118b0e9df1d3ecc8a3` |
| cubedeck-v2-ram16-gba | cubedeck-v2-ram16.gba | `23f9a6e44d31e7147837cc19ed51e29ebb99e548f4c02070406f67276dcbd748` |

The [qualified Dolphin 2407 / integrated mGBA build](EMULATOR.md#emulator-environment-and-qualifications)
was reused with a fresh isolated profile and the user's locally loaded BIOS.
Memory cards and Serial Port 1 were disabled. No BIOS, real save container,
cartridge or GameCube storage image is included in this repository.

## Observed result

A started one fresh image. The inspected completion screenshot showed:

- Sessions 1/1; session ID 1; Ready reads 1/8.
- Chunks **128/128**; bytes **16384/16384**.
- Accepted whole-image CRC **`E7638E98`**; GBA **cyan**.
- Startup error 0; data error 0; SI result 0; Owned SI 0.
- **Retries 2**, including the deliberately rejected second chunk.

[Complete image evidence](evidence/dolphin-ram16-complete.png).

There was one additional retry beyond the intentional NACK. Earlier progress
screenshots showed one retry; the final screenshot showed two. The current
counters do not identify the extra retry's chunk or cause. Do not attribute
it to CRC latency, IRQ copying, pending verdict timing or hardware without
more diagnostic evidence. Completion demonstrates bounded retry acceptance,
not an exactly-one-retry result or an explanation of the original physical
startup issue. The instantaneous-worker host fixtures completed with one retry.

The completed run used Dolphin `EmulationSpeed = 0.0` (speed limit disabled),
software rendering, NoAudio, single CPU thread and GBA threads disabled.
The final screenshot was captured on the eighth 30-second poll after A.
This is a functional test, not a throughput or hardware timing measurement.
An earlier normal-speed run was stopped while progressing without errors;
a second attempted launch sharing its input FIFO was discarded without a
completion claim. The completed run used a separate fresh profile and FIFO.
No savestates or modifications to CubeDeck binaries were used.

The 16 KiB Y loop, interruption/reboot scenarios, wrong companion, missing
callbacks and electrical faults were not exercised in this emulator record.
Their evidence from [the 1 KiB pair](BULK_EMULATOR.md) does not validate the
16 KiB pair. [RAM16.md](RAM16.md) describes its separate host fault matrix,
EWRAM placement check and IRQ-copy limitation. Physical console testing is
still required when the user has hardware available.
