# Separate 16 KiB verdict-wait experiment

**Not physical-hardware validated.** This RAM-only fixture improves waiting
for packet acceptance and displays the last retransmission's cause. It
does not modify the existing startup, recovery, 1 KiB or original 16 KiB
target behavior, and does not access cartridges, saves or GameCube storage.

## Separate pair

- Targets: `ram16-wait-gamecube` and `ram16-wait-gba` in the probe Makefile.
- Artifacts: `cubedeck-v2-ram16-wait-gamecube` and `cubedeck-v2-ram16-wait-gba`.
- Files: `cubedeck-v2-ram16-wait.dol` and `cubedeck-v2-ram16-wait.gba`.
- Readiness signature `43565736`; transfer prefix `B3CD0000`.

Use only the matching pair from one passing CI run, with the GBA on Port 2.
A runs one image, Y four fresh sessions, B interrupts. Payload, 128 ordered
chunks, 16,384-byte image and CRC `E7638E98` are unchanged from RAM16.md.

## Pending is a bounded wait

Previously every non-ACK verdict triggered a frame retransmission, including
PENDING. A new FRAME invalidates outstanding worker snapshots. The previous
16 KiB emulator run completed with one extra retry; its cause remains
unproven. This change addresses the independently demonstrated PENDING
behavior, without attributing that earlier retry to it.

Only PENDING for the exact current session and chunk qualifies for waiting.
The sender waits the existing 12-frame gap and queries again, without sending
FRAME or payload again. The GBA worker epoch stays intact. Seven successive
PENDING replies may wait; the eighth exhausts that attempt and triggers a
bounded frame retry. Two retransmissions are allowed, giving at most 24
matching PENDING responses if a peer never publishes a verdict. Ordinary
matching NACK retries immediately. Stale/wrong-session/chunk replies do not
qualify for waiting or acceptance. SI errors and watchdogs retain terminal,
single-owner behavior; no callback cancellation is claimed.

The screen shows total matching PENDING replies and the last retried chunk,
numeric cause and raw reply. Causes are 0=none, 1=matching NACK, 2=invalid or
unrelated verdict, 3=word echo mismatch, 4=PENDING limit. The deliberate NACK
normally records chunk 2, cause 1. Diagnostics survive subsequent successful
chunks but clear at a fresh session. A session-negotiation echo error is
terminal, not a packet retry, and retains the existing data-error display.

## Tests and limits

- Seven complete 16 KiB transfers delay final worker publication by one
  through seven query rounds. Final acceptance keeps the same epoch,
  correct whole-image CRC and just the one deliberate retry.
- Never-ready exhaustion, unrelated PENDING/ACK/NACK, all four retry causes,
  fresh-session diagnostic clearing and tick wraparound are covered.
- The common matrix still checks 18,041 image/packet faults and all 65,535 IDs.
- The SI adapter covers two full sessions and 3,632 sampled operation fault
  sites, plus interruption, watchdog, callback error and rejected submission
  during the first and last allowed delayed-query cycles. SEND and READ
  ownership are tested separately, including pending command preservation.
- CI uses address/undefined-behavior sanitizers and checks the GBA worker
  snapshot's EWRAM placement. Local leak scanning is disabled because the
  managed runner cannot inspect process task directories.

The 16,256-byte IRQ-excluded snapshot copy and external-RAM worker from the
16 KiB fixture remain; their physical timing is still unverified. This is a
synthetic transport experiment, not GBA game execution. Host tests, console
builds and any later emulator observations are separate evidence.
