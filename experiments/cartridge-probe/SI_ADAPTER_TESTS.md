# Actual SI adapter ownership checks

`tests/test_joyboot_si_adapter.c` compiles the actual
`experiments/cartridge-probe/joyboot_si_adapter.c`. The test substitutes only
the libogc `SI_Transfer`, `IRQ_Disable` and `IRQ_Restore` entrypoints. This
complements controller tests that substitute the entire adapter; it does not
execute real SI hardware or all of libogc.

Checks cover:

- Invalid inputs are rejected before any SDK call.
- 32-byte aligned buffers, owned command copy, zero padding and response clearing.
- Synchronous refusal releases the never-accepted owner for another adapter.
- Pending transfers and completed-but-undrained mailboxes block all other owners.
- Wrong-channel callbacks are ignored; an already-completed mailbox preserves its
  first result until drained. With no owner, a callback has no effect.
- A completion at the IRQ restore boundary is visible to the caller.
- Caller IRQ masks remain intact through submit and take.
- Repeated take/submit attempts during a delayed callback never reuse either
  transfer buffer or advance its token. A delay is not cancellation.
- Little-endian replies and short replies cannot expose older response bytes.

## Callback contract and limits

libogc's callback has no user context or hardware-supplied application token.
The adapter therefore relies on libogc delivering one callback for each accepted
transfer. The reviewed SI interrupt handler takes the registered callback and
clears it before invocation; `SI_Transfer` may also accept a queued transfer that
has not started on hardware yet. Both accepted cases retain the caller's buffers.
Reference reviewed on October 11, 2026:
[libogc SI implementation](https://github.com/devkitPro/libogc/blob/e3764ac5688d423662e4e146af6b87da28499cde/libogc/si.c).

The adapter cannot distinguish a fabricated duplicate from an old transfer
after a new owner starts on the same channel. Tests do not claim otherwise.
Application-token mismatch tests belong to the controller fault matrix, while
SDK callback discipline is a separate dependency. No cancellation is exposed.
Timeout failures retain static buffers and require a DOL restart rather than
reinitializing the owner.

CI runs these direct-adapter checks with ASan/UBSan. The separate v9 package
depends on their success. No console sources, established binary targets or
physical validation status change as part of these tests.
