# CubeDeck GBA Link Transport — Implementation Notes

Status: **Physical-hardware verified: GBA detection, 16/16 repeated Joybus replies, and a complete six-word sample packet validated on the GBA (cyan screen, CRC32 and seven payload bytes).** This does not establish bulk ROM transfer.

## Confirmed hardware path

The Nintendo GameCube-to-Game Boy Advance link cable (commonly DOL-011 or compatible) connects the GBA to the GameCube Serial Interface (SI) bus through a GameCube controller port. CubeDeck currently uses Port 2 / SI channel 1.

Prior art:
- [FIX94/gba-link-cable-rom-sender](https://github.com/FIX94/gba-link-cable-rom-sender) — MIT-licensed GameCube/Wii multiboot sender. It reads GBA ROM files from the GameCube-side SD filesystem and sends them to a GBA; this is the opposite direction from dumping a cartridge to the GameCube.
- [FIX94/gba-link-cable-dumper](https://github.com/FIX94/gba-link-cable-dumper) — GameCube/Wii homebrew that can dump GBA BIOS, cartridge ROMs, and saves over the link cable. Its README describes booting the GBA with no cartridge, then inserting the cartridge when prompted; it also documents a Start+Select boot workaround for some setups. Its stated 32 MiB ROM dump estimate is about 48 minutes, illustrating the cable protocol's throughput limits.
- [afska/gba-link-connection](https://github.com/afska/gba-link-connection) — GBA-side Joybus implementation in `lib/LinkCube.hpp`; useful reference for RCNT Joybus mode, JOYCNT flags, and high/low 16-bit transfer registers.
- [joypad-ai/joypad-os GBA link notes](https://github.com/joypad-ai/joypad-os/blob/main/docs/output/gba-link-cable.md).

## Hardware-confirmed milestones

1. `SI_GetTypeAsync(1, callback)` detects the GBA on GameCube Port 2.
2. A diagnostic word exchange changed the GBA screen and returned the expected GameCube reply `B76E6ABB`.
3. The maintainer ran the 16-round sequential-word test on physical hardware; all **16/16 replies passed**.\n4. The maintainer reported **6/6** acknowledgments, final CRC word `5DD2BACF`, XOR reply `F888860C`, and **cyan GBA screen** using the new packet parser, establishing end-to-end validation of the seven-byte sample packet.

The stress test validates a short sequence of independent words. It does not yet validate large payloads, sustained transfer, cartridge reads, ROM loading, save access, or TV playback.

## Current diagnostic implementation

`gba-diagnostic/main.c` is a standalone GBA ROM intended for a flashcart. It switches the GBA link port into Joybus mode, prepares a deterministic XOR response, and visibly changes screen color after each received word. It does not read or write cartridge ROM/save memory.

The GameCube sender uses callback-driven `SI_Transfer` calls for RESET, STATUS, send-word, and read-reply. It uses bounded frame timeouts and does not busy-wait on SI. The 16-round test is started with Y; X runs one round; A sends a host-encoded sample packet word-by-word. The A test verifies each XOR acknowledgment, while the current GBA diagnostic also decodes the full sample packet, checks CRC32 and its seven payload bytes, and displays cyan on success.

## Transport constraints and design rules

- This is Joybus/SI, not ordinary GBA multiplayer serial mode.
- Joybus words are split into high and low 16-bit halves. Preserve word order on both ends.
- Keep each SI operation callback-driven and bounded. A timeout must not start a second transfer while the previous SI operation may still be active, because libogc does not expose a cancellation API here.
- A passing single-word or short stress test is not proof of reliable bulk transport. Bulk transfer needs explicit framing, monotonically increasing sequence numbers, integrity checks, acknowledgements, retry limits, cancellation, and a final whole-image verification.
- Do not write save data during bring-up.
- The SuperCard's SD filesystem is not automatically exposed to the GameCube over the link cable. A SuperCard backup file cannot be assumed readable by generic GBA cartridge-bus reads.

## Next milestones

1. Keep the verified diagnostic and its current artifacts as the baseline.
2. Portable packet framing v1 exists in `include/cubedeck/transport_protocol.h` and `protocol/transport_protocol.c`. Host CI covers empty, partial-word, maximum-size, CRC-vector, corruption, malformed-shape, unsupported-version, and capacity cases. The GameCube A-button smoke test now sends a sample packet word-by-word; hardware verification of that new path is pending.
3. Add packet parsing to the GBA diagnostic and return an independently verifiable acknowledgement/checksum. Then verify payload bytes end-to-end on hardware before increasing payload size.
4. For cartridge ROM acquisition, evaluate the established [GBA Link Cable Dumper](https://github.com/FIX94/gba-link-cable-dumper) workflow before reimplementing cartridge access. Confirm whether its boot/insert-cartridge flow works with the user's GBA and carts.
5. Investigate SuperCard SD-file access separately; do not conflate it with reading a retail cartridge.
6. Only after verified ROM acquisition, choose a GameCube-compatible GBA emulator/frontend and storage path for TV playback.
7. Research save identification and read-only access separately; preserve existing saves and avoid writes during initial validation.

## Validation policy

Keep the known-working GameCube video initialization and framebuffer stride untouched. Minimize hardware testing; group changes into meaningful hardware milestones. Keep GCMCE disconnected while the earlier unexplained memory-card incident remains unresolved. Build GameCube and GBA components with separate toolchains. CI success proves compilation/artifact creation only. Record hardware milestones only after real request/response or data-integrity evidence.
