# SD2SP2 Diagnostic v5 — focused storage milestone

Branch `experiment/cartridge-header-readonly`, draft PR #3. v4 hardware result:
driver startup and insertion passed; mount failed with CubeDeck code **-22**.
That code is not libfat's errno. Swiss reading FAT32 does not establish that
CubeDeck/libogc can read its sectors or select the same partition.

## Review and changes

Upstream references inspected:
- [libfat mount implementation](https://github.com/devkitPro/libfat/blob/master/source/libfat.c)
- [libfat partition discovery/FSInfo](https://github.com/devkitPro/libfat/blob/master/source/partition.c)
- [libogc SD2SP2 wrapper](https://github.com/devkitPro/libogc/blob/master/libogc/gcsd.c)
- [libogc sector reader](https://github.com/devkitPro/libogc/blob/master/libogc/sdgecko_io.c)

`__io_gcsd2` correctly addresses drive 2. Startup/insertion do not test sector
reads. `fatMountSimple` selects start sector 0 for autodiscovery. Upstream
partition discovery returns early on unreadable entries, before checking later
partitions. It also identifies FAT boot sectors through the advisory FAT text
field. **Neither condition is yet confirmed on the user's card.**

v5 reads sector 0, validates a 512-byte FAT BPB or the four primary MBR entries,
skips unreadable/invalid candidates, and mounts validated primary FAT volumes
with an explicit LBA. It does not scan arbitrary sectors, change card formatting,
or reinterpret GPT/exFAT/extended partitions. Those layouts remain unsupported
by this focused selector. A BPB-valid volume without the FAT text may still be
rejected by libfat; diagnostics report BPB and tag separately.

The persistent DISC_INTERFACE passed to libfat clears CANWRITE and replaces
writeSectors with a denying callback. This matters because libfat's FAT32 FSInfo
initialization can attempt writes even during mounting. No driver write is
forwarded, even if a library implementation attempts one. Console output counts
blocked requests. No memory-card, cartridge save, or GameCube storage writes.

Diagnostics show sector-0/signature results, the retained raw driver failure
and LBA, each partition's LBA/read/BPB/FAT tag/mount attempt, successful mount LBA,
mount errno, read count, blocked writes and file open errno. errno may remain 0
on failure: libfat does not reliably set it for all mount failure paths.

The experimental uploader CI target is compiled with **CUBEDECK_SD_ONLY**:
pressing A cannot begin Joyboot or cartridge probing. Existing original Joybus,
ACK, multiframe and RAM targets are unchanged. This milestone stops after reading
`sd:/cubedeck/joyboot-probe.gba` and validating its image size/header plan.

## SD-ready package

Artifact `cubedeck-sd2sp2-v5-sd-ready` contains the versioned inner ZIP:

- `cubedeck-sd2sp2-v5.dol`
- `cubedeck/joyboot-probe.gba`
- `README.txt` with exact commit and CI run
- `SHA256SUMS`

The probe file is the built homebrew companion with its Nintendo logo bytes
removed in this storage-only package. It is a file-loading fixture, **not a
BIOS-bootable multiboot payload**. No proprietary logo, BIOS or commercial ROM
is included in this package. No embedding is used.

Copy these contents to SD root and launch the distinct v5 DOL with Swiss. A
`PASS: probe file read and image checks passed` is the target result. Otherwise
report the complete displayed diagnostics. No need to change or reformat the card.

## Automated validation and limits

Host tests exercise the actual mount adapter with simulated libogc/libfat calls:
primary FAT32, superfloppy, an unreadable earlier partition followed by valid FAT,
startup/insertion/sector failures, invalid signature/BPB, partition overflow,
unsupported type, failed mount and a forced library write attempt. ASan/UBSan CI
checks and the existing write API audit complement GameCube cross-compilation.
These tests do not emulate SD electrical timing or execute the real libfat
partition constructor. CI records installed libogc/libfat versions in build logs.

The v4 cause remains unconfirmed until these diagnostics run on the actual card.
Do not claim hardware success, GBA upload or cartridge acquisition from CI results.
