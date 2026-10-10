# GameCube GBA playback backend — use an existing emulator

**Discovery (October 2026):** The upstream open-source **Visual Boy Advance
GX** already supports Game Boy Advance emulation **on real GameCube hardware**.
CubeDeck must not spend its early milestones writing a new GBA CPU, PPU and APU
from scratch when an existing GameCube backend is available.

- Main project: https://github.com/dborth/vbagx
- Source build target: `make -f Makefile.gc -j3`, producing
  `executables/vbagx-gc.dol` (per upstream README).
- Current upstream release notes: https://github.com/dborth/vbagx/blob/master/CHANGELOG.md
  The 3.0.1 update added GameCube/Wii GBA dynamic recompilation and a GameCube
  ROM paging mechanism using ARAM plus SD storage. The 3.0.2 update includes
  stability fixes. Do not pin to an unspecified branch tip in CubeDeck CI.
- License: upstream is **GPL**; review dependency and redistribution terms
  before vendoring code, linking cores, or redistributing modified binaries.

## Product integration sequence

### Phase A — existing working binary, separate app

Use the upstream project's official GameCube build independently of CubeDeck.
Launching an existing emulator from Swiss immediately validates that GameCube
GBA playback is feasible **without a Game Boy Player**. This is not a new
CubeDeck hardware test request. Nothing in the CubeDeck diagnostic or GCMCE
needs changing. Follow the upstream `vbagx/roms` layout and keep existing
saves backed up; the emulator's normal save features are a separate concern
from our experimental cartridge-dumping path.

### Phase B — read-only cartridge acquisition

Implement a read-only GBA multiboot payload (see
`docs/rom-acquisition-plan.md`) and receive its output via the physical link
cable. First prove the cartridge header and a small bounded ROM range.
Transfer and verify complete ROM bytes without touching cartridge save areas
or any attached memory-card emulator. Capture a whole-image CRC32 and precise
expected size before considering persistence.

### Phase C — opt-in ROM handoff

Only after hardware acquisition is reliable, consider writing a verified ROM
to a new user-approved SD destination. Validate file length and final CRC,
then launch a **separate** emulator application through an established
GameCube homebrew loader. Avoid embedding or modifying VBA GX initially; this
keeps the CubeDeck link/dumper app independent from the emulator.

### Phase D — unified CubeDeck interface

Investigate integrating the VBA GX emulation core and video/audio/controller
drivers as licensed source into a unified frontend. This is substantially more
complex than launching a separate DOL: it requires GameCube memory layout,
ROM paging, startup/teardown, input, storage, and framebuffer ownership to
all be reconciled. Build a distinct experimental target before changing the
hardware-tested CubeDeck DOL. Respect GPL licensing for redistributed
derivatives.

## Critical architectural constraints

- There is only one physical GBA cartridge slot: running a diagnostic from
  the SuperCard/flashcart is not equivalent to having the retail cartridge
  available for concurrent reading. Multiboot is the planned direction.
- A GBA link cable can supply cartridge bytes; it does **not** remotely
  execute a GBA title on the GameCube by itself. The emulator handles that.
- 32 MiB ROMs exceed the present single-session protocol capacity of
  16,776,960 bytes. Our host-only segment planner safely partitions the
  length into three windows but **does not yet bind and authenticate session
  identities across windows**.
- This project previously experienced unexplained memory-card emulator
  corruption. Do not write new storage backends in the first read-only tests.
- Current CI establishes C host-test behavior and DOL compilation, not
  real-hardware compatibility of newly proposed integrations.
