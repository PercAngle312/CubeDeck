# SD2SP2 Diagnostic v6 — raw layout visibility

## User-reported real hardware result: v5

Photo IMG_1408.jpg shows:

- Driver 1, card 1, mount 0, open errno 0.
- Image bytes 0, state -1, CubeDeck error -22.
- Sector0 read 1, signature 1, retained raw error 0, failure flag 0.
- Reads 1, blocked writes 0, all four candidate flags 0.

This confirms a successful sector-0 read and 55 AA signature, but no volume was
accepted by v5's selector. No file read or Joyboot attempt occurred. The raw
partition types/offsets and sector-0 BPB were not captured, so this does not
prove GPT, an empty partition table, a malformed FAT filesystem, or a bad card.
The displayed zero LBAs were defaults: v5 only populated them for accepted
entries. They were not raw on-card evidence. No claim that v5 solved mounting.

## Focused changes

- Capture raw status/type/LBA/length for every MBR slot before validation.
- Display raw sector-0 jump byte, bytes per sector, sectors per cluster,
  reserved sectors, FAT count, root entries, FAT sizes, total sectors and root
  cluster, plus the SFD/BPB acceptance flag. On an MBR these BPB fields are
  boot-code bytes and must not be interpreted as the partition's BPB.
- If a protective 0xEE entry exists, read sector 1 and report its EFI PART
  signature. This is a diagnostic hint, **not full GPT validation or mounting**.
- Accept hidden primary FAT types (11/14/16/1B/1C/1E) with the same BPB and
  extent checks as ordinary FAT. No evidence yet that the user's card is hidden
  FAT; this closes a specific selector compatibility gap without changing data.
- Keep the persistent denying write callback, bounded cache, RAM-only buffers,
  external file loading and storage-only build with GBA upload disabled.

Host tests now preserve raw rejected entries, distinguish protective MBR with
and without an EFI signature, check sector-0 BPB diagnostics, and mount a
synthetic hidden FAT32 primary partition. Existing I/O/bounds/write-denial cases
remain. No arbitrary sector scan, formatting, writes or embedded fallback.

## Hardware test package

Artifact `cubedeck-sd2sp2-v6-sd-ready` contains a versioned ZIP with
`cubedeck-sd2sp2-v6.dol`, `cubedeck/joyboot-probe.gba`, README and SHA256SUMS.
The homebrew probe's logo bytes remain zeroed; this is a file-loading fixture,
not a BIOS-bootable payload. No BIOS, proprietary logo or retail ROM is bundled.

Copy contents to SD root, preserve the v5 DOL, launch the distinct v6 DOL via
Swiss, and capture the complete display. Target remains mount 1, image bytes
3200 and PASS. If it fails, the raw P1-P4 types/LBAs and S0/GPT fields determine
whether the next fix belongs in layout selection, BPB handling or sector I/O.

Reference [v5 implementation review](SD2SP2_V5.md) for the actual installed
libdvm/FatFs versions and limits. v6 has no real-hardware result yet.
