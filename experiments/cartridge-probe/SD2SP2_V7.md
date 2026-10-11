# SD2SP2 Diagnostic v7 — validated GPT FAT volume selection

## Evidence and diagnosed incompatibility

User photo IMG_1409.jpg from real hardware running v6 shows driver/card 1,
sector0/signature 1, P1 type **EE**, start LBA **1**, and an **EFI PART**
signature in sector 1. FAT32 is the filesystem; GPT is the surrounding partition
layout. The presence of GPT is now hardware evidence, although CRC validity and
its actual FAT volume offset have not yet been checked on the user's card.

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
the fix still needs confirmation on the actual card.

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

## SD-ready package and next hardware check

Artifact `cubedeck-sd2sp2-v7-sd-ready` contains versioned ZIP with:
`cubedeck-sd2sp2-v7.dol`, `cubedeck/joyboot-probe.gba`, README and checksums.
The 3200-byte homebrew fixture has its Nintendo logo removed. It is for storage
loading, not BIOS execution. No BIOS or retail ROM assets are bundled.

Keep v6; copy package contents to SD root and launch the distinct v7 DOL in
Swiss. Target: GPT error 0, a data candidate with BPB 1, mount 1, image bytes
3200 and PASS. Capture the full screen if any step fails. No reformat required.
v7's successful real-hardware mount/read remains outstanding.
