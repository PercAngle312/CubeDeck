# Experimental three-frame Joybus test

This experiment is **separate** from both the original GameCube/GBA
diagnostics and the user-confirmed cyan seven-word v2 sample probe.

## What it does

The GameCube sends three *different* seven-byte sample frames with the
same fixed test transfer ID (`0x42CD2026`) and increasing packet sequences
1, 2 and 3. Each sample is XOR-modified by `17 * (sequence - 1)` and has
its own CDTP CRC-32.

The GameCube still sends individual words via the demonstrated Joybus
SEND/READ flow and verifies a per-word XOR echo. Between frames it waits
12 video frames to allow the GBA main loop to validate and clear its
interrupt-fed frame buffer. The GBA checks CRC, v2 session ID, sequential
sequence and every sample byte before counting a frame.

### On-screen results

- **GameCube:** `V2 3-FRAME WORD ECHO: 3/3 frames sent` only means all
  outgoing words received the expected XOR echo, *not* that GBA verified
  every frame
- **GBA cyan:** all three complete frames passed every check, in order
- **GBA green:** one or two good frames; the test has not completed
- **GBA magenta:** failed CRC
- **GBA orange:** malformed packet or framing
- **GBA blue:** transfer ID or v2 opcode marker is wrong
- **GBA red:** unexpected sequence, opcode, length or payload
- **GBA gray:** collecting words or no complete frame decoded

## Separate GitHub Actions files

Download the two artifacts from the **same successful run**:

- `cubedeck-v2-multiframe-gamecube` -> `cubedeck-v2-multiframe.dol`
- `cubedeck-v2-multiframe-gba` -> `cubedeck-v2-multiframe.gba`

Use Swiss to launch the isolated DOL and an SD flashcart to launch the
isolated GBA ROM. GBA link cable goes to GameCube Port 2.
Disconnect GCMCE for this experimental test. Select the GameCube link
check and press **A**. Prefer keeping the older validated DOL/ROM copies
and avoid overwriting them.

## Boundaries

This is a synthetic fixed-ID demonstration, **not** an acknowledged
streaming transport. In particular, the GameCube never receives an
end-of-frame CRC accept/reject verdict from the GBA; it receives only
per-word XOR echoes. There is no automatic retransmission, dynamic
transfer-ID negotiation, cart ROM read, emulator execution, memory-card
operation or save write.

The code must be independently validated on physical hardware before
any claims of multi-frame Joybus success.

## Physical hardware result — 2026-10-10

**User-reported PASS:** On the real GameCube + GBA link cable, the user
observed **3/3** on the GameCube and **cyan** on the GBA using the isolated
three-frame experimental binaries. This is evidence that all three sequential
seven-byte v2 test frames reached the GBA, passed per-frame CRC, matched the
fixed test transfer ID, were accepted in sequence (1, 2, 3), and had the
expected distinct synthetic sample payloads. The GameCube's 3/3 indicator
itself only means per-word XOR echoes were received; the GBA cyan color
provides the additional frame-acceptance result.

This is a user-observed smoke test rather than independent bus capture. It does
**not** establish reliable retransmission on real hardware, high-volume ROM
streaming, dynamic session negotiation, ROM reading, save writes, or emulator
execution. The original diagnostics and cyan-verified single-frame probe
remain unchanged and should be retained as fallback artifacts.
