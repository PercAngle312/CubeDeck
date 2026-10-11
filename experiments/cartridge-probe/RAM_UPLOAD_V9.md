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
busy bounds and clock wraparound. CI tests and compilation do not establish
hardware or emulator execution. v7/v8 sources and all proven diagnostics remain
unchanged. No cartridge ROM/save, memory card, SD or GCMCE writes occur.
