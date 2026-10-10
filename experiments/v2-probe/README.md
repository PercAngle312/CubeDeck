# Optional Joybus v2 probe (experimental)

This is the first **separate console-target** test for the 32-bit transfer-ID
framing. It is intentionally **not** built by the default root Makefile and
does not change either previously hardware-tested diagnostic.

## Build artifacts

In GitHub Actions, the independent jobs publish:

- `cubedeck-v2-probe-gamecube`: `cubedeck-v2-probe.dol`
- `cubedeck-v2-probe-gba`: `cubedeck-v2-probe.gba`

They are built with `make -f experiments/v2-probe/Makefile gamecube` or
`make -f experiments/v2-probe/Makefile gba` at repository root.

## Test scope

The optional GameCube DOL retains the known callback-driven RESET, STATUS,
SEND and READ sequence, but wraps a deterministic seven-byte sample in the
experimental v2 CDTP packet with transfer ID `0x42CD2026`. Each word
still uses the familiar XOR reply for the physical handshake. The optional
GBA ROM uses the same Joybus IRQ and XOR response, then verifies the complete
packet's transport CRC and transfer ID and checks the seven sample bytes.
**Cyan on GBA means the expected v2 packet was accepted.** Red indicates
an invalid packet, and other colors are interim word-reception indicators.

The GC's `V2 WORD ECHO PASS` only means all words received valid XOR
responses. **It does not by itself prove the GBA accepted the entire packet.**
Check the GBA's cyan result separately.

## Safety and current limitations

- No cartridge reads, flash programming, saves, memory card, or filesystem
  writes are implemented in either probe.
- The test ID is deliberately fixed for a smoke test; it is not a fresh
  negotiated transfer ID, so these tools are **not** ready for repeated
  bulk sessions or actual data acquisition.
- It sends only one static sample frame, not the multi-packet sender/receiver
  and not an actual game or ROM.
- This is a hardware experiment, **not yet hardware verified**. Use the known
  diagnostic binaries as a fallback and disconnect GCMCE for tests.
- Unlike the protocol's per-frame negative/positive acknowledgments, the
  word-level XOR response is not a streaming ACK. Bridging the two mechanisms
  safely remains future work.

The normal `cubedeck.dol` and `cubedeck-gba-diagnostic.gba` are unchanged.

## GBA status colors (diagnostic build)

The updated experimental GBA build uses **gray shades** while raw words
arrive. Only after a complete candidate packet is captured does it show
a terminal result:

- **Cyan:** valid v2 frame, matching transfer ID, matching seven-byte sample
- **Magenta:** packet CRC mismatch
- **Orange:** malformed packet layout, length or version
- **Blue:** CRC is valid but v2 opcode marker or transfer ID is wrong
- **Red:** CRC and session are valid but opcode, sequence or sample differs

If the GBA remains **gray**, it never captured a complete frame. If the
GameCube shows 7/7 XOR replies but the GBA is gray, the physical transport
may have missed a packet-start marker or failed to reach main-loop decode.
This diagnostic is not a test of actual game-cartridge transfer.

**Important:** The older `cubedeck-gba-diagnostic.gba` does not support
the v2 marker or extra four-byte transfer ID. With the experimental
GameCube DOL it can acknowledge all seven words and then reject the sample.
Use the artifact explicitly named `cubedeck-v2-probe-gba`, extracted from
the **same workflow run** as the experimental GameCube DOL. On flashcarts,
verify the launched filename corresponds to that experimental ROM rather
than a previously cached diagnostic.

## Hardware validation status

**2026-10-10 — User-reported PASS on physical GameCube + GBA.** The user
reported the GameCube's seven-of-seven 32-bit words acknowledged, and then
**cyan** on the GBA after running the updated color-coded experimental v2
probe. This is evidence of a successful v2 packet decode, CRC check,
transfer-ID match and seven-byte sample verification on their real link.

Important limitations: this is a user-reported manual test, not independently
instrumented bus telemetry; it proves one fixed-ID, single-frame sample,
not reliable multi-frame transmission, cartridge ROM access, save access,
run-time emulator linkage, or repeated-session negotiation. Do not modify
or replace the known-good diagnostic; treat later bulk-link tests as separate
opt-in experiments and keep storage disconnected while investigating safety.
