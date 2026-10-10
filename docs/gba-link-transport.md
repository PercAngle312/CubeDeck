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

## Stop-and-wait transfer simulation (host only)

`include/cubedeck/reliable_link.h` and `protocol/reliable_link.c` add a bounded stop-and-wait sender and CRC-guarded receiver for packetized RAM data (up to 4096 bytes per session). Each DATA packet uses the existing CRC32 wire framing, is matched to a 16-bit sequence, and requires an empty-payload ACK with that sequence. A NACK or caller-declared timeout schedules a retransmission; after three retries, the sender fails closed. The receiver treats an exact retry of the last accepted packet as an idempotent duplicate instead of appending data twice.

`tests/test_reliable_link.c` simulates a 4096-byte transfer, a corrupted packet (NACK then retransmission), a lost ACK (duplicate retry without duplicate append), mismatched ACK sequence, and retry exhaustion. **This is deliberately host-only: it does not make GameCube or GBA stream chunks yet, nor does it access any memory card, cartridge or storage device.** CI compiles the simulator independently from both console binaries.

Future work: design GBA Joybus response scheduling for an actual frame-level ACK (distinct from the existing XOR word acknowledgment), connect one bounded chunk session to the console runtimes, and test only after substantial CI coverage.

## Whole-transfer integrity (host only)

The packet CRC32 protects individual frames but cannot prove that the receiver assembled the entire intended image. `include/cubedeck/image_integrity.h` and `protocol/image_integrity.c` add a RAM-only manifest carrying expected byte length and independent whole-image IEEE CRC32. The verifier returns incomplete for a short image, success only when both length and checksum match, and failure for overlong/corrupt images. The new CI simulation transfers 4096 bytes through the sender/receiver, checks the completed image against its manifest, then alters one byte to confirm rejection. The standard 123456789 CRC32 test vector is included.

**Not yet wired to the GBA runtime**: the manifest must be transferred and authenticated in a later protocol milestone before accepting real content. The module does no memory-card, cartridge, or file I/O. No hardware retest is necessary for this host-only change.

## Manifest-first receive guard (host-only)

`include/cubedeck/verified_session.h` and `protocol/verified_session.c` provide a small transfer session built on the existing RAM chunk receiver. The sender must first declare a **MANIFEST** packet (opcode `0x30`, sequence 0, eight little-endian bytes: expected total length and whole-image CRC32). The receiver refuses DATA without a manifest, refuses overflow beyond the declared length or 4096-byte buffer, and treats an identical manifest retry as idempotent. DATA frames use the existing `0x20` opcode and sequential indices starting at zero. A **FINISH** packet (`0x31`, zero payload, next expected data sequence) marks the session verified only after the final length and whole-image CRC32 match.

The new host test encodes/decodes the manifest and full-size chunks, exercises retransmitted last chunks and duplicate manifests, rejects wrong finish sequences and declared-length overflows, and refuses to mark an image with the wrong checksum as verified. These are protocol building blocks only: GameCube and GBA hardware still run the previously verified seven-byte diagnostic, with no ROM dumping, save writes, or new hardware-test request.

## Framed manifest + ACK/NACK bridge (host only)

`include/cubedeck/session_wire.h` and `protocol/session_wire.c` adapt complete CRC-protected wire frames to the manifest-first verified-session receiver. Valid manifest and data frames receive sequence-specific ACKs; invalid CRC, missing manifest, out-of-order chunks, and a failed FINISH checksum produce a NACK for the next expected data sequence. Duplicate manifest, duplicate last chunk, and duplicate FINISH are idempotent. The CI test simulates a corrupted manifest, corrupted data packet, lost chunk acknowledgment, out-of-order FINISH, and mismatching final image checksum.

**This code is host-only and deliberately not connected to the GBA interrupt handler or GameCube SI state machine.** The existing tested XOR Joybus response semantics are preserved. Any future hardware framing must schedule ACK frames in a way compatible with the existing link timing. There are no memory-card or cartridge writes.

## Incremental word stream parser (host only)

`include/cubedeck/packet_stream.h` and `protocol/packet_stream.c` accumulate incoming Joybus-sized 32-bit words into a bounded CDTP frame. They reject unsupported version and overlong declared payload before buffering the full message, and only deliver frames whose CRC and padding validate. A magic value embedded in payload is treated as payload, not a reset command. The host test sweeps all 257 possible payload lengths (0 through 256), including payload-embedded magic values, and verifies bad-CRC and invalid-header recovery.

This parser is **not** yet attached to the GBA IRQ routine or GameCube SI callback. A truncated valid-looking frame cannot be recognized without a timeout, so any future hardware integration must call `cdps_init` when the session restarts or a frame times out. No real-hardware test is needed for this host-only layer.

## Full host-only sender/receiver simulation

`include/cubedeck/session_sender.h` and `protocol/session_sender.c` now provide the missing sending side of the manifest-first transaction: send the expected length/CRC32 manifest, then sequential DATA frames, then FINISH. The sender requires a sequence-specific ACK at each step, allows at most three caller-triggered retransmissions per step, and fails closed when retries are exhausted. The accompanying CI simulation exercises empty, 1-byte, 257-byte, and 4096-byte transfers all the way through the framed receiver, including a corrupted DATA frame and simulated lost acknowledgment.

This is still **host-only simulation**, not bulk data transfer running on GameCube or GBA hardware. The known-good console sources are unchanged; the GCMCE/memory-card incident remains unresolved.

## Complete word-by-word host simulation

The `tests/test_word_level_session.c` job now simulates both sides of the cable at the **32-bit Joybus-word level** rather than passing fully assembled DATA packets directly. The sender serializes MANIFEST, DATA and FINISH into CDTP words. An incremental parser accepts the words, independently checks CRC32, and passes validated packets to `cdsw_receive_decoded`, which applies session sequence, manifest and checksum rules. ACKs travel back through a separate word parser. The test includes deliberately damaged DATA CRCs, a lost ACK, the final 4096-byte transfer integrity check, and resetting the word parser after a truncated-frame timeout. The receiver code does not read any memory card or cartridge.

This remains **host simulation only**: it does not prove physical packet streaming above the already confirmed seven-byte hardware sample. The hardware IRQ and GameCube SI callback paths are unchanged. Future console wiring must address packet-completion timing, timeouts, and how ACK packets are delivered over GBA Joybus without breaking the existing XOR diagnostic.

## Larger RAM-light receive prototype (host only)

`include/cubedeck/stream_receiver.h` and `protocol/stream_receiver.c` add an incremental, **no-built-in-storage** receiver with an abstract chunk consumer callback. Instead of buffering an entire transfer in the GBA's memory, the receiver retains only the current 256-byte DATA chunk for duplicate detection, a rolling CRC32, the manifest, counters, and sequence state. It checks the final byte count and whole-image CRC at FINISH. An external consumer callback must provide atomic success/failure semantics: nonzero returns are treated as no bytes committed. There is intentionally **no** memory-card, cartridge, or file-writing consumer yet.

The sequence number bounds each independent streaming session to **65,535 DATA chunks** (maximum 16,776,960 bytes at 256 bytes/chunk), so larger ROMs would need multiple validated sessions or a future protocol version. This is a *design limit*, not a measured cable throughput or supported ROM dump capability.

`tests/test_stream_receiver.c` uses a RAM-only mock sink to stream **32,771 bytes**, verify CRC32, simulate a failed consumer write (with no progression), detect duplicate replay and corruption, and reject lengths above the protocol limit. This does **not** imply the runtime streams real GBA game data yet.

## Storage guard in CI

`tests/check_runtime_storage_writes.py` rejects selected direct GameCube CARD, filesystem-write and GBA save-write API references found in the project's runtime and shared protocol C files. This is intentionally a **narrow static guard**; it cannot rule out indirect writes by libogc, Swiss, FlippyDrive or GCMCE firmware. The earlier memory-card incident remains unresolved, so disconnect the GCMCE for any future experimental hardware test.

## Framed 128 KiB+ host-only streaming prototype

The RAM-light receiver now has a `cdstw_receive_frame` bridge in `protocol/stream_wire.c`, which checks packet CRC32 and returns framed sequence-specific ACK/NACK packets. The new `protocol/stream_sender.c` is a callback-driven, bounded-memory sender: the caller supplies the image length and its precomputed CRC32, plus a reader callback. It caches the **current** packet so retries reproduce identical bytes even when the source callback would change. Neither side includes filesystem, cartridge, or GCMCE access.

CI covers a **131,077-byte (128 KiB plus 5 bytes)** manifest-to-DATA-to-FINISH transfer over these two protocol components, plus corrupted frames, rejected consumer operations, lost acknowledgements, stale ACKs, and failed source reads. The built-in streaming session limit remains **16,776,960 bytes**, based on 65,535 data sequence numbers and 256-byte packet payloads. Actual physical Joybus bulk streaming has *not* been tested or wired into the console applications.

**Protocol limitation:** current stop-and-wait ACK packets identify sequence only, not their corresponding opcode/phase. The MANIFEST and initial DATA packet both use sequence zero. The future hardware adaptation must disambiguate delayed ACKs across phases (for example by including the acknowledged opcode or a session identifier) before claiming robust bulk transfer. CRC32 guards accidental corruption but is **not** cryptographic authentication.

## Streaming ACK phase binding

The host-only streaming bridge now tags each successful ACK with **one payload byte identifying the acknowledged opcode**. The streaming sender accepts an ACK only when **both** the sequence and the acknowledged opcode match its cached outstanding packet. This closes a stop-and-wait ambiguity where a delayed ACK for MANIFEST sequence 0 could otherwise be mistaken for DATA sequence 0. Tests inject that exact stale-manifest-ACK case and confirm it does not advance the sender.

NACKs still use zero payload and report the next sequence expected by the receiver. This change applies only to `cdtx_sender`/`cdstw_receive_frame`; earlier RAM-only protocol components retain their older ACK format and require the same phase distinction before being used in a real hardware bulk-transfer workflow.

## Full streaming session through two word parsers (host only)

`tests/test_stream_word_link.c` runs a **65,539-byte** manifest/DATA/FINISH exchange through the bounded sender, 32-bit-word packet encoder, incoming incremental parser, RAM-only streaming receiver, ACK generator, outgoing incremental parser and sender reply checker. The simulation injects a damaged data CRC, a truncated frame with explicit parser timeout/reset, and a lost ACK that forces an exactly duplicated DATA replay. It checks final bytes and CRC, all without touching the GameCube or GBA runtime paths. This is an additional **host-only** integration milestone, not proof of physical bulk-link operation.

## Constant-memory manifest CRC preflight

`protocol/stream_hash.c` adds `cdsh_source_crc32`, which computes IEEE CRC32 over any supported source size via the same read-only callback used by the streaming sender, in **256-byte increments**. This supplies the manifest's expected CRC without first allocating an image-sized buffer. Its host-only test compares against the reference full-buffer CRC for boundary lengths up to 131,077 bytes and checks callback failure without publishing a partial checksum.

The caller must guarantee the source remains identical between the preflight hash pass and transmission. This is still callback infrastructure only: no implementation reads cartridge pins, saves, Swiss SD files, or memory cards.
