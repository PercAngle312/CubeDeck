# CubeDeck — cartridge acquisition integration plan

**Status:** engineering design, not a hardware-supported feature. Existing GameCube
and GBA diagnostic runtime files must remain unchanged until a separate, opt-in
hardware candidate has passed automated builds and protocol tests.

## What already exists

- Hardware: GameCube SI Port 2 reliably detects a real GBA, and a GBA-side
  diagnostic decoded one seven-byte CRC-framed packet with 6/6 Joybus words
  confirmed. This does **not** mean cartridges can be read yet.
- Host-only: full packet decoder, stop-and-wait retransmission, manifest,
  opcode-bound ACK, checksum, bounded streaming sender/receiver and mock sinks.
- Storage: an unexplained prior GCMCE corruption incident means writes should
  be disabled and the GCMCE disconnected for future experimental tests.

## Verified reference implementations to study

1. FIX94's `gba-link-cable-dumper`:
   https://github.com/FIX94/gba-link-cable-dumper
   A GameCube/Wii application and a GBA-side payload which transfers actual
   ROM, BIOS and save information over the link cable. Its README says to boot
   the GBA without a cartridge or hold START+SELECT at startup, then insert
   a cartridge when prompted; output goes to SD Gecko/other storage. CubeDeck
   should **not** immediately copy its output/write path. First inspect the
   GBA ROM-read routine and GC-side Joybus handshake as separate components.
2. FIX94's `gba-link-cable-rom-sender`:
   https://github.com/FIX94/gba-link-cable-rom-sender
   A useful reference for the GameCube-side GBA multiboot/bootstrap path.
   Its documented use is uploading a <=256 KiB multiboot payload to GBA RAM;
   that is not the same as executing a retail ROM on the GameCube.
3. Multiboot documentation:
   https://github.com/afska/gba-link-connection/blob/master/docs/multiboot.md
   Useful for RAM entrypoint and launch-handshake investigation. Do not
   assume the existing cartridge-based diagnostic is already a multiboot ROM.

Review source licenses before copying any code, and preserve applicable
attribution and license terms.

## Required steps before first cartridge-access test

1. **Read-only acquisition mode.** Define a GBA-side multiboot payload that
   never calls save Flash/EEPROM/SRAM write, erase or format APIs. The existing
   GBA diagnostic is launched from a flashcart; a retail cartridge cannot
   generally be inserted alongside the flashcart in the same cartridge slot.
   A read-only multiboot payload is a better retail-cartridge investigation.
2. **Handshake with precise timeouts.** Add GBA bootstrap support behind an
   opt-in mode in a separate test binary; keep known-good diagnostic launch
   code unchanged. Verify RESET, STATUS, transfer and execution phases.
3. **Bounded read request.** Ask for a cartridge header first (e.g. 192 bytes)
   and read a precisely bounded range with no persistence. Never infer ROM
   size from an untrusted header without independently validating bounds.
4. **Receive and verify.** Feed received 32-bit words into the host-tested
   streaming receiver. Count exact bytes, sequences, retries and CRC before
   accepting a transfer. Use a RAM-only sink and show a checksum on screen.
5. **Storage later.** Write a fresh dump to a dedicated opt-in destination
   only after ROM acquisition is verified. Do not use GCMCE during initial
   testing; back up storage and avoid all save writes.
6. **Emulation separately.** Obtaining ROM bytes is not equivalent to
   executing GBA CPU instructions on GameCube. A GameCube-targeted CPU/PPU/
   APU emulation core, controller mapping and persistent save design remain
   separate, substantial workstreams.

## Protocol caveats

- The current streaming session permits 65,535 DATA packets and 256 bytes
  per DATA packet (16,776,960 bytes per session). A 32 MiB game would require
  independently bounded, ordered multi-session transfers or a protocol
  revision; do not silently wrap the 16-bit sequence counter.
- ACKs must bind to sequence **and opcode**. For eventual multi-session
  use they must additionally bind to a session/transfer identifier so stale
  acknowledgments cannot cross transfer boundaries.
- CRC32 detects accidental transmission corruption, not a malicious sender.
- All-source preflight CRC assumes the underlying bytes stay unchanged between
  the checksum pass and the transfer itself.
- Physical Joybus timing, GBA interrupt reentrancy, and multiboot recovery
  are not validated by host C tests.

## Proposed next implementation checkpoint

Add a **separate** GameCube opt-in multiboot handshake module and GBA
read-only probe target, both exercised by GitHub Actions. Do not replace the
working `cubedeck.dol` diagnostic code path by default. Only request a
hardware test after cloud compilation and host handshake replay tests pass.

## Large-image segment planning (host only)

`protocol/transfer_segments.c` provides a constant-memory transfer planner
which divides a larger logical image into windows that fit the streaming
session's **65,535** data sequence numbers. A 32 MiB image needs **three**
sessions (16,776,960 + 16,776,960 + 512 bytes), and the host-only CI test
validates contiguous windows for 32 and 64 MiB scenarios. It does not
read or transmit any cartridge data.

This planner is intentionally **not wired to the runtime**. A real multi-
session protocol must add unique session IDs, absolute offsets, per-window
manifests, and a final whole-image checksum before accepting such a dump.

## Maximum-size single-window simulation

`tests/test_stream_max_window.c` uses a synthetic byte generator and an
in-memory verification callback (neither allocates a ROM-sized buffer) to
exercise a complete **65,535-DATA-packet**, **16,776,960-byte** transfer in
one manifest-first session. The test computes the preflight CRC32 using
256-byte reads, passes every frame through the host streaming receiver,
and requires a successful final checksum plus precisely bounded counters.
This confirms the C implementation can reach the last 16-bit DATA sequence
without wrapping, but does **not** demonstrate equivalent throughput or
reliability on an actual GBA link cable.
