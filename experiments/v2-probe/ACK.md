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

## Generic failure or timeout after RESET fix (2026-10-10)

The second hardware report was `Joybus transfer failed or timed out`.
That message alone does **not** identify whether RESET/STATUS/SEND/READ,
the new packet-verdict query, or the retry budget failed.

The GameCube ACK experiment now records and displays an explicit failure
snapshot, without touching the proven multiframe source or the GBA ROM:

- **Code 1**: watchdog expired while `SI_Transfer` was still pending;
  never immediately resubmit the transfer because SI has no cancellation
- **Code 2**: SI callback reported an error; the numeric SI result is shown
- **Code 3**: the GBA returned NACK/PENDING too many times (retry exhausted);
  compare the last reply and GBA screen color
- **Code 4**: the next synthetic packet could not be encoded

The screen also shows the last active **stage**, frame, word index, retry
count, SI callback result, and last verdict. Stage **1=RESET**,
**2=STATUS**, **3=SEND frame word**, **4=READ echo**, **8=wait for
decoder**, **9=SEND verdict query**, **10=READ verdict**.

This is a diagnostic revision, not a confirmed fix for the second
hardware failure. Use a **new** GameCube ACK artifact produced after the
diagnostics change; the GBA ACK image is unchanged. No cart/save writes.

## Physical restart / retry observation (2026-10-10)

The user explicitly clarified that **without copying any new files or changing
the binaries**, they fully restarted both the GameCube and the GBA, reran the
previously failing experimental ACK/NACK test, and reported **everything
passed** (including a preceding report of "3"). The success therefore
belongs to the **existing installed ACK/NACK pair**, not to a later
diagnostic build or any SD-card replacement. The user subsequently **explicitly confirmed 3/3 on GameCube and cyan on
GBA** for that same successful restart/retry run. This verifies three
packets completed the GBA's acceptance path in the ACK/NACK experiment.
The user subsequently **explicitly confirmed that the same GameCube success
screen showed `Retries: 1`**. Together with GBA cyan and 3/3, this
user-observed hardware result validates the diagnostic's intended
one-time packet-2 NACK and bounded GameCube retransmission on real consoles.
This is a single successful test after a restart, not proof of repeatable
cold-start reliability or reliable recovery from arbitrary dropped words.

The earlier failure might be a transient startup / GBA Joybus state or
timing problem, but the root cause has **not** been isolated. A cold restart
appeared to recover the setup on the same code. Preserve the known-good
three-frame cyan files and avoid asserting that diagnostic changes caused
this success. Future work should focus on robust startup synchronization,
sequential session reset and real-hardware repeatability.
