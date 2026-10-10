#include "cubedeck/word_exchange.h"
#include "cubedeck/image_integrity.h"
#include <stdio.h>
#include <string.h>

#define LENGTH 759u
#define CHECK(e) do { if (!(e)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #e); return 1; } } while (0)

typedef struct {
    unsigned char input[LENGTH];
    unsigned char output[LENGTH];
    size_t written;
    unsigned writes;
    unsigned reads;
} fixture;

static int read_image(void *ctx, size_t offset, uint8_t *to, size_t count)
{
    fixture *f = (fixture *)ctx;
    if (offset > LENGTH || count > LENGTH - offset)
        return -1;
    memcpy(to, f->input + offset, count);
    ++f->reads;
    return 0;
}

static int write_image(void *ctx, size_t offset,
                       const uint8_t *data, size_t count)
{
    fixture *f = (fixture *)ctx;
    if (offset != f->written || offset > LENGTH || count > LENGTH - offset)
        return -1;
    memcpy(f->output + offset, data, count);
    f->written += count;
    ++f->writes;
    return 0;
}

static int single_case(unsigned mode)
{
    fixture f;
    cdtx_sender sender;
    cdst_receiver receiver;
    cdwx_link link;
    uint32_t request[CDTP_MAX_PACKET_WORDS];
    uint32_t reply[CDTP_MAX_PACKET_WORDS];
    uint32_t reply_old[CDTP_MAX_PACKET_WORDS];
    size_t request_count, reply_count, old_count = 0u, i;
    unsigned frames = 0u, fault_done = 0u;
    const uint32_t session_id = 0x31410000u + mode;
    memset(&f, 0, sizeof(f));
    for (i = 0u; i < LENGTH; ++i)
        f.input[i] = (uint8_t)(i * 19u ^ (i >> 4u));
    cdst_init(&receiver, write_image, &f);
    CHECK(cdbs_init_sender(&sender, read_image, &f, LENGTH,
                           cdim_crc32(f.input, LENGTH)) == 0);
    CHECK(cdwx_init(&link, &sender, session_id) == 0);
    while (link.phase != CDWX_COMPLETE) {
        uint32_t word;
        size_t emitted = 0u;
        int phase = sender.phase;
        unsigned reads = f.reads;
        CHECK(++frames < 20u);
        if (mode == 3u && !fault_done && phase == CDTX_DATA &&
            sender.next_sequence == 1u) {
            /* The SI driver transmits two words, then reports timeout.
             * Retry must restart at the original frame magic word. */
            CHECK(cdwx_next_word(&link, &word) == 1);
            CHECK(word == CDTP_MAGIC);
            CHECK(cdwx_next_word(&link, &word) == 1);
            CHECK(cdwx_timeout(&link) == 0);
            CHECK(sender.retries == 1u && f.reads == reads);
            CHECK(link.phase == CDWX_SENDING && link.next_word == 0u);
            fault_done = 1u;
            continue;
        }
        while (cdwx_next_word(&link, &word) == 1) {
            CHECK(emitted < CDTP_MAX_PACKET_WORDS);
            request[emitted++] = word;
        }
        CHECK(link.phase == CDWX_WAIT_REPLY);
        CHECK(emitted > 0u);
        request_count = emitted;
        CHECK(cdbs_receive(&receiver, session_id, request, request_count,
                           reply, CDTP_MAX_PACKET_WORDS, &reply_count) == 1);
        CHECK(reply_count >= CDTP_HEADER_WORDS + 1u);
        if (phase == CDTX_MANIFEST) {
            memcpy(reply_old, reply, reply_count * sizeof(reply[0]));
            old_count = reply_count;
        }
        if (phase == CDTX_DATA && sender.next_sequence == 0u && old_count) {
            /* Old MANIFEST ACK is valid CRC/ID/seq but wrong opcode. */
            for (i = 0u; i < old_count; ++i)
                CHECK(cdwx_reply_word(&link, reply_old[i]) == 0);
            CHECK(link.phase == CDWX_WAIT_REPLY && sender.offset == 0u);
        }
        if (!fault_done && phase == CDTX_DATA &&
            sender.next_sequence == 1u) {
            if (mode == 0u) {
                /* ACK completely absent. */
                CHECK(cdwx_timeout(&link) == 0);
            } else if (mode == 1u) {
                /* ACK with broken CRC should not advance. */
                reply[reply_count - 1u] ^= 0x80u;
                for (i = 0u; i < reply_count; ++i)
                    CHECK(cdwx_reply_word(&link, reply[i]) == 0);
                CHECK(cdwx_timeout(&link) == 0);
            } else if (mode == 2u) {
                /* Only a prefix arrives, then the link times out. */
                for (i = 0u; i < 2u; ++i)
                    CHECK(cdwx_reply_word(&link, reply[i]) == 0);
                CHECK(cdwx_timeout(&link) == 0);
            }
            CHECK(link.phase == CDWX_SENDING);
            CHECK(sender.retries == 1u);
            CHECK(f.reads == reads);
            fault_done = 1u;
            continue;
        }
        for (i = 0u; i < reply_count; ++i) {
            int result = cdwx_reply_word(&link, reply[i]);
            CHECK(result == (i + 1u == reply_count ? 1 : 0));
        }
    }
    CHECK(fault_done == 1u && receiver.verified);
    CHECK(f.written == LENGTH && sender.offset == LENGTH);
    CHECK(f.writes == 4u);
    CHECK(memcmp(f.input, f.output, LENGTH) == 0);
    return 0;
}

int main(void)
{
    unsigned mode;
    for (mode = 0u; mode < 4u; ++mode)
        if (single_case(mode))
            return 1;
    puts("Timeout, malformed ACK, partial reply and replay recovery passed.");
    return 0;
}
