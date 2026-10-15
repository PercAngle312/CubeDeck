# Opt-in SD2SP2 environment for CubeDeck tests

This is a local Dolphin test adapter, not upstream Dolphin SD2SP2 support. It
changes only the emulator; the released v7/v8 DOLs and original diagnostics stay
unchanged. It uses an image opened with `std::ifstream` (read-only), on EXI channel
2/device 0, enabled only by `CUBEDECK_SD2SP2_IMAGE`. With that variable absent,
Dolphin's original AD16 behavior remains. No physical SD or console is accessed.

## Reproduce

Use Dolphin source commit `b92e354389bb7c0bd114a8631b8af110d3cb3a14` (2407).
The apply script checks the revision and checks the patch before applying; it
refuses to overwrite an existing model header. It does not supply dependencies,
BIOS files, logos or commercial ROMs. Build Dolphin with its normal build tools.

```sh
python3 tools/emulator/apply_dolphin_sd2sp2.py /path/to/dolphin-source
cmake --build /path/to/dolphin-build --target dolphin-emu
python3 tools/emulator/make_sd2sp2_image.py \
  --v7-zip /path/to/cubedeck-sd2sp2-v7-cdd13b0c7539.zip \
  --output /path/to/new-disposable-sd.img
python3 tools/emulator/run_sd2sp2_test.py \
  --dolphin /path/to/dolphin-build/Binaries/dolphin-emu \
  --dol /path/to/cubedeck-joyboot-ready-v8.dol \
  --image /path/to/new-disposable-sd.img \
  --bios /path/to/your-own-gba-bios.bin \
  --out /path/to/new-test-results
```

The runner uses an isolated profile, no GameCube memory cards, an empty GBA ROM
slot, and a FIFO GameCube controller. It checks that the DOL and synthetic SD
image hashes remain unchanged. Inspect the screenshot; a living process alone
is not a test pass. Do not commit BIOS files or the generated profile. The image
builder creates a new file exclusively and refuses replacement. Its 128 MiB
sparse image contains protective MBR, CRC-checked primary/backup GPT, a FAT32
volume at LBA 2048 and the logo-free v7 homebrew fixture under the real long path
`cubedeck/joyboot-probe.gba`. It also checks that the input has no logo bytes.

## Reproduce the v9 RAM upload

The v9 ZIP contains the new DOL and a logo-free status template. Extract its
DOL separately; the image builder reads only named ZIP members and checks both
DOL/template SHA-256 entries against the package manifest. It takes a local
BIOS supplied by the user; no assets are fetched or bundled.

```sh
python3 tools/emulator/make_sd2sp2_ram_image.py \
  --v9-zip /path/to/cubedeck-ram-upload-v9-eab432c6d976.zip \
  --bios /path/to/your-own-gba-bios.bin \
  --output /path/to/new-private-v9.img
python3 tools/emulator/run_sd2sp2_test.py \
  --dolphin /path/to/dolphin-build/Binaries/dolphin-emu \
  --dol /path/to/cubedeck-ram-upload-v9.dol \
  --image /path/to/new-private-v9.img \
  --bios /path/to/your-own-gba-bios.bin \
  --out /path/to/new-v9-results
```

The image contains `cubedeck/ram-status-v9.gba` and `cubedeck/gba-bios.bin` on
disjoint FAT chains. **Keep this BIOS-containing image private: do not upload,
commit or distribute it.** The original BIOS and ZIP remain unchanged. Input
validation and BIOS revision checking happen before output creation. A target
that already exists is never replaced.

For an intentional revision-gate failure, `--corrupt-bios` flips one byte in
the image copy. `--allow-invalid-bios` is explicitly for failing fixtures and
synthetic host tests; it does not bypass the console's revision check or provide
a bootable replacement BIOS. Neither flag changes the source BIOS file.

CI independently decodes the two VFAT names/checksums, follows all FAT chains,
checks for shared clusters and compares both FAT copies, using synthetic bytes
for the BIOS file. It checks template sizes, invalid inputs, archive checksums,
duplicate members and refusal to overwrite. It does not run Dolphin with those
fake BIOS bytes. See [v9 evidence](../../experiments/cartridge-probe/RAM_UPLOAD_V9.md)
for the actual separately observed emulator execution.

## Model and tests

The model handles the libogc SDHC SPI initialization path, R1/R2/R3/R7 replies,
CMD17/CMD18 sector reads, CMD12 stop, CID/CSD/status data and CRC7/CRC16. It keeps
queued replies across the select boundaries used by libogc. It rejects write,
erase and lock commands and cannot write the image. The adapter also implements
EXI immediate read/write. An opt-in EXI channel fix completes deselected startup
clocks with all-ones reads; unpatched Dolphin leaves TSTART pending when no
chip select is asserted, stalling libogc's SD reset path before CMD0.

Host tests cover initialization, invalid CRC, selection boundaries, sector bounds,
streaming/stop, rejected writes and unchanged image bytes. The interrupted-session
matrix drops chip select at all five incomplete command lengths, resets a stream
before its data token and within its body/CRC, reinitializes after reset, reads
past the final sector, and sends data after rejected single/multiple-block writes.
Each case checks that the next permitted read returns the original sector and
that pending or failed operations do not leak stale bytes. These are model-level
tests, not physical hot-removal or power-loss tests. Separate Python tests
independently verify GPT/header CRCs, FAT32 BPB, VFAT filename and file chains,
including a maximum-size fixture and refusal to overwrite existing images.
CI tests the standalone protocol and image generator. The Dolphin build and
integrated run were performed locally; CI does not run Dolphin.

This is a bounded test model, not a full SD implementation. It does not model
card timing, electrical behavior, hot removal, native SD mode, arbitrary card
families or save states. Do not use Dolphin save/load states with this adapter.
References: libogc `sdgecko_io.c` and the
[SD Association simplified specifications](https://www.sdcard.org/downloads/pls/).

## Observed integrated result — October 11, 2026

The exact released v8 DOL from source
`9bb1e29ec2cf5919f190597474b37ffc3be49402` / CI run
[38165076417](https://github.com/PercAngle312/CubeDeck/actions/runs/38165076417)
was run without recompilation or bypassing the SD gate. DOL SHA-256:
`cd52ed4b81cef9115574099ba835151257643e11c82e065bc6702a7985144c1d`.
The image used the exact 3200-byte v7 probe SHA-256:
`868572ea88313743b157253bb1c4fcad6d245a3fd1c5e61c0568b7bb051fa168`.

On-screen: driver 1, card 1, mount 1, storage error 0, image 3200 bytes,
GPT error 0, mounted entry 1/LBA 2048. After A: READY, STATUS polls 1,
SI refusals 30, SI raw 0, pending 0, and **PASS: BIOS ready and session key
received.** Transient SI refusals stayed within the controller's bounded window.
The SD trace recorded `denied=0`: no write commands reached the emulator model.

This validates the released DOL's SD/file path and RESET → STATUS → key-read
path in the patched emulator with a synthetic disk and user-supplied BIOS.
The emulator result does not validate GBA program execution, payload upload or
physical timing. Separately, the user supplied a photographed real v8 readiness
PASS on October 11, 2026; see
[the v8 hardware record](../../experiments/cartridge-probe/JOYBOOT_READY_V8.md).
The physical SD baseline remains v7's photographed read PASS followed by three
successful cold starts. Physical v8 readiness has one recorded PASS; payload
upload and interrupted-session reliability still await separate validation.
