# 1 KiB RAM fixture: Dolphin execution record

**Emulator results only; physical hardware validation remains pending.**
The original hardware-verified ACK/NACK milestone is unchanged. No CubeDeck
source or binary modification was needed for these emulator checks.

## Exact pair

Both unmodified artifacts came from [Actions run 38100813295](https://github.com/PercAngle312/CubeDeck/actions/runs/38100813295),
source commit `d06805692342f0944664d9705a986104a65655fe`; all 49 CI jobs passed.

| Artifact | File | SHA-256 |
| --- | --- | --- |
| cubedeck-v2-bulk-gamecube | cubedeck-v2-bulk.dol | `98a369498cbdd7cad6c28da1bb3f18f6aebdc2998117ef4fef5b6063fd892328` |
| cubedeck-v2-bulk-gba | cubedeck-v2-bulk.gba | `8b3a02adf540fcbe5fa2aa59ed2279b2d62e6ee14464b886dcb160bd6da08bbd` |

The [qualified Dolphin 2407 / integrated mGBA environment](EMULATOR.md#emulator-environment-and-qualifications)
was reused with a fresh isolated bulk profile, this matching pair and the
locally loaded user-supplied BIOS. The local evdev monitor guard and disabled
host USB panic dialogs apply here too. CPU, SI and Joybus emulation were not
modified. Neither BIOS contents nor emulator save containers are committed.
Both memory-card slots and Serial Port 1 were disabled. These checks used
synthetic RAM data only, without any cartridge or GameCube storage backend.

## Observed results

Pipe controller input drove A, Y and B; screenshots of the GameCube display
and integrated GBA were inspected for counters, errors, CRC and color.

| Scenario | Result |
| --- | --- |
| A: one image | Sessions 1/1; chunks 16/16; bytes 1024/1024; accepted CRC `6E99AE3A`; Retries 1; cyan GBA |
| Y: four fresh sessions in the same process | Sessions 4/4; final chunks 16/16; bytes 1024/1024; accepted CRC `6E99AE3A`; Retries 1; cyan; session ID advanced to 5 after the preceding A run |
| B at four early startup/packet timings, then A | Each interruption drained to Owned SI 0, Interrupted 1, startup error 7 and data error 0; each fresh A completed the full image with correct CRC, one retry and cyan |
| B after a partially accepted image, then A | Interrupted at 1/16 chunks, 64/1024 bytes and one retry; Owned SI 0 after draining, no accepted whole-image CRC; fresh A completed 16/16, CRC `6E99AE3A`, one retry, cyan |
| Reset integrated GBA during a partially accepted image | Before reset: 1/16 chunks, 64/1024 bytes, one retry, Owned SI 1; afterward: SI result `0x8`, data error 2, Owned SI 0, accepted whole-image CRC 0, blue GBA |
| A after the reset-induced SI error | Terminal error and partial-image counters stayed unchanged; no transfer restart |

Completed images showed startup/data errors 0, Ready reads 1/8 and Owned SI 0.
Partial images never displayed an accepted whole-image CRC or cyan success.
The reset used Dolphin's integrated-GBA reset hotkey, not a simulated cable
disconnect. The terminal SI error requires relaunch, as the current design
documents; this is not automatic recovery from a rebooted peer.

For the four early interruptions, A and B were held for 50 ms; delays after
releasing A were 0, 50, 300 and 1000 ms. Drained states were captured after
600 ms, and each restarted image was allowed 35 seconds. The last two
interrupted replies were word echoes with Ready reads 1/8 and no accepted
chunks. These host delays are scheduling inputs, not hardware timing
measurements. The four-session loop was captured after 160 seconds. The later partial-image
interruption used a five-second delay after A and a 35-second completion
check after restarting. This check ran in a fresh emulator launch after the
reset-induced terminal failure.

The drained screenshots do not alone establish pending-buffer preservation:
that invariant is checked by the 6,004 portable SI adapter fault sites.
These emulator checks do not force missing callbacks, hung SI operations,
electrical faults, session-ID exhaustion or a wrong bulk companion.

## Evidence

- [Four fresh sessions](evidence/dolphin-bulk-four.png)
- [Early packet interruption](evidence/dolphin-bulk-interrupted.png)
- [Fresh image after interruption](evidence/dolphin-bulk-resumed.png)
- [Partial image interrupted](evidence/dolphin-bulk-partial-interrupted.png)
- [Fresh complete image after partial interruption](evidence/dolphin-bulk-partial-resumed.png)
- [GBA reset with partial image](evidence/dolphin-bulk-reset.png)
- [A remains stopped after SI error](evidence/dolphin-bulk-reset-stopped.png)

The [portable matrix and build targets](BULK.md) remain separate from these
execution observations. None of these results validates physical console
startup timing, the link cable, or the user-reported intermittent startup.
