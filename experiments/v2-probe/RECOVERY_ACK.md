# Opt-in startup recovery + three CRC/ACK packets

**Not hardware validated.** The confirmed hardware result remains the
original ACK pair: **3/3 verified, Retries: 1, GBA cyan**, after rebooting
both consoles without replacing any files. Neither that pair nor the
original diagnostic, single-frame or multiframe source is changed here.
The successful startup-only predecessor is commit `e03844f`, Actions run
[38097395555](https://github.com/PercAngle312/CubeDeck/actions/runs/38097395555)
(43/43 jobs); that result is software/build verification only.

This new experiment reuses the portable `joybus_recovery` controller and
its SI adapter from `gamecube-startup.c`. It adds a small **synthetic
three-packet fixture**, not a replacement bulk streaming protocol. Existing
`cdgu_encode`/`cdgu_decode` perform frame/session/CRC verification. The
portable streaming engine remains separate; no cartridge or storage backend
is connected.

## Separate paired artifacts

- `cubedeck-v2-recovery-gamecube` → `cubedeck-v2-recovery.dol`
- `cubedeck-v2-recovery-gba` → `cubedeck-v2-recovery.gba`

Take both from the same passing Actions run. Building does not activate
them. Do not mix these companions with the ACK, multiframe or startup pair.
All earlier targets/artifact names remain stable and have separate filenames.

From the repository root:

```
make -f experiments/v2-probe/Makefile recovery-gamecube
make -f experiments/v2-probe/Makefile recovery-gba
```

## Session sequence

1. Asynchronous RESET → settling delay → STATUS → settling delay → bounded
   application readiness reads. Wrong companion signatures cannot start data.
2. A nonzero session number is negotiated after RESET. The receiver accepts
   a new number only when armed by RESET; an old negotiation cannot replace
   the active session. Numbers never wrap/reuse within a running GC process;
   exhaustion fails closed.
3. Before each packet/retry a session/sequence-bound FRAME marker explicitly
   clears partial words and invalidates any deferred CRC job.
4. Each of the same seven logical v2 sample words is sent as tagged low/high
   halves and each **wire word** receives the original XOR echo check.
   Tagging is specific to this diagnostic. It prevents arbitrary CRC/payload
   bits from colliding with control words; it is not the old seven-wire-word
   experiment and does not redefine the existing CDTP format.
5. GBA validates the reconstructed packet's CRC, session ID, opcode, sequence
   and exact seven-byte sample payload outside the serial IRQ.
6. After a bounded verification gap, GC sends a session/sequence-bound QUERY.
   ACK/NACK/PENDING responses also bind both session and sequence.
7. Packet 2 receives one deliberate NACK. GC retries the exact same packet,
   with at most two retries per packet. Lost verdicts are idempotent: a
   retransmitted already-accepted packet gets ACK without advancing twice.
8. GC reports three verified packets only after all expected packet ACKs;
   GBA cyan requires three validated packets. Both are needed for a future
   physical pass report.

RESET and frame restarts use monotonic receiver epochs. Main-loop CRC work
captures epoch/ID/sequence under IRQ exclusion, decodes with interrupts
enabled, then publishes only if all ownership fields still match. Old work
cannot commit an ACK after reset, session replacement or a same-session
frame restart. Epoch exhaustion also fails closed. IRQ exclusion includes
compiler memory barriers around shared GBA RAM.

SI submission, buffers and callback mailbox use the tested single-owner
adapter. Libogc must deliver at most one callback per accepted transfer;
the adapter cannot distinguish an invented duplicate callback after that
transfer was already drained because SICallback has no context argument.
Wrong controller tokens and delayed accepted completions are covered by
portable tests. An accepted transfer is never canceled by a software abort.

## Failure behavior

- SI callback errors/rejected submissions abort; they are not packet retries.
- A watchdog never resubmits a possibly pending SI operation. Ownership and
  command bytes remain retained until a matching completion is consumed.
  Timeout stays terminal even if completion eventually arrives.
- B stops scheduling; the owner loop still drains callbacks/watchdogs. After
  a timely drain, A starts a fresh RESET/session and discards partial packets.
- CRC failure, wrong echo, missing verdict, or stale/wrong ACK consumes the
  bounded packet retry budget only after SI completion. Every retry has an
  explicit FRAME restart, so a truncated packet cannot swallow the next one.
- Restart/disconnect of the GBA mid-packet invalidates session state. This
  fixture aborts after bounded failures; it does not guess SI cancellation or
  silently negotiate a session in the middle of a packet.
- Startup errors other than timeout can be retried explicitly once drained.
  Data errors/timeouts require relaunching this diagnostic; no infinite loops.

## Automated verification

`tests/test_recovery_packets.c` runs 343 corruption/replay/interruption cases
and checks all 65,535 usable session IDs. It includes every bit in all seven
frame words, missing half-words, lost/stale ACKs, retry exhaustion, valid-CRC
wrong payload/opcode/session/sequence, clock wrap, and stale deferred results
across RESET and same-session restart.

`tests/test_recovery_adapter.c` compiles the real experimental GC adapter
against an SI double. It completes 16 sessions and injects four fault types
at each of the 133 SI operations in a session (532 sites): interruption,
watchdog, callback error and rejection. It checks no chained command after
abort, byte-for-byte preservation of pending command buffers, terminal late
completions, fresh-session recovery and bounded missing-worker failures.

The new tests, existing startup tests and storage audit run in CI. Both DOL
and GBA ROM build with warnings treated as errors. These are simulations and
cross-compilation, not measurements of physical Joybus timing.

## Later physical test — user is currently away

When hardware is available, keep GCMCE disconnected and preserve all proven
files. Use the paired recovery files under their distinct names. Controller
Port 1, GBA cable Port 2. A runs three packets; Y repeats 16 fresh sessions;
B interrupts without canceling SI. Record startup/data errors, ready-read
count, sessions, packet count, retries, SI result and GBA color. Do not report
success from cyan alone or associate this new pair with earlier hardware
milestones. Larger RAM-only transfers remain the next separate milestone.
