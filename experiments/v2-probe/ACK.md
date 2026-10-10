# Experimental packet-level ACK/NACK Joybus handshake

**Opt-in; not yet hardware tested.** This is a separate test from the
user-verified three-frame cyan Joybus experiment.

The GBA replies with a per-word XOR echo for each seven-word v2 frame, as
before. After the last word, the GameCube waits 12 video frames and sends a
reserved query word for the frame sequence. The GBA serial IRQ responds with
the result previously calculated by the GBA main loop after a full CDTP CRC,
transfer-ID, sequence, opcode and payload verification.

The GameCube's subsequent READ retrieves that sequence-bound **ACK** or
**NACK**. It only proceeds on an ACK matching the current sequence. A NACK
or pending status triggers a resend of the exact current seven-word sample,
with at most two retries. Unrecognized verdicts abort the experiment.

The GBA intentionally rejects the **first attempt of packet 2** after
checking it, to exercise physical retransmission without damaging the sample
or touching ROM/save memory. On the second attempt it accepts packet 2.
The intended success result is **cyan** on the GBA and
`V2 ACK PASS: 3/3 verified` on the GameCube, with a retry count of one.

These control words are **diagnostic only**. They aren't the final framed
ACK/NACK transport and don't negotiate fresh transfer IDs. The control
query uses `0x7AD0xxxx`, ACK `0xACCDxxxx`, NACK `0xBAD0xxxx` with the
current 16-bit sequence in the low bits. The physical test is still
synthetic and uses a fixed ID. No cartridge ROM, SRAM, flashcart saves,
filesystem storage, memory card or Game Boy Advance emulator operations.

From one successful GitHub Actions run, download *both*:
- `cubedeck-v2-ack-gamecube` containing `cubedeck-v2-ack.dol`
- `cubedeck-v2-ack-gba` containing `cubedeck-v2-ack.gba`

Keep the user-verified multiframe files. Use a link cable on GameCube
Port 2 and disconnect GCMCE for this isolated test. Launch GBA ROM first,
then DOL through Swiss, enter link check and press A.

If the test fails, note the GameCube's displayed last response and the GBA
color. Don't reuse the stable probe as the partner to this ACK/NACK build.

## RESET stall correction (2026-10-10)

The first ACK/NACK experimental GameCube build could stay indefinitely on
`Waiting for RESET`. The `joybus_stage == 1` callback handler had
accidentally become an empty branch when the ACK query state machine was
added. The RESET callback was consumed without starting the subsequent
`SI_Transfer(1, 0x00, ..., 3)` STATUS command. This was an application
state-machine regression, not evidence that the link cable or GBA failed.

Corrected in the experimental `gamecube-ack.c` only: stage 1 now
sets stage 2 and issues the 3-byte STATUS query. A CI source regression guard
also checks the callback edge. The known-good single-frame and three-frame
cyan-validated binaries were never modified. Users must download a *new*
`cubedeck-v2-ack-gamecube` artifact from a successful build after this
correction: the previously downloaded DOL will remain defective.
