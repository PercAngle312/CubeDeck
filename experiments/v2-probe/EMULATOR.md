# Recovery ACK fixture: Dolphin execution record

These are **emulator results, not physical hardware validation**. The
hardware-verified original ACK/NACK milestone and its provenance in ACK.md
remain unchanged. The recovery program still says UNVALIDATED because real
console timing, cable behavior and intermittent startup remain unverified.

## Exact tested pairs

Unmodified artifacts from [Actions run 38098089591](https://github.com/PercAngle312/CubeDeck/actions/runs/38098089591),
source commit `1f8f27de65c8b06491804afb31b5dee9e8f2e576` (46/46 CI jobs passed):

| Artifact | File | SHA-256 |
| --- | --- | --- |
| cubedeck-v2-recovery-gamecube | cubedeck-v2-recovery.dol | `18f84680e1d5c5feb3f02e55a30ca1495737a77e2f931c01b84c48db6be6916d` |
| cubedeck-v2-recovery-gba | cubedeck-v2-recovery.gba | `9e7cc88743816dc6495b89ff027f7e1c615353290e14c1d2aef92c571cb5d098` |
| cubedeck-v2-startup-gamecube | cubedeck-v2-startup.dol | `b7ce1cd77428232144fbfbc5224b5b2807338326b88b651e637807c7bb79c39f` |
| cubedeck-v2-startup-gba | cubedeck-v2-startup.gba | `9943414bef30d1c66b40a6758a05fe6db7aa329d6c0eeef82cd77aded7960a30` |

No CubeDeck code or binaries were changed for these runs. These results
precede the separate 1 KiB module; they do not validate its new pair.
That pair has its own [execution record](BULK_EMULATOR.md). The original
ACK, multiframe and single-frame targets were not executed in this record.

## Observed execution

Controller commands were delivered through Dolphin's pipe input backend;
results were checked in captured GameCube and integrated GBA screenshots.

| Scenario | Observed result |
| --- | --- |
| A: one fresh session | Sessions 1/1; Verified 3/3; Retries 1; GBA cyan |
| Y: 16 fresh sessions in the same process | Sessions 16/16; final Verified 3/3; final Retries 1; GBA cyan |
| B during early startup, then A | Interrupted=1, startup error 7, data error 0, owned SI 0 after draining; fresh session 3/3, Retries 1, cyan |
| B during later startup, then A | Same safe interruption and fresh-session result |
| B during packet words, then A | Interrupted=1, TX phase 2, data error 0, owned SI 0 after draining; fresh session 3/3, Retries 1, cyan |
| B while awaiting packet verdict, then A | Interrupted=1, TX phase 4, GBA green; fresh session 3/3, Retries 1, cyan |
| Startup-only matching pair | Completed 1/1, then 16/16; errors 0; GBA cyan |
| Recovery DOL with wrong startup-only GBA | Startup error 6 (not ready), ready reads 8/8, verified 0/3, no data error, no owned SI; no data transfer |
| Reset integrated GBA during packet words | SI result 0x8 (no response), data error 2, verified 0/3, owned SI 0; terminal stop |
| A after the reset-induced SI error | Counters and terminal error stayed unchanged; no unsafe automatic restart |
| Fresh emulator launch after that terminal error | Recovery pair returned to 1/1, 3/3, Retries 1, cyan |

All completed sessions displayed startup/data errors 0, ready reads 1/8,
and owned SI 0. Error 7 during B is the intended interrupted state, not a
watchdog or callback error. The four interruption runs used 50 ms A/B button
holds, with 0, 50, 300 and 1000 ms delays after releasing A before pressing B.
These are host scheduling delays, not precise measurements of SI timing.
After B, screenshots were taken after 600 ms; each fresh A session was
allowed nine seconds before checking its result. Session IDs advanced
through the four interrupted/restarted pairs to session 8.

Captured evidence:

- [16 completed sessions](evidence/dolphin-recovery-16.png)
- [Interrupted verdict wait](evidence/dolphin-recovery-interrupted.png)
- [Fresh session after interruption](evidence/dolphin-recovery-resumed.png)

These checks did not inject missing or erroneous SI callbacks, force an
outstanding transfer to remain hung, force a cable disconnect,
or exercise session-ID exhaustion. The GBA reset check used Dolphin's
normal integrated-GBA Reset hotkey while TX phase 2 held an SI operation.
It exercised an emulated GBA reboot, not an electrical cable fault. Portable fault-matrix tests cover those
software cases; physical timing and electrical faults still need hardware.
The drained screenshot does not by itself prove pending-buffer preservation;
that invariant is separately checked by the host SI adapter tests.

## Emulator environment and qualifications

- Ubuntu 24.04 x86-64; Dolphin tag **2407**, commit
  `b92e354389bb7c0bd114a8631b8af110d3cb3a14`.
- Bundled mGBA commit `8739b22fbc90fdf0b4f6612ef9c0520f0ba44a51`.
- GCC 13.3, Qt 6.4.2, Release build at `-O1`, integrated mGBA enabled.
- Software graphics, NoAudio, single CPU thread, GBA threads disabled,
  Xvfb display. No emulator savestates were used.
- User-supplied 16,384-byte GBA BIOS loaded locally. **BIOS contents are not
  committed, uploaded to GitHub, or included in artifacts.**
- A local Dolphin-only guard returns from the evdev hotplug thread when
  its host udev monitor cannot be opened. The restricted runner otherwise
  passed an invalid descriptor to FD_SET and aborted before emulation.
  The guard checks monitor creation, receiving enablement and descriptor
  range. It does not modify CPU, GBA, SI, or Joybus emulation.
- Dolphin's panic dialogs were disabled in this isolated profile because
  the runner cannot initialize host USB. The USB warning and a GLX
  SwapInterval warning were logged. Physical USB controllers were not used.
  This is a qualified local source build, not an unmodified Dolphin release.

## Paired setup

Use a fresh isolated Dolphin user directory, a legally dumped BIOS, and the
exact matching pair above. Port 1 is a standard emulated GameCube controller;
Port 2 is **GBA (Integrated)** with the recovery GBA file. Disable both
memory-card slots and Serial Port 1. Example Dolphin.ini keys for version 2407:

```ini
[Core]
SIDevice0 = 6
SIDevice1 = 13
SIDevice2 = 0
SIDevice3 = 0
SlotA = 255
SlotB = 255
SerialPort1 = 255
GFXBackend = Software
CPUThread = False
[DSP]
Backend = NoAudio
[GBA]
BIOS = /absolute/path/to/local/gba_bios.bin
Rom2 = /absolute/path/to/cubedeck-v2-recovery.gba
Threads = False
SavesInRomPath = False
```

Launch the DOL with `dolphin-emu -u /path/to/isolated/user -e /path/to/cubedeck-v2-recovery.dol -b`.
A runs one session, Y runs 16, B interrupts. A can restart after timely
callback draining; watchdog/data errors remain terminal as documented in
RECOVERY_ACK.md. Check both the GameCube counters and GBA color.

The test uses only the synthetic RAM fixture. No physical cartridge,
cartridge save, GameCube memory card, GCMCE, or GameCube storage is accessed.
Dolphin's own configuration/log files and its synthetic companion's local
emulator save container remain in the isolated host directory; no real game
save or storage image was supplied.
