# CubeDeck — GameCube homebrew starter

CubeDeck is a Nintendo GameCube homebrew dashboard with an early GBA link-cable development path. Cloud builds mean you do **not** need to install a compiler on your PC.

## Cloud builds

Open the repository's **Actions** tab and run **Build CubeDeck**, or let a push trigger it. The workflow publishes two artifacts when both jobs succeed:

- `cubedeck-gamecube`: GameCube `.dol` and `.elf`
- `cubedeck-gba-diagnostic`: standalone GBA diagnostic `.gba` and `.elf`

The GBA diagnostic is intended to run from your SuperCard/flashcart. CubeDeck does not yet upload it over the link cable. If a build fails, use the workflow log as the source of truth; do not test a missing or incomplete artifact.

## Current GameCube features

- Text console UI and GameCube controller navigation
- System information and About pages
- Four-port GameCube controller monitor
- GBA link cable detection on GameCube Port 2 using asynchronous SI device-type polling
- Callback-driven Joybus RESET/STATUS and two-way diagnostic word exchange
- One-round verification (X) and a 16-round sequential-word stress test (Y)

**Physical-hardware validation:** the diagnostic exchange works, and the maintainer confirmed **16/16 replies valid** in the sequential-word stress test. This is not yet bulk ROM transfer or TV playback.

## GBA diagnostic payload

The separate `gba-diagnostic/` target is an experimental GBA-side test ROM. It switches the GBA link port into Joybus mode, prepares a deterministic XOR reply, and changes its screen color when it receives data. It does not read or write cartridge ROM or save memory.

## Hardware validation

1. Confirm the Actions workflow succeeds and both artifacts are present.
2. Keep a copy of the last known-good `.dol` and `.gba`.
3. Launch the GameCube artifact from Swiss and run the GBA diagnostic from the flashcart.
4. Open the GBA link page and press X for one round or Y for the 16-round stress test.

## Roadmap

1. A portable packet framing v1 has been added and host-tested in CI; it is not yet connected to the running GameCube/GBA code.
2. Prove byte-for-byte transfer of a small test payload on real hardware.
3. Evaluate established GBA Link Cable Dumper code/workflow for reading retail cartridges; keep SuperCard SD-file access as a separate question.
4. Once ROM acquisition is proven, evaluate a GameCube-compatible GBA emulator/frontend and storage path for TV playback.
5. Investigate save access separately, starting read-only and protecting existing saves.

CubeDeck does not modify IPL, system settings, cartridge ROM, or save data. See [docs/gba-link-transport.md](docs/gba-link-transport.md) for transport details and validation rules.
