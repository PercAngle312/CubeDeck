# CubeDeck GBA Link Transport — Implementation Notes

Status: **GameCube Port 2 GBA detection is confirmed by the maintainer on physical hardware. A separate GBA-side diagnostic source and cloud build target have been added; compilation and Joybus exchange remain unverified.**

## Confirmed hardware path

The Nintendo GameCube-to-Game Boy Advance link cable (commonly DOL-011 or compatible) connects the GBA to the GameCube Serial Interface (SI) bus through a GameCube controller port.

Prior art:
- [FIX94/gba-link-cable-rom-sender](https://github.com/FIX94/gba-link-cable-rom-sender) — MIT-licensed GameCube/Wii multiboot sender; its SI calls and multiboot framing are useful references.
- [FIX94/gba-link-cable-dumper](https://github.com/FIX94/gba-link-cable-dumper) — separate ROM/save reader project; not integrated.
- [afska/gba-link-connection](https://github.com/afska/gba-link-connection) — GBA-side Joybus implementation in `lib/LinkCube.hpp`; it uses RCNT Joybus mode, JOYCNT flags, and high/low 16-bit transfer registers.
- [joypad-ai/joypad-os GBA link notes](https://github.com/joypad-ai/joypad-os/blob/main/docs/output/gba-link-cable.md).

## Confirmed milestone: detection

CubeDeck calls `SI_GetTypeAsync(1, callback)` for GameCube Port 2. The maintainer reports that the app still boots and the GBA is detected on their hardware. This confirms device detection on that setup, not Joybus payload transfer.

## Added: standalone GBA diagnostic source

`gba-diagnostic/main.c` is intended to compile as a normal GBA ROM for the existing flashcart. It:
- switches the GBA link port into Joybus mode;
- enables serial interrupts and reads/writes the 16-bit Joybus register halves explicitly;
- starts with a fixed `CDBK` signature response and prepares a deterministic XOR reply after receiving a word;
- changes screen color when receive count advances;
- never accesses cartridge ROM or save memory.

`gba-diagnostic/Makefile` is a separate devkitARM/libgba target. GitHub Actions now has a separate GBA build job and artifact. This is **source-level progress only** until CI succeeds. The GameCube side still sends no Joybus requests, so no screen change is expected yet.

## Protocol constraints

- This is Joybus/SI, not ordinary GBA multiplayer serial mode.
- Joybus register words are split into high and low 16-bit halves. Preserve word order on both ends.
- Transfers must be paced; use callback completion, finite timeouts, retry/cancel handling, and disconnect-safe state. Never spin indefinitely waiting for SI completion.
- The SuperCard occupies the cartridge slot. A retail cartridge cannot be read while the SuperCard is inserted; a ROM on the SuperCard SD is not automatically exposed to the GameCube.
- No save writing during bring-up.

## Next milestones

1. Confirm both CI jobs compile and produce artifacts.
2. Add a GameCube-side, callback-driven Joybus diagnostic sender for fixed test words, with bounded timeouts and visible sent/received/error counters.
3. Run the GBA diagnostic from the flashcart and verify a known signature/XOR response over the real cable.
4. Only after repeatable round-trips, develop chunked transport with sequence numbers, checksums, acknowledgements, retries, and cancellation.
5. Research cartridge ROM reading and save identification separately; keep initial save access read-only.
6. Consider emulation only after ROM transfer and integrity checks are demonstrated.

## Validation policy

Keep the working GameCube video initialization untouched. Build GameCube and GBA components with separate toolchains. A successful compile is not proof of communication; only record a Joybus milestone after actual request/response evidence.
