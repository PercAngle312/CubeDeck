#define CUBEDECK_STARTUP_HOST_TEST
#include "../experiments/v2-probe/gamecube-recovery.c"
#include <assert.h>

static cdrp_peer fake_peer;
static void (*pending_callback)(s32, u32);
static u8 *pending_response;
static unsigned int submissions, reject_at, immediate, no_worker;
static uint32_t response_value;
static void decode_peer(void)
{
    cdrp_work work;
    cdrp_result result;
    if (!no_worker && cdrp_peer_take(&fake_peer, &work)) {
        cdrp_peer_validate(&work, &result);
        cdrp_peer_publish(&fake_peer, &result);
    }
}
static void respond(uint32_t result)
{
    assert(pending_callback);
    for (unsigned int i = 0u; i < 4u; ++i)
        pending_response[i] = (u8)(response_value >> (i * 8u));
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
    if (reject_at == submissions) return 0u;
    u8 *bytes = out;
    if (bytes[0] == 0xFF) cdrp_peer_reset(&fake_peer);
    if (out_length == 5u) {
        uint32_t word = (uint32_t)bytes[1] | ((uint32_t)bytes[2] << 8) |
                        ((uint32_t)bytes[3] << 16) | ((uint32_t)bytes[4] << 24);
        cdrp_peer_word(&fake_peer, word);
        decode_peer();
    }
    response_value = fake_peer.reply;
    pending_response = in;
    pending_callback = callback;
    if (immediate) respond(0u);
    return 1u;
}
static void fresh(void)
{
    cdjr_init(&link, WATCHDOG);
    cdjr_configure(&link, SETTLE, SYNC_ATTEMPTS);
    tick = owned = mailbox_done = 0u;
    data_stage = running = passed = interrupted = data_error = 0u;
    submissions = reject_at = immediate = no_worker = 0u;
    target = 1u;
    pending_callback = NULL;
    memset(&fake_peer, 0, sizeof(fake_peer));
    assert(begin_session());
}
static void advance(void)
{
    if (pending_callback) respond(0u);
    service();
    ++tick;
}
static void finish(void)
{
    unsigned int limit = 0u;
    while (passed < target) {
        assert(++limit < 5000u && !data_error && link.phase != CDJR_FAILED);
        advance();
    }
    assert(!owned && packet_tx.accepted == 3u && packet_tx.recoveries == 1u);
    assert(fake_peer.accepted == 3u);
}
int main(void)
{
    fresh();
    immediate = 1u;
    target = 16u;
    finish();
    unsigned int per_session = submissions / 16u;
    assert(submissions % 16u == 0u && per_session == 133u);
    /* Every SI operation in a full session is an interruption/watchdog/
     * rejection injection site, including verdict QUERY and READ. */
    for (unsigned int operation = 1u; operation <= per_session; ++operation) {
        fresh();
        while (submissions < operation) advance();
        assert(owned && pending_callback);
        unsigned int before = submissions;
        stop_session();
        assert(!begin_session());
        respond(0u);
        service();
        assert(!owned && submissions == before);
        assert(begin_session());
        finish(); /* fresh RESET clears half/frame state on the peer */

        fresh();
        while (submissions < operation) advance();
        u8 saved[sizeof(command)];
        memcpy(saved, command, sizeof(saved));
        tick = owned_tick + WATCHDOG;
        service();
        assert(owned && submissions == operation && !begin_session());
        assert(memcmp(saved, command, sizeof(saved)) == 0);
        respond(0u);
        service();
        assert(!owned && !begin_session());
        assert(link.error == CDJR_ERR_TIMEOUT || data_error == 1u);

        fresh();
        while (submissions < operation) advance();
        respond(8u); /* completed SI error: stop, never automatic replay */
        service();
        assert(!owned && submissions == operation);
        assert(link.error == CDJR_ERR_CALLBACK || data_error == 2u);

        fresh();
        reject_at = operation;
        while (submissions < operation) advance();
        service();
        assert(!owned && !pending_callback && submissions == operation);
        assert(link.error == CDJR_ERR_SUBMIT || data_error == 4u);
    }
    fresh();
    no_worker = 1u; /* GBA never publishes a CRC verdict: bounded exhaustion */
    for (unsigned int i = 0u; i < 500u && !data_error; ++i) advance();
    assert(data_error == 3u && packet_tx.phase == CDRP_TX_FAILED && !owned);
    assert(packet_tx.recoveries == CDRP_RETRIES && fake_peer.accepted == 0u);
    printf("Recovery SI adapter: 16 full sessions and %u operation fault sites passed.\n",
           per_session * 4u);
    return 0;
}
