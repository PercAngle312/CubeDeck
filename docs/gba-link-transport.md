# CubeDeck GBA Link Transport — Design Notes

Status: **research/design only; CubeDeck does not yet contain a working GameCube↔GBA transport implementation.**

## Confirmed hardware path

The relevant interface is the Nintendo GameCube-to-Game Boy Advance link cable (commonly DOL-011 or a compatible cable). It connects the GBA to the GameCube Serial Interface (SI) bus through a GameCube controller port. Open-source projects demonstrate that GameCube homebrew can use this connection to upload a small GBA multiboot program and then exchange Joybus commands with that running program.

Useful prior art:
- [FIX94/gba-link-cable-rom-sender](https://github.com/FIX94/gba-link-cable-rom-sender) — GameCube/Wii homebrew that sends small multiboot GBA ROMs through the cable.
- [afska/gba-link-connection](https://github.com/afska/gba-link-connection) — GBA-side library with a Joybus/LinkCube implementation for communicating with GameCube or Wii.
- [joypad-ai/joypad-os GBA link notes](https://github.com/joypad-ai/joypad-os/blob/main/docs/output/gba-link-cable.md) — documents the DOL-011 SI connection, multiboot upload, and Joybus command exchange.

These are reference projects, not code already integrated into CubeDeck. Before implementation, inspect their licensing and interfaces and adapt only what is needed.

## Important constraints

- The classic GameCube↔GBA cable is a Joybus/SI connection, not the normal GBA multiplayer serial protocol. The two protocols must not be confused.
- A GameCube can upload a multiboot payload to the GBA's RAM, but the multiboot payload is small (roughly 16 KiB for the documented path). That is enough for a diagnostic client, not a full GBA emulator.
- Once a GBA-side payload is running, Joybus exchanges use small command/response transfers (4-byte reads/writes in the documented implementation). Bulk data transfer will need chunking, pacing, acknowledgements, and timeout recovery.
- The SuperCard occupies the GBA cartridge slot. The GBA cannot read a retail cartridge while the SuperCard is inserted. A ROM on the SuperCard SD is not automatically visible to the GameCube over the cable.
- No direct cartridge-save writing should be attempted in the link-probe milestone.

## First implementation milestone

Start with a harmless diagnostic payload. The GameCube should:
1. Detect/initialize the link interface using a known-good Joybus/SI implementation.
2. Upload or otherwise start the tiny GBA-side diagnostic client.
3. Exchange a fixed test value and a monotonically increasing counter.
4. Verify returned values, count failures, and show connection/timeout status in CubeDeck.
5. Recover cleanly from a disconnected cable and never block the menu indefinitely.

The GBA diagnostic client should:
- enter Joybus mode correctly;
- answer a simple status/read/write exchange;
- return a signature and counter that the GameCube can verify;
- avoid touching cartridge save memory.

## Later milestones

1. **Link proof:** repeatable Joybus command/response on real hardware or a trusted emulator.
2. **Chunked transport:** bounded payload chunks with sequence numbers, checksums, acknowledgements, retries, and cancellation.
3. **Cartridge reader research:** a separate GBA-side program that reads the inserted cartridge ROM safely and streams it to the GameCube. This requires the retail cartridge to be in the GBA slot; it cannot be done while the SuperCard is occupying that slot.
4. **Save support:** begin read-only and identify save type before any write path; preserve backups and never claim generic save writing without per-type validation.
5. **Emulation:** only after ROM transfer and integrity checks are proven. Real-time emulation on GameCube is a separate large engineering task.

## Build and validation policy

- Keep the known-working CubeDeck video initialization untouched unless a specific defect requires changing it.
- Build GameCube and GBA components as separate targets with explicit toolchains.
- A successful cloud compile is not proof of cable communication; validate the link protocol independently.
- Do not describe a feature as supported until it has been built and tested.
