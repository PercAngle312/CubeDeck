# CubeDeck v10 — RAM-resident link protocol (development checkpoint)

## Verified foundation

On physical GameCube + linked GBA, the v9 `fdae52e` SI-polling-isolation
build reached RAM PROGRAM PASS, returned `0x39524443` (`CDR9`), and the
GBA LCD was cyan. This is **one successful hardware session**, not proof
of repeated cold-start reliability. Preserve the v9 build and its fixtures.

The v9 GBA binary is RAM-only and has no cartridge commands. The GameCube
currently polls a constant runtime response; it does not yet issue a
bidirectional application command. Do not describe v9 as a ROM reader.

## v10 milestones (separate tests, no changes to proven v9)

1. **Cold-start reliability:** rerun the *unchanged* known-good v9 DOL
   three times, restarting both systems between runs. Record each result.
   A failure here must be diagnosed before introducing a second protocol.
2. **RAM-only request/response:** develop a separate v10 GBA payload
   and GameCube DOL; define distinct ping/ack and counter/echo responses.
   Keep a bounded one-command-at-a-time state machine and verify SI
   ownership before reusing any transfer buffer.
3. **Read-only cart ID later:** only after v10 RAM protocol passes on hardware,
   introduce carefully isolated GBA-side cartridge header reads.
   No save writes, cartridge writes, or bulk ROM transfers in the initial v10 test.

## Safety/implementation constraints

- GBA plugged into GameCube controller port 2, powered on without cart for
  RAM-only tests. Keep GameCube controller scanning suspended after starting
  the GBA handshake: the isolated v9 experiment passed while the earlier
  polling build returned an SI callback result `0x08` during RESET.
  That suggests interference but does **not** prove causation.
- Continue to load only a user's own local 16 KiB BIOS, validate its
  expected CRC, and patch its logo into the in-memory GBA image.
  Never commit BIOS data, copyrighted logos, ROMs, or game images.
- Keep SD2SP2 accesses read-only. Never silently retry a timed-out
  SI operation without knowing who owns the response buffer; require
  a clean power cycle after terminal errors.
- Use a new v10 artifact name and a clear on-screen build ID. Preserve
  the exact v9 DOL for regression comparison.
- A BIOS final reply is not program-execution proof: demand a
  v10-specific application response **and** visual GBA feedback.
- CI validates compilation and portable protocol tests, but only a
  physical session can validate real GBA/GC Joybus reliability.

## Suggested minimal wire-protocol sketch (not yet implemented)

Reserve unique 32-bit reply values for READY, PING_ACK and ERROR,
all distinct from `CDR9`; include a small monotonic sequence counter
in later request/reply exchanges. Test exact byte order and packet
lengths with a host-side fixture before interpreting a hardware reply.

Next engineering task: build the independent v10 RAM payload and
GameCube entrypoint, with portable encoder/decoder tests; do not patch
the hardware-verified v9 source in place.

## v11 cartridge-header gate (planned, not hardware validated)

Before integrating `gba_rom_reader.c` into a RAM-resident GBA payload, require
three cold-start v10 PING/ACK hardware passes with the cartridge **absent**.
Record the GameCube PING stage, SI error, and purple GBA screen for each.
A CI-green v10 run alone is not proof of Joybus reliability.

Build v11 as separately named DOL/GBA artifacts, preserving v9 and v10.
The first cartridge experiment must read only the 192-byte ROM header through
aligned halfword loads, validate the header checksum, and return bounded
metadata over a sequence-checked request/reply protocol. Do not expose bulk
ROM reads, infer cartridge capacity, or touch save address space. Require a
physical cartridge and explicit user action; refuse empty/invalid headers.

Before any hardware run, add host tests for invalid checksum, interrupted
reads, repeated/stale responses, byte order, and out-of-range offsets, plus
ARM and PowerPC cross-compilation. A GBA-side ROM read callback already exists
but is not yet wired to the v10 RAM runtime. Never report this as a completed
cartridge read until a real GBA cartridge is successfully identified.
