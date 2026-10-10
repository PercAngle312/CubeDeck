# CubeDeck GBA Link Transport — Implementation Notes

Status: **GBA detection and a two-way Joybus word exchange are confirmed by the maintainer on physical hardware. The next hardware check is a 16-round sequential-word stress test.**

## Confirmed hardware path

The Nintendo GameCube-to-Game Boy Advance link cable (commonly DOL-011 or compatible) connects the GBA to the GameCube Serial Interface (SI) bus through a GameCube controller port.

Prior art:
- [FIX94/gba-link-cable-rom-sender](https://github.com/FIX94/gba-link-cable-rom-sender) — MIT-licensed GameCube/Wii multiboot sender; its SI calls and multiboot framing are useful references.
- [FIX94/gba-link-cable-dumper](https://github.com/FIX94/gba-link-cable-dumper) — separate ROM/save reader project; not integrated.
- [afska/gba-link-connection](https://github.com/afska/gba-link-connection) — GBA-side Joybus implementation in `lib/LinkCube.hpp`; it uses RCNT Joybus mode, JOYCNT flags, and high/low 16-bit transfer registers.
- [joypad-ai/joypad-os GBA link notes](https://github.com/joypad-ai/joypad-os/blob/main/docs/output/gba-link-cable.md).

## Confirmed milestones: detection and round-trip

CubeDeck calls `SI_GetTypeAsync(1, callback)` for GameCube Port 2. The maintainer confirmed that the GBA is detected and that the two-way diagnostic exchange works on physical hardware: the GBA screen visibly changes color after receiving a word, and the GameCube reads the expected reply `B76E6ABB`. This confirms one diagnostic word exchange, not sustained transfer or ROM access.

## Added: standalone GBA diagnostic source

`gba-diagnostic/main.c` is intended to compile as a normal GBA ROM for the existing flashcart. It:
- switches the GBA link port into Joybus mode;
- enables serial interrupts and reads/writes the 16-bit Joybus register halves explicitly;
- starts with a fixed `CDBK` signature response and prepares a deterministic XOR reply after receiving a word;
- changes screen color when receive count advances;
- never accesses cartridge ROM or save memory.

`gba-diagnostic/Makefile` is a separate devkitARM/libgba target. GitHub Actions now has a separate GBA build job and artifact. CI builds both targets. The maintainer has also verified the matching sender/receiver on a real cable. The new stress-test path is not yet hardware verified.

## Protocol constraints

- This is Joybus/SI, not ordinary GBA multiplayer serial mode.
- Joybus register words are split into high and low 16-bit halves. Preserve word order on both ends.
- Transfers must be paced; use callback completion, finite timeouts, retry/cancel handling, and disconnect-safe state. Never spin indefinitely waiting for SI completion.
- The SuperCard occupies the cartridge slot. A retail cartridge cannot be read while the SuperCard is inserted; a ROM on the SuperCard SD is not automatically exposed to the GameCube.
- No save writing during bring-up.

## Next milestones

1. Confirm both CI jobs compile and produce artifacts.
2. Run the new 16-round sequential-word stress test and confirm every reply on hardware.
3. Develop chunked transport with sequence numbers, checksums, acknowledgements, retries, and cancellation.
4. Investigate separate ROM acquisition paths for physical cartridges and SuperCard SD files; neither is currently exposed to the GameCube.
5. Research save identification and read-only access separately; do not write saves during bring-up.
6. Consider emulation only after ROM transfer and integrity checks are demonstrated.

## Validation policy

Keep the working GameCube video initialization untouched. Build GameCube and GBA components with separate toolchains. A successful compile is not proof of communication; only record a Joybus milestone after actual request/response evidence.
