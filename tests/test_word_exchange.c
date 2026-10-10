#include "cubedeck/word_exchange.h"
#include "cubedeck/image_integrity.h"
#include <stdio.h>
#include <string.h>

#define NBYTES 1527u
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)

typedef struct {
    uint8_t source[NBYTES];
    uint8_t target[NBYTES];
    size_t written;
} mock;

static int read_source(void *context, size_t offset,
                       uint8_t *data, size_t count)
{
    mock *m = (mock *)context;
    if (offset > NBYTES || count > NBYTES - offset)
        return -1;
    memcpy(data, m->source + offset, count);
    return 0;
}

static int write_target(void *context, size_t offset,
                        const uint8_t *data, size_t count)
{
    mock *m = (mock *)context;
    if (offset != m->written || offset > NBYTES ||
        count > NBYTES - offset)
        return -1;
    memcpy(m->target + offset, data, count);
    m->written += count;
    return 0;
}

static int run(void)
{
    mock m;
    cdwx_link link;
    cdtx_sender sender;
    cdst_receiver receiver;
    cdps_parser requests;
    cdtp_packet valid;
    uint32_t req_words[CDTP_MAX_PACKET_WORDS];
    uint32_t response[CDTP_MAX_PACKET_WORDS];
    size_t req_count, reply_count, i;
    unsigned int packets = 0u, dropped = 0u, damaged = 0u;
    const uint32_t session = 0x76B3D101u;
    memset(&m, 0, sizeof(m));
    for (i = 0u; i < NBYTES; ++i)
        m.source[i] = (uint8_t)(i * 37u ^ (i >> 4u));
    cdst_init(&receiver, write_target, &m);
    cdps_init(&requests);
    CHECK(cdbs_init_sender(&sender, read_source, &m, NBYTES,
                           cdim_crc32(m.source, NBYTES)) == 0);
    CHECK(cdwx_init(&link, &sender, session) == 0);
    while (link.phase != CDWX_COMPLETE) {
        uint32_t word;
        size_t count = 0u;
        CHECK(++packets < 30u);
        CHECK(link.phase == CDWX_SENDING);
        while (cdwx_next_word(&link, &word) == 1) {
            CHECK(++count <= CDTP_MAX_PACKET_WORDS);
            CHECK(cdps_feed(&requests, word, &valid) ==
                  (count == link.word_count ? 1 : 0));
        }
        CHECK(count == link.word_count);
        CHECK(link.phase == CDWX_WAIT_REPLY);
        CHECK(cdtp_encode(&valid, req_words, CDTP_MAX_PACKET_WORDS,
                          &req_count) == CDTP_OK);
        CHECK(cdbs_receive(&receiver, session, req_words, req_count,
                           response, CDTP_MAX_PACKET_WORDS,
                           &reply_count) == 1);
        if (sender.phase == CDTX_DATA && sender.next_sequence == 1u &&
            !dropped) {
            dropped = 1u;
            CHECK(cdwx_timeout(&link) == 0);
            continue;
        }
        if (sender.phase == CDTX_DATA && sender.next_sequence == 3u &&
            !damaged) {
            uint32_t bad[CDTP_MAX_PACKET_WORDS];
            memcpy(bad, response, reply_count * sizeof(response[0]));
            bad[reply_count - 1u] ^= 1u;
            for (i = 0u; i < reply_count; ++i)
                CHECK(cdwx_reply_word(&link, bad[i]) == 0);
            CHECK(link.phase == CDWX_WAIT_REPLY);
            damaged = 1u;
            CHECK(cdwx_timeout(&link) == 0);
            continue;
        }
        for (i = 0u; i < reply_count; ++i) {
            int result = cdwx_reply_word(&link, response[i]);
            CHECK(result == (i + 1u == reply_count ? 1 : 0));
        }
    }
    CHECK(dropped && damaged);
    CHECK(sender.phase == CDTX_COMPLETE);
    CHECK(receiver.verified && m.written == NBYTES);
    CHECK(memcmp(m.source, m.target, NBYTES) == 0);
    CHECK(cdwx_next_word(&link, &response[0]) == 0);
    return 0;
}

static int test_retry_exhaustion(void)
{
    mock m;
    cdtx_sender sender;
    cdwx_link link;
    uint32_t word;
    unsigned int attempts;
    memset(&m, 0, sizeof(m));
    CHECK(cdbs_init_sender(&sender, read_source, &m, 0u, 0u) == 0);
    CHECK(cdwx_init(&link, &sender, 0x1234ABCDu) == 0);
    for (attempts = 0u; attempts < CDRL_MAX_RETRIES + 1u; ++attempts) {
        size_t emitted = 0u;
        while (cdwx_next_word(&link, &word) == 1)
            ++emitted;
        CHECK(emitted == CDTP_HEADER_WORDS + 3u + 1u);
        CHECK(link.phase == CDWX_WAIT_REPLY);
        if (attempts != CDRL_MAX_RETRIES) {
            CHECK(cdwx_timeout(&link) == 0);
            CHECK(link.phase == CDWX_SENDING);
        } else {
            CHECK(cdwx_timeout(&link) == -1);
            CHECK(link.phase == CDWX_FAILED);
            CHECK(sender.phase == CDTX_FAILED);
            CHECK(cdwx_next_word(&link, &word) == -1);
        }
    }
    return 0;
}

int main(void)
{
    if (run() || test_retry_exhaustion())
        return 1;
    puts("Nonblocking session-bound Joybus word scheduler simulation passed.");
    return 0;
}
