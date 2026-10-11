# Separate startup/recovery experiment (not hardware validated)

The maintainer reported the existing ACK/NACK pair passed **3/3 packets,
cyan GBA, Retries: 1**, after rebooting both consoles without changing files.
That is the hardware baseline, belonging to the same installed files after
reboot, not a subsequent diagnostic revision. The recorded passing ACK build
is [Actions run 38095586629](https://github.com/PercAngle312/CubeDeck/actions/runs/38095586629)
(38/38 jobs), and the result is recorded in `ACK.md` at documentation commit
`e966353f2638e5ca027a8c48f6c0bd9093ae87d5`.
The starting repository head for this work was `7abdaf9`; its newer portable
controller CI passed before these changes. This experiment has **only software tests and
cross-compilation** until the maintainer tests it on real consoles.

The known-good ACK, multi-frame, production and diagnostic sources are
unchanged. Existing artifacts are not replaced: this uses new filenames,
build targets, source files and artifact names. Keep the proven pair locally;
a rebuild using floating toolchains is not a claim of binary identity.

## Controller review and changes

`protocol/joybus_recovery.c` previously treated successful STATUS transport
as READY, had no settling delay or interrupted-session drain, and could wrap
a command token back to an old value. The new optional configuration adds:

- RESET → settle → STATUS → settle → application readiness READ → READY.
  A successful SI callback alone does not prove GBA application readiness.
- Bounded readiness reads with settling between attempts. Exhaustion stops
  with `CDJR_ERR_NOT_READY`; no automatic RESET/error retry loop.
- Explicit transfer rejection: SI_Transfer returning zero means no callback
  was scheduled. Buffer ownership is released without waiting for a watchdog.
- Explicit interruption: stop scheduling, retain any outstanding buffers,
  drain only the matching completion, then permit a fresh RESET session.
- Callback deadline checks even when completion is processed before poll.
- Permanently latched timeout. A late matching completion releases software
  ownership but does not restart the link. Reset/relaunch is required.
- Refusal to issue commands after token exhaustion; no wrapped-token alias.
- Modular elapsed tick comparisons, with intervals below half the tick range.

The old RESET/STATUS-only API remains available to host-only legacy callers.
The new SI adapter uses the timestamped completion API exclusively.

## Opt-in binaries

Build from the repository root:

```
make -f experiments/v2-probe/Makefile startup-gamecube
make -f experiments/v2-probe/Makefile startup-gba
```

Actions uploads `cubedeck-v2-startup-gamecube` and
`cubedeck-v2-startup-gba`, containing `cubedeck-v2-startup.dol` and
`cubedeck-v2-startup.gba`. Building does not activate the experiment.
Use this pair together; it is not compatible with the proven ACK companion.

The minimal fixture checks startup and a synthetic session-word echo,
not three CRC-verified packets. It does not perform cartridge ROM/save
operations, GameCube storage operations, or bulk transfer.

### Ownership and interrupted sessions

The libogc callback has no context argument. The adapter uses one immutable
owner descriptor until the callback mailbox is consumed. The callback copies
its submission token and kind into the mailbox; it never looks up a new
software session token. Descriptor publication, mailbox consumption and
submission are protected with IRQ_Disable/IRQ_Restore. Only the owner loop
changes controller state or queues another command. The fixture does not issue GetTypeAsync and disables Port 2 polling.
It also avoids PAD_ScanPads, whose missing-pad rescan would otherwise queue
PAD_Reset/GetType operations for the GBA port. PAD_Read supplies Port 1
button edges without that rescan. Initial PAD_Init discovery can still be
busy; a rejected submission fails safely and can be retried explicitly.

B stops scheduling immediately. The loop continues servicing the mailbox
and watchdog even while interrupted. It does not clear pending state or
reuse buffers. A/Y can start again only after an accepted command drains;
a timeout remains terminal. Data-stage timeouts also remain terminal.
Errors from readiness/startup can be retried explicitly with A/Y after drain;
data-stage errors require relaunching this small diagnostic fixture.

The GBA serial IRQ prioritizes RESET over simultaneous old RECEIVE flags,
clears its RAM session/reply state, and publishes the readiness signature.
The small receiver runs entirely in IRQ context, so there is no deferred
old-session packet decoder to publish a stale verdict. JOYCNT flags are
acknowledged using the captured write-one-to-clear mask.

Hardware reference: [libogc SI_Transfer implementation](https://github.com/devkitPro/libogc/blob/master/libogc/si.c)
returns zero when it cannot schedule a transfer. JOYCNT reset/receive flags
and their write-one-to-clear behavior are described in the
[GBA programming manual](https://mutantstargoat.com/~nuclear/tmp/gba_prog_manual.pdf).

## Software validation

- `tests/test_joybus_recovery.c`: 32 phase-fault/readiness scenarios plus
  argument bounds, legacy flow, token exhaustion and wrapped deadlines.
- `tests/test_startup_adapter.c`: actual GameCube adapter compiled against
  an SI double; rejected submission, wrong channel, immediate callbacks,
  interrupted startup/data sessions, startup/data watchdogs and 16 resets.
- `tests/test_startup_fixture.c`: 1000 fresh RAM-only receiver sessions;
  reset clears prior session word and reply.
- All three run in CI with AddressSanitizer/UndefinedBehaviorSanitizer.
- The controller also cross-compiles to PPC and ARM; both experimental
  executables build with warnings treated as errors.
- Runtime storage API audit now includes experimental C sources. This is
  a source guard, not a guarantee about Swiss or flashcart firmware behavior.

## Later hardware test (only when back at the consoles)

1. Keep the proven ACK pair, and use distinct startup filenames on SD.
2. Run the startup GBA companion and launch the startup DOL from Swiss.
3. Use controller Port 1 and GBA link Port 2. A tests one fresh session;
   Y tests 16 fresh RESET/readiness/echo sessions without rebooting the GBA.
4. Record Completed, Session, Ready reads, Error, Data error and SI result.
   GBA blue means boot/RESET ready; cyan means session word received, not
   a CRC-verified packet pass. The GC echo verdict is also required.
5. B during RESET, readiness or data interrupts; after drain press A again.
   Confirm no timeout, old-session completion or spurious automatic restart.
6. If it times out, record the screen and reboot/relaunch both consoles;
   the fixture deliberately does not resubmit on an unproven SI state.

CI cannot establish startup reliability, cable timing, RESET interrupt
behavior or recovery after physical unplugging. Those remain unvalidated.
