# Portable Joybus startup and recovery controller

This is **host-tested prototype infrastructure**, not connected to the
hardware-tested `gamecube-ack.c` binary yet. The verified 3/3 cyan,
`Retries: 1` firmware remains unchanged.

`include/cubedeck/joybus_recovery.h` and
`protocol/joybus_recovery.c` define a small asynchronous controller for
RESET → STATUS → READY. It does not issue real Joybus commands or access
cart ROM, saves, flashcart files, GC memory cards, or GCMCE.

The caller sends the returned RESET/STATUS action, retains its callback
token, and only invokes `cdjr_callback` with the same token when the
asynchronous SI completion actually arrives. Results are:

- Correct successful RESET callback transitions to STATUS
- Successful STATUS callback transitions to READY
- A late/stale token does not change the phase or satisfy a different
  command; tokens are monotonic across clean application sessions
- Callback errors abort with the failing numeric result
- A watchdog expiry aborts permanently and **does not requeue a transfer**
- Tick comparisons tolerate 32-bit wrapping
- After any watchdog expiry the controller forbids restarting via
  `begin()`; physical SI may still own command/reply buffers. The host
  must externally prove SI quiescence / reinitialize the controller safely
  (or power-cycle) before attempting another session

`tests/test_joybus_recovery.c` simulates all these behaviors without
hardware and is run by GitHub Actions.

The next hardware experiment should integrate this controller in a
**separate DOL** only, first proving startup repeatability without adding
ROM operations. Before connection, add explicit SI callback token ownership
rather than assuming the callback knows which request was completed.

Do not claim this CI test solves intermittent real-console startup; the
user's restart on unchanged binaries demonstrated recovery but the
physical timing root cause has not yet been isolated.

## October 2026 opt-in startup-sync experiment

Branch `experiment/joybus-session-sync` adds an **unverified**, independent
`cubedeck-v2-ack-sync.dol` from `gamecube-ack-sync.c`. Build using:
`make -f experiments/v2-probe/Makefile ack-sync-gamecube`.

The separate DOL inserts eight VSync intervals after successful RESET
and after successful STATUS before transmitting framed data. It reuses
the existing GBA ACK fixture; **do not replace the previously proven
`cubedeck-v2-ack.dol` or GBA ROM**. This experiment is not a fix verified
on real hardware. Only test it after safely rebooting both devices and
when access to hardware is available.

The portable host controller now optionally waits an owner-controlled
tick interval between RESET and STATUS, and refuses to reuse an
interrupted in-flight SI request (until external quiescence and explicit
reinitialization). An interruption with no request in flight may be
followed by a new software session. Nothing in this controller performs
ROM, save, or storage writes.

**Known limitation:** the alternate DOL still uses the legacy ACK probe's
SI callbacks. It does not yet integrate `cdjr_link` token ownership; do
not claim that the controller's host fault matrix proves the DOL immune to
late hardware callbacks or hardware startup races.
