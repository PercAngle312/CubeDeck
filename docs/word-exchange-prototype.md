# Nonblocking Joybus word exchange: host-only prototype

`protocol/word_exchange.c` adds a compact, platform-agnostic state machine
for sending a session-bound frame one 32-bit word at a time and processing a
CRC-validated response through an incremental parser. It builds on the v2
sender/receiver adapter and the host-tested stop-and-wait mechanism.

The API intentionally does **not** call GameCube SI, GBA link registers,
firmware, timers, filesystem or cartridge slots:

- `cdwx_init` primes the first framed packet with a caller-selected
  nonzero transfer ID.
- `cdwx_next_word` yields one word for an external asynchronous driver to
  transmit. After the final word it waits for an ACK/NACK frame.
- `cdwx_reply_word` incrementally checks an incoming reply, ignores corrupted
  or unrelated responses, and advances the sender after a matching
  acknowledgment.
- `cdwx_timeout` resets the reply parser and replays the cached *identical*
  outgoing frame, counting against the sender's bounded retry budget.
- A current-session, sequence-matching NACK triggers an immediate resend of
  the cached outgoing frame. The NACK itself has already consumed one retry;
  the scheduler does not charge a second retry. An unrelated session's NACK
  cannot trigger retransmission.

The host integration test simulates a transfer of 1,527 bytes and injects
a dropped ACK and a corrupted ACK. Exact final byte count and full-image
CRC32 must still pass. Both GameCube PowerPC and GBA ARM toolchains compile
the module in their independent cross-compilation CI jobs.

## Why this is not a hardware transfer yet

An external driver must coordinate when an SI transaction completes, enforce
timeouts, and keep callbacks/interrupt handling free of unbounded processing.
The GameCube and GBA command/reply ordering, transport error codes, and
multiboot handoff must be tested in a **separate opt-in console target**.
Source-only compilation and simulated words do not establish real Joybus
timing. The known-good diagnostic remains the only hardware-proven framed
packet implementation so far, and GCMCE is not used by these modules.
