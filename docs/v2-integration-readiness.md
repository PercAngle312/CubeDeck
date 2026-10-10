# Transfer-ID framing and console build readiness

The experimental v2 transport now uses a reserved high opcode bit to
separate its on-wire packets from the older hardware-tested CDTP opcodes.
The ID-aware decoder strips that bit only after validating packet CRC and a
matching caller-selected 32-bit transfer ID. A legacy v1 packet with an
accidentally matching first four payload bytes is still rejected.

## Module boundaries

- `session_guard.c`: low-level v2 envelope, CRC-framed transfer ID and
  matched sequence/opcode acknowledgment.
- `bound_stream.c`: host-only adapter between the envelope and the
  manifest/DATA/FINISH streaming sender/receiver.
- `stream_sender.c`: adjustable DATA chunk limit (252 bytes for v2, 256
  bytes for existing callers), with bounded retransmission.
- `transfer_segments.c`: size-aware independent session planning. With a
  252-byte v2 DATA chunk, one session holds 16,514,820 bytes maximum; 32 MiB
  is partitioned into 16,514,820 + 16,514,820 + 524,792 bytes.
- `tests/test_bound_word_stream.c`: 65,539-byte, two-direction test in
  which the actual incremental word parser sees encoded v2 messages and
  returns framed replies. Faults include CRC corruption, a truncated frame,
  lost acknowledgments, and a previous-phase ACK.
- `tests/test_session_guard.c`: tests all payload sizes and rejects legacy
  CDTP packets even if the initial payload coincidentally matches the
  transfer ID.
- CI builds protocol objects independently for PowerPC/GameCube and
  ARM7TDMI/GBA. Cross-compilation only checks portability of the C sources;
  it does **not** mean these objects run on hardware yet.

## Work remaining before console integration

1. Introduce a separate, opt-in Joybus experiment rather than modifying the
   known-good diagnostic input path.
2. Specify handshake timeout, parser reset, retry scheduling and interrupt
   synchronization on both processors; ISR code must not do unbounded work.
3. Negotiate unique transfer IDs, and bind every segment to a manifest with
   absolute source offset and total image ID.
4. Verify read-only acquisition of a bounded cartridge header using a GBA
   multiboot payload without save/flash writes.
5. Only consider ROM persistence and VBA GX handoff after end-to-end hardware
   verification. Keep GCMCE disconnected for these experiments.

All stream tests still use RAM-only mock sources/sinks. No cartridge contents,
save operations or memory-card writes are implemented in the new modules.
