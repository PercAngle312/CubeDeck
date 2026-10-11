# 16 KiB verdict-wait: Dolphin execution record

**Emulator results only; physical hardware validation remains pending.**

## Exact pair

Unmodified artifacts from [Actions run 38103472549](https://github.com/PercAngle312/CubeDeck/actions/runs/38103472549),
source `8ed0570b700f0c2edce0272fbf3c3771af7f9e1d` (55/55 CI jobs passed):

| Artifact | File | SHA-256 |
| --- | --- | --- |
| cubedeck-v2-ram16-wait-gamecube | cubedeck-v2-ram16-wait.dol | `e3c777394b82bea3932e0c875d127c0a1e4be8c113522a6c6b7d9f3f47b2cfd1` |
| cubedeck-v2-ram16-wait-gba | cubedeck-v2-ram16-wait.gba | `e6d678090ccdc554788f82a7f386deb1448df49911558c31dacea025a4afe84c` |

The [qualified Dolphin 2407 / integrated mGBA build](EMULATOR.md#emulator-environment-and-qualifications)
was used with a fresh profile, locally loaded user-supplied BIOS, software
rendering, NoAudio, single CPU thread, GBA threads disabled and speed limit
disabled (`EmulationSpeed = 0.0`). No savestates were used. These are
functional checks, not throughput measurements. Memory cards and Serial
Port 1 were disabled; only synthetic RAM payloads were supplied. Neither
BIOS contents nor emulator save containers are committed.

## Matching pair: full-image acceptance

A started one image. The ninth 30-second completion poll captured:

- Sessions 1/1, session ID 1, Ready reads 1/8.
- Chunks **128/128**, bytes **16384/16384**.
- Accepted whole-image CRC **`E7638E98`**, GBA **cyan**.
- Startup/data errors 0, SI result 0, Owned SI 0.
- **Retries 1**, last retry chunk 2, cause 1 (NACK), reply `BA000102`.
- **Pending polls 2**; final reply `AC000180` (session 1, chunk 128 ACK).

[Full-image evidence](evidence/dolphin-verdict-wait-complete.png).

Two exact-session/chunk PENDING replies exercised the new waiting path
without adding a frame retransmission. The total counter does not identify
their chunk numbers or wall-clock timing. Successful acceptance retained
only the deliberate chunk-2 NACK retry. Host tests separately confirm that
queries preserve the worker epoch and that a never-ready peer fails within
its fixed wait/retry budgets.

The [older 16 KiB run](RAM16_EMULATOR.md) recorded two retries without cause
information. This result supports the new bounded-PENDING behavior; it does
not prove the cause of that earlier extra retry, explain the original
physical startup failure or validate real console interrupt latency.

## Wrong companion: no packet transfer

The same verdict-wait DOL was launched in a separate fresh profile with the
older `cubedeck-v2-ram16.gba` from source `ce91b96694e583f8780df1fc4a60f71b77661785`
and Actions run 38102277785. Its SHA-256 was
`23f9a6e44d31e7147837cc19ed51e29ebb99e548f4c02070406f67276dcbd748`.
After A and six seconds, the inspected screenshot showed startup error 6
(not ready), Ready reads 8/8, chunks 0/128, bytes 0/16384, CRC 0, Retries 0,
Pending polls 0, data error 0, Owned SI 0 and blue GBA. Last reply `43535236`
was the older companion signature, so the new `43565736` pair refused it
before packet data.

[Wrong-companion evidence](evidence/dolphin-verdict-wait-wrong.png).

The four-session loop and interruption/reboot emulator scenarios were not
run for this new pair. The [portable fault tests](RAM16_WAIT.md), including
14 delayed-query SI ownership sites, are separate evidence. Physical console
validation remains pending until the user has hardware available.
