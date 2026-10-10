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
