# CubeDeck v11 — experimental read-only cartridge header

This is a separately named GameCube/GBA runtime pair. CI and host simulations
validate software behavior; this build has **not passed a physical cartridge
test**. Keep the verified v9 cyan/CDR9 build and the separate v10 package.

## Required test order

First obtain three successful **v11 RAM PREFLIGHT** PING/ACK cold starts with
**no cartridge**, using `cubedeck-ram-preflight-v11.dol` and the included v11
GBA template. Press GC A once per cold start. This DOL never sends BEGIN and
the GBA PING handler never calls the cartridge reader.
Restart both systems between runs; record PING stage, SI error and purple GBA
screen. Do not advance to v11 cartridge testing if any RAM preflight run fails.
The v11 ZIP is prepared in advance; its presence does not establish that gate.

## SD layout

- `sd:/cubedeck-ram-preflight-v11.dol` (NO cartridge; purple GBA + GC PING PASS)
- `sd:/cubedeck-ram-header-v11.dol` (retail cartridge; header test)
- `sd:/cubedeck/ram-header-v11.gba`
- Your existing `sd:/cubedeck/gba-bios.bin` (16 KiB, CRC32 `81977335`)

Merge the ZIP into the SD card without deleting earlier builds or your BIOS.
No BIOS, Nintendo logo, game ROM, or save is bundled. The template's logo
region is zero; the GameCube patches only RAM from your own validated BIOS.
Do not launch the GBA template directly from a flashcart.

## One cartridge test, after the RAM preflight gate passes

1. Power both systems off. Keep the GCMCE disconnected as in earlier tests.
2. Put a physical GBA retail cartridge in the GBA **before power-on**. Do not
   insert, remove, or swap a cartridge while either test is running.
3. Connect the GBA link cable to GameCube controller port 2; controller in port 1.
4. Hold GBA START + SELECT during power-on to stay in the BIOS multiboot
   receiver instead of starting the cartridge. Keep held through startup.
5. Launch `cubedeck-ram-header-v11.dol` from Swiss on SD2SP2. Press GameCube A
   once to authorize RAM upload and the bounded 192-byte header scan.
6. GBA amber means waiting/in progress; green means all 192 bytes were sent.
   **Only the GameCube PASS with title/code/maker/version and a valid checksum
   and fixed header byte counts as a successful cartridge identification.**
   Green alone does not validate the cartridge header. Red indicates a
   terminal GBA protocol error; the GameCube separately reports transport or
   header-validation failures.
7. Photograph the GameCube result (including build ID, error and SI result).
   Restart both systems after any terminal failure. No in-session retry.

The GBA reads only ROM offsets 0–191 using aligned halfword loads. It never
reads save address space, writes cartridge memory, dumps a ROM or starts an
emulator. GameCube SD use is read-only. Header validation is a format check,
not proof of cartridge authenticity or ROM capacity.

## Implementation and validation

The existing v10 uploader is reused unchanged for BIOS transport. The separate
v11 preflight driver fixes application WRITE byte order: JOY_RECV receives
low byte first, separately from BIOS upload key encoding. The historical v10
runtime used high-byte-first and remains unchanged for comparison; use this
corrected preflight when recording the three required RAM-only passes. Its READY
marker starts the runtime phase; only a v11-specific `D5` BEGIN acknowledgment
establishes the v11 session. BEGIN binds an 8-bit nonce, then 192 `D1` requests
carry nonce-plus-offset and offset; `D2` replies must match both. A single boot
allows one scan. Duplicate last requests return cached replies without another
ROM read. Wrong order, wrong session, invalid offset and read failures fail
closed; a RESET after BEGIN aborts the live session.

The GameCube queues one SI operation at a time, waits for its callback, and
keeps static DMA buffers alive after timeout. It never retries an accepted
WRITE. Stale READ replies have a bounded request deadline, and the complete
scan has a separate deadline. Parsed fields stay hidden until all bytes pass
the fixed `0x96` byte and header checksum checks.

Host tests also prove PING never calls the cartridge reader and RESET after
BEGIN aborts the session. Host tests cover all 193 BEGIN/header WRITE stalls and callback failures,
all 192 cartridge-read failure points and stale-byte replies, duplicates,
wrong nonce/order, empty headers, bad checksum/fixed byte and frame wraparound.
CI cross-compiles the GameCube and RAM-resident GBA programs, validates DOL
sections/entry point and logo-free GBA identity/size/checksum, and publishes a
ZIP with source/run metadata and SHA256 sums. Physical timing remains untested.

Multiboot reference: [GBATEK](https://dswifi.akkit.org/info/gbatek.htm),
“BIOS Multi Boot (Single Game Pak)” (START + SELECT at power-on with cart).

Joybus register/byte-order reference: [GBATEK SIO JOY BUS mode](https://problemkaputt.de/gbatek-sio-joy-bus-mode.htm).
