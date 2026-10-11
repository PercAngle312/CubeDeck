# SD2SP2 Diagnostic v7 — validated GPT FAT volume selection

## Evidence and diagnosed incompatibility

User photo IMG_1409.jpg from real hardware running v6 shows driver/card 1,
sector0/signature 1, P1 type **EE**, start LBA **1**, and an **EFI PART**
signature in sector 1. FAT32 is the filesystem; GPT is the surrounding partition
layout. The v6 screen established the GPT layout. The v7 result below subsequently
confirmed successful metadata validation and FAT volume selection at LBA 2048.

v5/v6 deliberately accepted only ordinary/hidden MBR FAT partitions and SFD.
They rejected the protective EE entry, never mounting the contained FAT volume.
This explains the v5/v6 selection failure. It does not mean Swiss failed to read
the card or that the probe path was incorrect.

The earlier v4 library path used fatMountSimple. Installed package versions are
libfat-ogc 2.1.0-4/libogc 3.1.0-1. The package contains libdvm 2.1.0 and
FatFs-mod 0.15.3. That version's `ffconf.h` sets **FF_LBA64=0** and `ff.c` places
GPT autodiscovery inside `#if FF_LBA64`. With this configuration, the protective
MBR is searched as an ordinary MBR instead of entering GPT discovery. This is a
source-supported explanation for v4's mount failure on the observed GPT layout;
the v7 result below confirms the explicit-volume fix mounts the actual card.

References:
- [devkitPro package recipe](https://github.com/devkitPro/pacman-packages/blob/master/libfat/PKGBUILD)
- [installed FatFs version configuration](https://github.com/devkitPro/fatfs-mod/blob/v0.15.3/source/ffconf.h)
- [FatFs volume discovery](https://github.com/devkitPro/fatfs-mod/blob/v0.15.3/source/ff.c)
- [libdvm FAT mount wrapper](https://github.com/devkitPro/libdvm/blob/v2.1.0/source/fat_wrappers.c)
- [UEFI GPT format](https://uefi.org/specs/UEFI/2.10/05_GUID_Partition_Table_Format.html)

## Focused implementation

`sd_gpt.c/.h` reads primary GPT metadata into fixed RAM buffers using single
512-byte sector reads. It validates the primary header CRC32, table CRC32,
header layout, usable/disk/metadata ranges, all populated partition ranges,
nonzero unique IDs, duplicate IDs and overlapping partitions before publishing
any candidate. Bounds are explicit: at most 128 entries of 128 bytes, primary
header only, and addresses representable by the driver's 32-bit LBA API.
Unsupported geometry or a damaged primary table fails closed; no backup repair
or arbitrary-sector scan occurs. The bounded workspace is about 17 KiB in RAM.

Microsoft Basic Data entries are candidates. EFI System and other partition
types are not selected. Each candidate must contain a valid supported FAT BPB
within its extent. The adapter passes its explicit starting LBA to `fatMount`,
so the library sees that FAT volume's boot sector rather than protective MBR.
The same persistent read-only interface and denying write callback remain.

v7 shows GPT error, candidate count, actual GPT entry number, LBA, extent, BPB
result, mount attempt and successful entry. Only the first four candidates are
shown, but up to 128 can be considered. Error -26 means GPT validation failed;
GPT errors: -1 I/O, -2 header/CRC, -3 unsupported geometry/bounds, -4 table CRC,
-5 malformed/overlapping entries. -22 after GPT error 0 means no FAT candidate
mounted; -23 means mounting succeeded but the expected file could not open.

GBA upload and cartridge probing remain disabled in the packaged storage-only
DOL. No SD, memory-card, cartridge ROM/save or GCMCE writes. Original verified
Joybus and ACK/NACK builds remain untouched.

## Automated checks

The fixture is generated independently with Python struct/uuid/binascii:
header CRC **995FD933**, partition array CRC **49202D30**. It contains a synthetic
EFI partition followed by a Microsoft Basic Data FAT32 volume at LBA 4096.
Host tests exercise the real parser and adapter with simulated hardware/mount
calls, including GUID byte order and correct explicit data-partition offset.

ASan/UBSan checks cover valid selection, each of 33 metadata reads failing,
header/table corruption, unsupported dimensions, arithmetic bounds, invalid
ranges, overlapping entries, duplicate/zero IDs, no data candidates and atomic
empty outputs on failure. Existing MBR/SFD/hidden FAT and write-denial cases
remain. Tests do not replace physical SD reads or execute real libdvm mounting.

## SD-ready package

Artifact `cubedeck-sd2sp2-v7-sd-ready` contains versioned ZIP with:
`cubedeck-sd2sp2-v7.dol`, `cubedeck/joyboot-probe.gba`, README and checksums.
The 3200-byte homebrew fixture has its Nintendo logo removed. It is for storage
loading, not BIOS execution. No BIOS or retail ROM assets are bundled.

## Hardware result — October 11, 2026

The user supplied IMG_1411.jpg showing v7 running on the real GameCube with
Swiss and the SD2SP2 FAT32 card. The displayed result was:

- Driver startup **1**, card detected **1**, FAT mount **1**, errno **0**.
- GPT sector read **1**, EFI signature **1**, GPT validation error **0**.
- One data candidate; GPT entry **1**, start LBA **2048**, BPB **1**, mount **1**.
- Source `sd:/cubedeck/joyboot-probe.gba`, image bytes **3200**, error **0**.
- **PASS: probe file read and image checks passed.**
- **Storage-only v7: upload disabled.**

After the photographed pass, the user performed the requested three cold starts
with the same installed v7 files and reported **all 3 passed** on October 11,
2026. These are three additional successful runs, for four reported passes
including the photographed run.

This confirms SD2SP2 mounting, file loading and image checks on the user's actual
card, including repeatability across those three cold starts. It does not yet
validate interrupted sessions or other cards/partition layouts. GBA upload, BIOS
execution and cartridge access were not tested by these storage-only results.

The tested v7 package was produced from source commit
[`cdd13b0c75399014af20314e63cfd4a9998817ce`](https://github.com/PercAngle312/CubeDeck/commit/cdd13b0c75399014af20314e63cfd4a9998817ce),
with [CI run 38162766596](https://github.com/PercAngle312/CubeDeck/actions/runs/38162766596)
passing **76/76 jobs**. This result belongs to that installed package, not a
subsequent documentation build. Preserve its binaries and ZIP
`cubedeck-sd2sp2-v7-cdd13b0c7539.zip`.

Package file SHA-256 values (verified from the distributed ZIP, not on-console):

| File | SHA-256 |
| --- | --- |
| `cubedeck-sd2sp2-v7.dol` | `2481b64a87f6d5e37af308acdc208405f75269a60f436c1a33028f01cf209f46` |
| `cubedeck/joyboot-probe.gba` | `868572ea88313743b157253bb1c4fcad6d245a3fd1c5e61c0568b7bb051fa168` |

The focused three-cold-start check is complete. Preserve this tested v7 package
as the storage baseline before developing a separately opt-in upload experiment.
No replacement download is required to retain that baseline. Earlier verified
builds remain intact.
