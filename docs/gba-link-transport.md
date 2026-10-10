# CubeDeck GBA Link Transport — Implementation Notes

Status: **first step implemented in source: asynchronous GBA device-type detection on GameCube Port 2. No Joybus payload exchange or multiboot upload is implemented yet.**

## Confirmed hardware path

The relevant interface is the Nintendo GameCube-to-Game Boy Advance link cable (commonly DOL-011 or a compatible cable). It connects the GBA to the GameCube Serial Interface (SI) bus through a GameCube controller port. Open-source projects demonstrate that GameCube homebrew can upload a small GBA multiboot program and exchange Joybus commands with that running program.

Useful prior art:
- [FIX94/gba-link-cable-rom-sender](https://github.com/FIX94/gba-link-cable-rom-sender) — GameCube/Wii homebrew that sends small multiboot GBA ROMs through the cable. Its source demonstrates asynchronous SI transfer callbacks and the Joybus command framing used for multiboot.
- [FIX94/gba-link-cable-dumper](https://github.com/FIX94/gba-link-cable-dumper) — prior art for reading GBA ROMs and saves through the cable; this is not integrated into CubeDeck.
- [afska/gba-link-connection](https://github.com/afska/gba-link-connection) — GBA-side library with a Joybus/LinkCube implementation.
- [joypad-ai/joypad-os GBA link notes](https://github.com/joypad-ai/joypad-os/blob/main/docs/output/gba-link-cable.md) — further protocol and hardware notes.

These are reference projects, not code already integrated into CubeDeck. Check license terms and adapt only the minimum required code when implementing transfer.

## Implemented: device-presence check

The CubeDeck menu includes **GBA link cable check (Port 2)**. It calls `SI_GetTypeAsync(1, callback)` to query SI channel 1 (GameCube Port 2), updates the page from the callback, and retries approximately once per second while the page is open.

Safety boundaries:
- It does not issue Joybus RESET/STATUS/READ/WRITE commands.
- It does not upload or execute a GBA payload.
- It does not read or write cartridge ROM or save data.
- The callback is asynchronous; the menu loop does not busy-wait for a device response.

This change is source-committed but still needs a cloud compile and physical-cable validation. A no-device result is not proof that a cable or GBA is faulty until tested against a known-good cable and the supported hardware setup.

## Important constraints

- The GameCube↔GBA cable uses Joybus/SI, not the normal GBA multiplayer serial protocol. Do not confuse them.
- Multiboot payloads are small; a diagnostic client should be kept tiny.
- Joybus transfers are command-based and small. Bulk transfer needs chunking, pacing, sequence numbers, acknowledgements, checksums, timeouts, and cancellation.
- The SuperCard occupies the GBA cartridge slot. The GBA cannot read a retail cartridge while the SuperCard is inserted. A ROM on the SuperCard SD is not automatically visible to the GameCube.
- Do not attempt direct cartridge-save writing during link bring-up.

## Next implementation milestones

1. **Validate detection:** cloud compile, boot on GameCube, test Port 2 with cable/GBA connected and disconnected.
2. **Multiboot diagnostic:** add a minimal GBA-side payload built with an explicit GBA toolchain. Reuse known-good SI/multiboot logic only after confirming its license and required dependencies.
3. **Joybus proof:** exchange a fixed signature and counter with the running payload. Validate command byte order, transfer sizes, pacing, callback completion, timeout behavior, and disconnect recovery.
4. **Chunked transport:** bounded chunks with sequence numbers, checksums, acknowledgements, retries, and cancellation.
5. **Cartridge reader:** research a separate GBA-side program that reads the inserted retail cartridge ROM and streams it to the GameCube. This requires the retail cartridge to occupy the GBA slot, not the SuperCard.
6. **Save support:** start read-only, identify save type, preserve backups, and validate per cartridge before any write path.
7. **Emulation:** only after ROM transfer and integrity checks work. Real-time emulation on GameCube is a separate substantial engineering task.

## Build and validation policy

- Keep the known-working video initialization and framebuffer stride untouched.
- Keep GameCube and GBA components as separate build targets with explicit toolchains.
- A successful cloud compile is not proof of cable communication.
- Never describe a feature as supported until the corresponding build and test have actually succeeded.
