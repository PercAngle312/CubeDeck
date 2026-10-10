#include "cubedeck/session_sender.h"
#include "cubedeck/session_wire.h"
#include "cubedeck/packet_stream.h"
#include <stdio.h>
#include <string.h>

/* Host-only two-direction word-level integration.
 * Requests and ACK/NACK responses traverse separate incremental parsers,
 * exactly one 32-bit word at a time. Faults are deterministic.
 * Neither hardware implementation nor storage is involved.
 */
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\\n", __LINE__, #x); return 1; } } while (0)

/* Encode, optionally damage/drop a word, and feed an entire packet.
 * Return 1 if a validated packet appeared, 0 if dropped, -1 if discarded.
 * The receiver must be reinitialized on timeout for truncated frames.
 */
static int transmit_words(cdps_parser *parser, const cdtp_packet *in,
                          cdtp_packet *out, int damage, int drop)
{
    uint32_t words[CDTP_MAX_PACKET_WORDS];
    size_t count, i;
    int result = 0;
    if (cdtp_encode(in, words, CDTP_MAX_PACKET_WORDS, &count) != CDTP_OK)
        return -2;
    for (i = 0u; i < count; ++i) {
        if (drop && i == count - 1u)
            continue;
        if (damage && i == count - 1u)
            words[i] ^= 0x100u;
        result = cdps_feed(parser, words[i], out);
        if (result != 0)
            return result;
    }
    return 0;
}

static int test_word_level_transfer(size_t length)
{
    uint8_t image[CDCS_MAX_IMAGE];
    cdss_sender sender;
    cdvs_receiver receiver;
    cdps_parser gba_parser, gc_parser;
    cdtp_packet outgoing, delivered, reply, delivered_reply;
    unsigned int frame_number = 0u;
    size_t i;
    for (i = 0; i < length; ++i)
        image[i] = (uint8_t)(i * 97u + (i >> 3u));
    cdvs_init(&receiver);
    cdps_init(&gba_parser);
    cdps_init(&gc_parser);
    CHECK(cdss_init(&sender, image, length) == 0);

    while (sender.phase != CDSS_COMPLETE) {
        int outcome;
        CHECK(frame_number++ < 70u);
        CHECK(cdss_next(&sender, &outgoing) == 1);

        /* Corrupt the first DATA packet once, then use the sender's
         * bounded retry mechanism. Parser must never deliver it.
         */
        if (outgoing.opcode == CDCS_OP_DATA &&
            outgoing.sequence == 0u && sender.retries == 0u) {
            CHECK(transmit_words(&gba_parser, &outgoing, &delivered, 1, 0) == -1);
            CHECK(receiver.chunks.length == 0u);
            CHECK(cdss_timeout(&sender) == 0);
        }

        outcome = transmit_words(&gba_parser, &outgoing, &delivered, 0, 0);
        CHECK(outcome == 1);
        /* Decoding has already verified CRC, so hand its safe packet
         * straight to the verified session without redundant encoding. */
        CHECK(cdsw_receive_decoded(&receiver, &delivered, &reply) == 1);

        /* A lost ACK prompts an exact retry; replay must not append twice. */
        if (outgoing.opcode == CDCS_OP_DATA &&
            outgoing.sequence == 1u && sender.retries == 0u) {
            size_t previous = receiver.chunks.length;
            CHECK(cdss_timeout(&sender) == 0);
            CHECK(transmit_words(&gba_parser, &outgoing,
                                 &delivered, 0, 0) == 1);
            CHECK(cdsw_receive_decoded(&receiver, &delivered, &reply) == 1);
            CHECK(receiver.chunks.length == previous);
        }

        CHECK(transmit_words(&gc_parser, &reply,
                             &delivered_reply, 0, 0) == 1);
        CHECK(cdss_reply(&sender, &delivered_reply) == 1);
    }
    CHECK(receiver.verified);
    CHECK(receiver.chunks.length == length);
    CHECK(memcmp(receiver.chunks.data, image, length) == 0);
    CHECK(gba_parser.accepted >= 2u);
    CHECK(gc_parser.accepted >= 2u);
    return 0;
}

static int test_truncated_packet_timeout(void)
{
    cdps_parser parser;
    cdtp_packet data, output;
    cdps_init(&parser);
    memset(&data, 0, sizeof(data));
    data.opcode = CDCS_OP_DATA;
    data.payload_length = 4u;
    CHECK(transmit_words(&parser, &data, &output, 0, 1) == 0);
    CHECK(parser.count > 0u);
    /* The caller, not the parser, owns the timeout decision. */
    cdps_init(&parser);
    CHECK(transmit_words(&parser, &data, &output, 0, 0) == 1);
    return 0;
}

int main(void)
{
    if (test_word_level_transfer(0u) ||
        test_word_level_transfer(1u) ||
        test_word_level_transfer(513u) ||
        test_word_level_transfer(CDCS_MAX_IMAGE) ||
        test_truncated_packet_timeout())
        return 1;
    puts("Bidirectional word-stream, retries, manifest and CRC tests passed.");
    return 0;
}
