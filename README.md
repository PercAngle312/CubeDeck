# CubeDeck — GameCube homebrew starter

CubeDeck is a conservative first-stage Nintendo GameCube homebrew utility dashboard written in C with libogc. The project is configured for a cloud build so you do **not** need to install devkitPro or a compiler on your PC.

## Cloud build (recommended)

1. Open the repository's **Actions** tab.
2. Select **Build CubeDeck** and press **Run workflow**, or let a push trigger it automatically.
3. Open the completed workflow run and download the `cubedeck-gamecube` artifact.
4. Extract the artifact and copy `cubedeck.dol` to your SD card for launch through Swiss.

The workflow uses the `devkitpro/devkitppc` container. If the cloud build fails, use the workflow log as the source of truth; do not test a missing or incomplete artifact on hardware.

## Current features

- Video initialization using libogc's preferred video mode
- Text console UI and GameCube controller navigation (D-pad, A, B)
- System information and About pages
- Four-port GameCube controller monitor (connection status, buttons, sticks)
- **GBA link cable detection check** on GameCube controller Port 2 using asynchronous SI device-type polling

The GBA link page is only a device-presence check. It does **not** yet upload a GBA program, exchange Joybus payload data, read a cartridge, or access saves.

This is a starter build, not a full system manager. It does not modify memory cards, IPL, or system settings.

## Real-hardware launch

Use the homebrew launch method already supported by your console setup (for example, Swiss from SD). Copy the downloaded `cubedeck.dol` to your SD card and launch it from Swiss. Keep a known-good Swiss setup and the original project archive untouched.

## Validation checklist

1. Confirm the GitHub Actions build succeeds and both expected output files are present.
2. Launch the `.dol` from Swiss on the physical console and confirm the dashboard still displays correctly.
3. Confirm D-pad selection works, A opens each page, and B returns.
4. Check all four controller ports.
5. With a compatible GameCube-to-GBA link cable connected to Port 2 and the GBA powered on, open **GBA link cable check** and observe whether it reports a GBA device. Also test with the cable disconnected.
6. If it fails, record the console region/video cable, display mode, and exact failure point before changing code.

## Honest status

The code change adding GBA detection has been committed, but this environment has not run the cloud build or tested the new check on Dolphin or a physical GameCube. A successful compile must be confirmed in GitHub Actions; actual cable detection must then be verified on hardware. Do not treat compilation alone as proof of communication.

## Roadmap

See [docs/gba-link-transport.md](docs/gba-link-transport.md) for protocol constraints and the staged implementation plan. Next: use the proven multiboot/SI implementation as a reference, add a tiny GBA-side diagnostic payload, and only then implement bounded Joybus request/response handling. Cartridge ROM reading and save access remain later, separately validated milestones.
