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

The diagnostic exchange is verified on physical hardware. This sends only test words and XOR replies; ROM streaming and save access are not implemented.

## GBA diagnostic payload

The separate `gba-diagnostic/` target is an experimental first GBA-side client. It switches the GBA link port into Joybus mode, enables serial interrupts, prepares a fixed signature response, and changes its screen color when it receives data. It does not read or write cartridge ROM/save memory.

The source and cloud build target are committed and compile in CI. The maintainer has confirmed the real-cable test on hardware: the GBA screen changes color and the GameCube receives the expected XOR reply. The GBA diagnostic does not read cartridge ROM or save memory.

## Hardware validation

1. Confirm the Actions workflow succeeds and both artifacts are present.
2. Keep a copy of the last known-good `cubedeck.dol`.
3. Launch the new GameCube artifact from Swiss and confirm the dashboard still boots.
4. Run `cubedeck-gba-diagnostic.gba` from the GBA flashcart while the cable is connected to GameCube Port 2.
5. At this stage, the GameCube app only detects the GBA; it does not yet send diagnostic words.

## Safety and roadmap

CubeDeck does not modify IPL, system settings, cartridge ROM, or save data. Next, add a bounded GameCube Joybus test command path with callback-based completion and timeouts, then verify the signature/counter round-trip. Cartridge reading and save access remain later, separately validated milestones. See [docs/gba-link-transport.md](docs/gba-link-transport.md).
