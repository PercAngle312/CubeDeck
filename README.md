# CubeDeck — GameCube homebrew starter

CubeDeck is a Nintendo GameCube homebrew dashboard with an early GBA link-cable development path. Cloud builds mean you do **not** need to install a compiler on your PC.

## Cloud builds

Open the repository's **Actions** tab and run **Build CubeDeck**, or let a push trigger it. The workflow publishes two artifacts when both jobs succeed:

- `cubedeck-gamecube`: GameCube `.dol` and `.elf`
- `cubedeck-gba-diagnostic`: standalone GBA diagnostic `.gba` and `.elf`

The GBA diagnostic is intended to run from your SuperCard/flashcart. CubeDeck does not yet upload it over the link cable. If a build fails, use the workflow log as the source of truth; do not test a missing or incomplete artifact.

## Current GameCube features

- Text console UI and GameCube controller navigation
- System information and About pages
- Four-port GameCube controller monitor
- GBA link cable detection on GameCube Port 2 using asynchronous SI device-type polling
- Callback-driven Joybus RESET/STATUS and two-way diagnostic word exchange
- Framed-packet word-by-word smoke test (A), one-round verification (X), and a 16-round sequential-word stress test (Y)

**Physical-hardware validation:** the maintainer confirmed **16/16 replies valid** in the sequential-word stress test and **6/6 packet words acknowledged** with the GBA screen turning **cyan** after decoding the complete sample packet, checking CRC32 and all seven payload bytes. The reported CRC word was `0x5DD2BACF` and the final XOR reply `0xF888860C`. This verifies a sample packet end-to-end on real hardware, **not** bulk ROM transfer or TV playback.

## GBA diagnostic payload

The separate `gba-diagnostic/` target is an experimental GBA-side test ROM. It switches the GBA link port into Joybus mode, prepares a deterministic XOR reply, and changes its screen color when it receives data. The latest diagnostic also captures, decodes and CRC-verifies a framed sample packet, using cyan to indicate the complete sample matched. It does not read or write cartridge ROM or save memory.

## Current host-side transfer development

The shared C protocol modules now support packet CRC32, sequence checking, idempotent retries, manifest-first sessions, and complete-image checksums. A newer **RAM-light callback-based sender and receiver** are tested in CI with a **131,077-byte** simulated transfer, including corrupted frames, dropped ACKs, read failures, sink failures, and stale acknowledgments. Streaming ACKs identify both packet opcode and sequence to distinguish MANIFEST from DATA(0).

**These large-transfer capabilities are software simulations only.** They are **not wired into the GameCube or GBA runtime**, and no ROM playback or cartridge dumping is implemented. A source-code guard rejects selected direct storage-write APIs; this cannot guarantee Swiss or firmware never writes storage.

## Hardware validation

1. Confirm the Actions workflow succeeds and both artifacts are present.
2. Keep a copy of the last known-good `.dol` and `.gba`.
3. Launch the GameCube artifact from Swiss and run the GBA diagnostic from the flashcart.
4. Open the GBA link page and press A for the framed-packet word smoke test, X for one round, or Y for the 16-round stress test.
5. The A test checks the reply to each encoded word, while the current GBA diagnostic additionally decodes and CRC-verifies the complete sample packet; cyan confirms matching payload bytes. Keep the GCMCE disconnected during experimental hardware testing pending investigation of an earlier unrelated/uncertain memory-card corruption incident.

## Roadmap

1. Portable packet framing v1 is host-tested and now used by the GameCube A-button smoke test. The GBA diagnostic now both acknowledges each word with XOR and passively parses the complete sample packet; this path passed the physical-hardware cyan-screen check.
2. The GBA-side seven-byte packet parser is hardware-verified. Next integrate bounded chunk framing, timeouts, and opcode-bound ACK handling into the Joybus runtimes before requesting another hardware test.
3. Evaluate established GBA Link Cable Dumper code/workflow for reading retail cartridges; keep SuperCard SD-file access as a separate question.
4. Evaluate **Visual Boy Advance GX**, an existing GameCube GBA emulator, as the playback backend; its current releases already support GBA emulation on GameCube. See [docs/emulator-backend.md](docs/emulator-backend.md) for the integration sequence. CubeDeck does not yet launch it.
5. Investigate save access separately, starting read-only and protecting existing saves.

CubeDeck does not modify IPL, system settings, cartridge ROM, or save data. See [docs/gba-link-transport.md](docs/gba-link-transport.md) for transport details and validation rules, and [docs/rom-acquisition-plan.md](docs/rom-acquisition-plan.md) for the separate, read-only retail-cartridge multiboot roadmap.
