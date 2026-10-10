# Session-bound streaming adapter

This host-only prototype joins the standalone transfer-ID wire envelope to the
RAM-light streaming sender and receiver.

Files:

- `include/cubedeck/bound_stream.h` and `protocol/bound_stream.c`
  encode requests, reject cross-session replay, generate session-bound
  replies, and validate replies before advancing the sender.
- `cdtx_init_chunked` adds a selectable DATA payload size without changing
  the legacy `cdtx_init` behavior; v2 uses 252 bytes so the wire envelope
  has room for a 4-byte transfer identifier.
- `tests/test_bound_stream.c` simulates two consecutive transfers and
  verifies that DATA packets and ACKs from the previous session do not mutate
  or complete the next one. It also tests CRC rejection and last-chunk length.

Limitations:

- The v2-safe maximum image window is 65,535 x 252 = **16,514,820 bytes**.
  The previous 16,776,960-byte bound remains valid only for v1's 256-byte
  payload size. Use distinct sizes in any future segment planner.
- IDs are chosen by the caller and must be unique while old packets might
  remain. This protocol is not a cryptographic authentication mechanism.
- A malformed or wrong-session frame is **silently ignored**, with no ACK or
  NACK. The sender must use its bounded retry/timeout policy.
- The current receiver still verifies complete image length and CRC32 before
  reporting verified, but an external sink must make each callback atomic and
  is not rolled back on a final CRC mismatch. No persistent storage sink is
  included or allowed in the hardware diagnostic.
- Full data streaming and session negotiation are **not** yet integrated with
  real GameCube SI or GBA Joybus hardware. The original six-word hardware
  diagnostic remains unchanged and should not be replaced by this prototype.

Next hardware integration should use a separate opt-in binary that first
proves bounded messages, transport timeouts and reentrancy, keeping GCMCE
disconnected and existing saves backed up.
