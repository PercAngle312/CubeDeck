/* Exercise the actual experimental adapter against a deterministic SI double.
 * This verifies software ownership/scheduling, not electrical hardware timing. */
#define CUBEDECK_STARTUP_HOST_TEST
#include "../experiments/v2-probe/gamecube-startup.c"
#include <assert.h>
static unsigned int reject, immediate, submissions;
static void (*pending_callback)(s32, u32);
static cdsr_peer fake_peer;
static u8 *pending_response;

static void respond(uint32_t word, uint32_t result)
{
    assert(pending_callback && pending_response);
    for (unsigned int i = 0; i < 4u; ++i)
        pending_response[i] = (u8)(word >> (i * 8u));
    void (*cb)(s32, u32) = pending_callback;
    pending_callback = NULL;
    cb(PORT, result);
}
u32 SI_Transfer(s32 channel, void *out, u32 out_length, void *in,
                u32 in_length, void (*callback)(s32, u32), u32 delay)
{
    (void)in_length; (void)delay;
    assert(channel == PORT && !pending_callback);
    ++submissions;
    if (reject) return 0u;
    u8 *bytes = out;
    if (bytes[0] == 0xFF) cdsr_reset(&fake_peer);
    if (out_length == 5u) {
        uint32_t word = (uint32_t)bytes[1] | ((uint32_t)bytes[2] << 8) |
                        ((uint32_t)bytes[3] << 16) | ((uint32_t)bytes[4] << 24);
        cdsr_receive(&fake_peer, word);
    }
    pending_response = in;
    pending_callback = callback;
    if (immediate) respond(fake_peer.reply, 0u);
    return 1u;
}
static void fresh(void)
{
    cdjr_init(&link, WATCHDOG);
    cdjr_configure(&link, SETTLE, SYNC_ATTEMPTS);
    tick = owned = mailbox_done = 0u;
    data_stage = running = passed = interrupted = data_error = 0u;
    target = 1u;
    reject = immediate = submissions = 0u;
    pending_callback = NULL;
    memset(&fake_peer, 0, sizeof(fake_peer));
    assert(begin_session());
}
static void step(uint32_t now)
{
    tick = now;
    service();
}
int main(void)
{
    fresh();
    reject = 1u;
    step(0u);
    assert(link.error == CDJR_ERR_SUBMIT && !owned && !link.inflight);
    reject = 0u;
    assert(begin_session());
    step(1u);
    assert(owned && link.inflight);
    stop_session();
    assert(!begin_session() && owned);
    completed(PORT + 1, 0u); /* wrong channel */
    assert(!mailbox_done);
    tick = 2u;
    respond(CDSR_READY, 0u);
    step(2u);
    assert(!owned && !link.inflight && link.error == CDJR_ERR_INTERRUPTED);
    assert(submissions == 2u); /* no command chained from interrupted callback */
    assert(begin_session());
    step(3u);
    step(123u);
    assert(link.error == CDJR_ERR_TIMEOUT && owned && link.inflight);
    tick = 124u;
    respond(CDSR_READY, 0u);
    step(124u);
    assert(!owned && !begin_session() && link.error == CDJR_ERR_TIMEOUT);

    fresh();
    immediate = 1u; /* callback may occur inside SI_Transfer */
    target = 16u;
    for (uint32_t now = 0u; now < 1000u && passed < target; ++now) step(now);
    assert(passed == 16u && !owned && link.phase == CDJR_READY);
    assert(submissions == 16u * 5u && fake_peer.resets == 16u);

    fresh();
    immediate = 1u;
    for (uint32_t now = 0u; now < 50u; ++now) {
        step(now);
        if (owned_kind == 5u && mailbox_done) {
            /* Leave during the data-send callback; main loop must drain
             * without issuing READ or silently beginning a new session. */
            stop_session();
            step(now + 1u);
            assert(!owned && !data_error && submissions == 4u);
            assert(begin_session());
            break;
        }
    }
    assert(link.phase == CDJR_RESET);

    fresh();
    immediate = 1u;
    for (uint32_t now = 0u; now < 50u; ++now) {
        step(now);
        if (link.phase == CDJR_READY && owned_kind == 5u) {
            /* Discard only in this fake transport to simulate a lost IRQ. */
            mailbox_done = 0u;
            immediate = 0u;
            step(now + WATCHDOG);
            assert(data_error == 1u && owned && !begin_session());
            break;
        }
    }
    assert(data_error == 1u);

    fresh();
    immediate = 1u;
    for (uint32_t now = 0u; now < 50u; ++now) {
        step(now);
        if (link.phase == CDJR_READY && owned_kind == 5u) {
            mailbox_done = 0u;
            immediate = 0u;
            stop_session();
            /* A late completion must latch the data watchdog even when
             * the interruption happened before the owner polled expiry. */
            tick = now + WATCHDOG;
            completed(PORT, 0u);
            service();
            assert(!owned && data_error == 1u && !begin_session());
            break;
        }
    }
    assert(data_error == 1u);
    puts("Startup SI adapter: rejection, interruption, watchdog and 16 fresh sessions passed.");
    return 0;
}
