# CubeDeck RAM program upload v9 — opt-in, hardware unverified

Keep the hardware-verified v7 storage DOL, v8 readiness DOL and existing
`cubedeck/joyboot-probe.gba`. This new experiment has separate file names.

## SD setup and one focused test

1. Extract this ZIP to SD root: `cubedeck-ram-upload-v9.dol` and
   `cubedeck/ram-status-v9.gba`. Do not replace the existing v7 probe.
2. Copy your own already-dumped 16 KiB GBA BIOS to
   `cubedeck/gba-bios.bin`. It is not included in the ZIP. This version accepts
   the user's tested BIOS revision with CRC32 `81977335`; other revisions
   fail closed with file error -29. This is revision identification, not a
   cryptographic authenticity check.
3. Connect the GBA to GameCube controller port 2, with **no cartridge**.
   Power it on and wait at the BIOS boot screen.
4. Launch the new DOL through Swiss. Confirm storage error 0, then press A once.
5. Expected: GBA screen **cyan** and GameCube **PASS: RAM program replied CDR9**.
   Photograph both screens. This outcome remains unverified on physical hardware.
6. On any failure photograph diagnostics, then restart both consoles before
   another attempt. Never retry within the same DOL after a terminal failure.

The template in this ZIP deliberately has zero logo bytes and cannot boot by
itself. The GameCube reads the user's BIOS, validates its size/revision and
copies its 156-byte header logo at BIOS offset `0x3290` into the payload **in
RAM only**. No patched GBA file or BIOS is published. Neither SD file changes.
No BIOS, Nintendo assets or commercial ROM is included in the ZIP/repository.

File errors: -27 nonzero template logo; -28 BIOS file unavailable; -29 wrong
BIOS length/revision; -30 invalid controller start. Existing SD/file errors
retain their meaning. Only the CDRAM9 template is accepted; image limit 16 KiB.

## Operation and limits

The new controller uses unchanged v8 RESET/STATUS/key readiness, then sends a
client key, header, encrypted payload and CRC through Joybus. The SI adapter's
little-endian key word is converted to the original PPC sender's big-endian
raw-key convention before encryption. Header command bytes preserve their
original image order: the isolated v9 adapter corrects the existing portable
encoder's reversed header words without changing v7/v8 or that shared module.
The final BIOS read is displayed as
untrusted and never counts as execution. After a startup delay, bounded reads
must obtain the status program's distinct `0x39524443` reply (CDR9).

The tiny GBA program paints VRAM cyan and publishes that fixed Joybus word.
It has no cartridge reader, save code, filesystem or storage operations.
It uses the devkitARM RAM/multiboot startup specification `gba_mb.specs`.

Each accepted SI operation has a 180-frame watchdog, and the whole attempt has
9000-frame cap. Runtime replies are polled at most 20 times. Refused native
submissions retry only within an existing deadline; encrypted words are rolled
back only when SI never accepted them. Errors/timeouts are terminal. Static
buffers retain ownership after timeouts; there is no cancellation or restart.
No automatic retransmission occurs after an accepted but uncertain write.

Host tests cover key byte order, actual-runtime versus BIOS-only acceptance,
refused-word rollback, owned buffers on watchdog expiry, stale/error callbacks,
busy bounds and clock wraparound. The interrupted-operation matrix reaches real
submitted client-key, header, encrypted-body, CRC, final-BIOS-read and runtime-read
operations. Each is tested with a missing callback, wrong token, SI error,
callback exactly at the deadline and callback after the deadline (30 faults),
plus valid completion one tick before expiry. Tests assert terminal failure,
no further submission or restart, no rollback of uncertain bytes, and retention
of buffers for missing callbacks. Separate tests reject a BIOS-only runtime
marker, exhaust all 20 bad runtime replies, bound refused final reads and enforce
the overall attempt deadline. These faults are host simulations, not injected
electrical faults or physical interrupted-session validation.

[Direct SI adapter tests](SI_ADAPTER_TESTS.md) additionally compile the real
adapter with stubbed libogc SI/IRQ entrypoints to check exclusive ownership,
completion timing, IRQ masks and cleared responses. The v9 package is gated
on these checks; the previously delivered hardware-test ZIP is unchanged.

CI tests and compilation do not establish
hardware or emulator execution. v7/v8 sources and all proven diagnostics remain
unchanged. No cartridge ROM/save, memory card, SD or GCMCE writes occur.

## Observed Dolphin execution — October 11, 2026

The exact released DOL from source `eab432c6d9767c83a710c64e0d07e89724310954`
and [CI run 38172031280](https://github.com/PercAngle312/CubeDeck/actions/runs/38172031280)
(84/84 successful jobs) was run in our opt-in patched SD2SP2 Dolphin environment.
DOL SHA-256: `bfb85a73478c71e253f8205092c5c86ad9c3f54021f721c15c4626deff47c453`.
The logo-free 2064-byte GBA template SHA-256:
`bd39f628f715c5d7916519f177177c17c97acc5752eb2f0c45ac4540ae531cac`.
The disposable SD image included the user's private BIOS, which is not published.

The GBA window turned cyan and the GameCube screen displayed **PASS: RAM program
replied CDR9**, runtime reads 1 and RAM reply `39524443`. Storage mounted and
loaded the 2064-byte template. The runner checked that the DOL and synthetic SD
image hashes did not change. This establishes program upload and execution in
that emulator setup; it does not establish physical v9 execution, cartridge
access or interrupted-session reliability. Physical testing was deferred at
the user's request while away from home. The already-delivered v9 ZIP remains
the next hardware test; this later evidence/test commit does not replace it.

For an emulator failure check, one byte in the BIOS file within a separate
disposable SD image was flipped before launch. The same exact v9 DOL mounted
the image and loaded the template, but displayed file error **-29**, runtime
reads 0 and RAM reply 0 after the runner pressed A. The GBA stayed at its BIOS
boot screen. The runner again verified unchanged DOL/image hashes during the
run. This validates that the revision/file gate blocks upload for this corrupt
file; it is not a simulated SI disconnect or a real-card corruption test.
