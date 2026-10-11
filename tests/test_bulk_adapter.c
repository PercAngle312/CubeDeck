#define CUBEDECK_STARTUP_HOST_TEST
#ifdef CUBEDECK_VERDICT_WAIT
#include "../experiments/v2-probe/gamecube-ram16-wait.c"
#elif defined(CUBEDECK_RAM16_TRANSFER)
#include "../experiments/v2-probe/gamecube-ram16.c"
#else
#include "../experiments/v2-probe/gamecube-bulk.c"
#endif
#include <assert.h>

static cdbr_peer fake_peer;
static void (*pending_callback)(s32, u32);
static u8 *pending_response;
static unsigned int submissions, reject_at, immediate, no_worker;
static uint32_t response_value;
#ifdef CUBEDECK_VERDICT_WAIT
static unsigned int hold_final;
#endif
static void decode_peer(void)
{
    cdbr_work work;
    cdbr_result result;
#ifdef CUBEDECK_VERDICT_WAIT
    if (hold_final && fake_peer.frame_sequence == CDBR_PACKETS) return;
#endif
    if (!no_worker && cdbr_peer_take(&fake_peer, &work)) {
        cdbr_peer_validate(&work, &result);
        cdbr_peer_publish(&fake_peer, &result);
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
    if (bytes[0] == 0xFF) cdbr_peer_reset(&fake_peer);
    if (out_length == 5u) {
        uint32_t word = (uint32_t)bytes[1] | ((uint32_t)bytes[2] << 8) |
                        ((uint32_t)bytes[3] << 16) | ((uint32_t)bytes[4] << 24);
        cdbr_peer_word(&fake_peer, word);
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
#ifdef CUBEDECK_VERDICT_WAIT
    hold_final = 0u;
#endif
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
        assert(++limit < target * CDBR_PACKETS * (CDBR_WORDS * 4u + CDBR_GAP + 16u) + 500u && !data_error && link.phase != CDJR_FAILED);
        advance();
    }
    assert(!owned && packet_tx.accepted == CDBR_PACKETS && packet_tx.recoveries == 1u);
    assert(fake_peer.accepted == CDBR_PACKETS && fake_peer.image_crc == CDBR_IMAGE_CRC);
    for (size_t i = 0; i < CDBR_IMAGE_BYTES; ++i)
        assert(fake_peer.image[i] == cdbr_expected_byte(i));
}
int main(void)
{
    fresh();
    immediate = 1u;
#ifdef CUBEDECK_VERDICT_WAIT
    target = 4u; /* Match the console Y-button fresh-session sequence. */
#else
    target = 2u;
#endif
    finish();
    unsigned int per_session = submissions / target;
    assert(submissions % target == 0u && per_session == 5u + (CDBR_PACKETS + 1u) * (CDBR_WORDS * 4u + 4u));
#ifdef CUBEDECK_VERDICT_WAIT
    assert(link.generation == target);
    assert(packet_tx.pending_total == 0u);
    assert(packet_tx.last_retry_sequence == 2u);
    assert(packet_tx.last_retry_reason == CDBR_RETRY_NACK);
#endif
#ifdef CUBEDECK_VERDICT_WAIT
    /* Interrupt early, midway, and near completion, then require four new
     * complete sessions. No pre-interruption chunk may count as completed. */
    const unsigned int cutoffs[] = {1u, CDBR_PACKETS / 2u, CDBR_PACKETS - 1u};
    for (unsigned int cutoff_index = 0u; cutoff_index < sizeof(cutoffs) / sizeof(cutoffs[0]); ++cutoff_index) {
        fresh();
        target = 4u;
        while (packet_tx.accepted < cutoffs[cutoff_index]) advance();
        assert(passed == 0u);
        stop_session();
        while (owned) advance(); /* Do not recycle an outstanding SI command. */
        assert(interrupted && !running && !owned && passed == 0u);
        assert(begin_session());
        finish();
        assert(passed == 4u && link.generation == 5u);
    }
#endif
    /* Every SI operation in a full session is an interruption/watchdog/
     * rejection injection site, including verdict QUERY and READ. */
    unsigned int sites = 0u;
    for (unsigned int operation = 1u; operation <= per_session; ++operation) {
#ifdef CUBEDECK_RAM16_TRANSFER
        /* Keep exhaustive 1 KiB coverage. For 16 KiB sample all startup
         * operations, every frame boundary, midpoint, verdict and final read. */
        unsigned int width = CDBR_WORDS * 4u + 4u;
        unsigned int offset = operation > 5u ? (operation - 6u) % width : 0u;
        if (operation > 5u && offset != 0u && offset != 1u &&
            offset != width / 2u && offset < width - 4u) continue;
#endif
        sites += 4u;
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
#ifdef CUBEDECK_VERDICT_WAIT
    unsigned int wait_sites = 0u;
    for (unsigned int poll = 1u; poll < CDBR_PENDING_POLLS; poll += CDBR_PENDING_POLLS - 2u) {
        for (unsigned int half = 0u; half < 2u; ++half) {
            for (unsigned int fault = 0u; fault < 3u; ++fault) {
                fresh();
                hold_final = 1u;
                while (packet_tx.pending_total < poll || !pending_callback) advance();
                if (half) advance(); /* next QUERY's READ instead of SEND */
                assert(owned && pending_callback && packet_tx.pending_total == poll);
                assert(packet_tx.phase == CDBR_TX_QUERY && !fake_peer.image_crc);
                unsigned int before = submissions;
                if (fault == 0u) {
                    stop_session();
                    assert(!begin_session());
                    respond(0u); service();
                    assert(!owned && submissions == before);
                    hold_final = 0u;
                    assert(begin_session()); finish();
                } else if (fault == 1u) {
                    u8 saved[sizeof(command)];
                    memcpy(saved, command, sizeof(saved));
                    tick = owned_tick + WATCHDOG; service();
                    assert(owned && !begin_session() && submissions == before);
                    assert(memcmp(saved, command, sizeof(saved)) == 0);
                    respond(0u); service();
                    assert(!owned && data_error == 1u && !begin_session());
                } else {
                    respond(8u); service();
                    assert(!owned && data_error == 2u && submissions == before);
                }
                ++wait_sites;
            }
        }
        fresh(); hold_final = 1u;
        while (packet_tx.pending_total < poll) advance();
        assert(!owned);
        reject_at = submissions + 1u;
        while (submissions < reject_at) advance();
        service();
        assert(!owned && !pending_callback && data_error == 4u);
        ++wait_sites;
    }
    printf("Verdict-wait SI ownership: %u delayed-query fault sites passed.\n", wait_sites);
#endif
    fresh();
    no_worker = 1u; /* GBA never publishes a CRC verdict: bounded exhaustion */
#ifdef CUBEDECK_VERDICT_WAIT
    const unsigned int verdict_wait = CDBR_PENDING_POLLS * (CDBR_GAP + 8u);
#else
    const unsigned int verdict_wait = 0u;
#endif
    for (unsigned int i = 0u; i < (CDBR_RETRIES + 1u) * (CDBR_WORDS * 4u + CDBR_GAP + 32u + verdict_wait) + 100u && !data_error; ++i) advance();
    assert(data_error == 3u && packet_tx.phase == CDBR_TX_FAILED && !owned);
    assert(packet_tx.recoveries == CDBR_RETRIES && fake_peer.accepted == 0u);
    printf("%u-byte SI adapter: %u full sessions and %u operation fault sites passed.\n",
           CDBR_IMAGE_BYTES,
#ifdef CUBEDECK_VERDICT_WAIT
           4u,
#else
           2u,
#endif
           sites);
    return 0;
}
