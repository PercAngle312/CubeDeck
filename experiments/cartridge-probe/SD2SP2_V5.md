# SD2SP2 Diagnostic v5 — focused storage milestone

Branch `experiment/cartridge-header-readonly`, draft PR #3. v4 hardware result:
driver startup and insertion passed; mount failed with CubeDeck code **-22**.
That code is not libfat's errno. Swiss reading FAT32 does not establish that
CubeDeck/libogc can read its sectors or select the same partition.

## Review and changes

CI reports **libfat-ogc 2.1.0-4 / libogc 3.1.0-1**. The package named
libfat is now libdvm 2.1.0 plus FatFs-mod 0.15.3. References inspected:
- [actual package recipe](https://github.com/devkitPro/pacman-packages/blob/master/libfat/PKGBUILD)
- [libdvm v2.1.0 FAT compatibility wrapper](https://github.com/devkitPro/libdvm/blob/v2.1.0/source/fat_wrappers.c)
- [libdvm FAT driver](https://github.com/devkitPro/libdvm/blob/v2.1.0/source/fat_driver.c)
- [libdvm cache](https://github.com/devkitPro/libdvm/blob/v2.1.0/source/dvm_cache.c)
- [FatFs-mod volume discovery](https://github.com/devkitPro/fatfs-mod/blob/v0.15.3/source/ff.c)
- [libogc SD2SP2 wrapper](https://github.com/devkitPro/libogc/blob/master/libogc/gcsd.c)
- [libogc sector reader](https://github.com/devkitPro/libogc/blob/master/libogc/sdgecko_io.c)

`__io_gcsd2` correctly addresses drive 2. Startup/insertion do not test sector
reads. The current compatibility wrapper passes start sector 0 to FatFs, whose
`find_volume` handles SFD and MBR discovery. Unlike archived libfat's partition
search, this current implementation can continue past an earlier read failure.
**The archived libfat early-abort behavior is not a demonstrated v4 cause.**

The current libdvm defaults are four cache pages of 64 sectors each. Cache
prefetch can turn an initial single-sector request into a larger driver read.
v5 uses eight pages of eight sectors (32 KiB total) and explicitly validated
volume offsets. This bounds page prefetch but is not proof of a timing fix.
The installed-library fingerprints are retained in CI build logs.

v5 first reads sector 0 directly, validates a 512-byte FAT BPB or the four
primary MBR entries, skips unreadable/invalid candidates, and mounts validated
primary FAT volumes with an explicit LBA. This separates sector I/O failure
from BPB/selection/mount failure. It does not scan arbitrary sectors, change
card formatting, or reinterpret GPT/exFAT/extended partitions. Those layouts
remain unsupported by this focused selector, even where FatFs supports more.
FatFs FAT32 recognition still depends on its FAT32 text/jump checks; diagnostics
report BPB and FAT tag separately. **No failing stage below mount was captured
in v4, so the specific card's root cause remains unconfirmed.**

The persistent DISC_INTERFACE passed to libfat clears CANWRITE and replaces
writeSectors with a denying callback. This also protects against metadata writes in legacy libraries and future
accidental write requests; the current FatFs driver reports write protection. No driver write is
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
