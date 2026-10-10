# CubeDeck — GameCube homebrew starter

CubeDeck is a conservative first-stage Nintendo GameCube homebrew utility dashboard written in C with libogc. The project is configured for a cloud build so you do **not** need to install devkitPro or a compiler on your PC.

## Cloud build (recommended)

1. Put this project in a GitHub repository using the GitHub website (or upload the ZIP and push it with any existing workflow you already use).
2. Open the repository's **Actions** tab.
3. Select **Build CubeDeck** and press **Run workflow**, or push a commit to trigger it automatically.
4. Open the completed workflow run and download the `cubedeck-gamecube` artifact.
5. Extract the artifact and copy `cubedeck.dol` to your SD card for launch through Swiss.

The workflow uses the official `devkitpro/devkitppc` container, which includes the GameCube toolchain and development libraries. No local compiler installation is required. If the cloud build fails, use the workflow log as the source of truth; do not test a missing or incomplete artifact on hardware.

## Current features

- Video initialization using libogc's preferred video mode
- Text console UI
- GameCube controller navigation (D-pad, A, B)
- System information page
- Live controller input test
- About page
- Four-port GameCube controller monitor (connection status, buttons, sticks)

This is a starter build, not a full system manager. It does not modify memory cards, IPL, or system settings.

## Real-hardware launch

Use the homebrew launch method already supported by your console setup (for example, Swiss from SD). Copy the downloaded `cubedeck.dol` to your SD card and launch it from Swiss. Do not overwrite IPL or install anything to console flash memory. Keep a known-good Swiss setup and the original project archive untouched.

## Validation checklist

1. Confirm the GitHub Actions build succeeds and both expected output files are present.
2. Launch the `.dol` from Swiss on the physical console.
3. Confirm video appears, D-pad selection works, A opens each page, and B returns.
4. Open Controller test and check all four ports; connect a controller to different ports and verify the status and input values.
5. If it fails, record the console region/video cable, display mode, and exact failure point before changing code.

## Honest status

This source has **not yet been compiled or tested in this environment**, and it has not been tested in Dolphin or on a physical GameCube. The workflow is provided to perform the first reproducible build in the cloud. Do not consider a build successful until GitHub Actions reports success and the artifact is available.

## Roadmap

The next project milestone is to add a separate GBA-side link probe and GameCube-side receiver. Those are not included as working binaries yet. Cartridge ROM dumping, controller input over link, and direct save access must be validated independently before being described as supported. The current controller monitor only reads GameCube controller ports; it does not communicate with a GBA.
