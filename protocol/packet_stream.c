#include "cubedeck/packet_stream.h"
#include <string.h>

void cdps_init(cdps_parser *parser)
{
    if (parser != NULL)
        memset(parser, 0, sizeof(*parser));
}

int cdps_feed(cdps_parser *parser, uint32_t word, cdtp_packet *out_packet)
{
    cdtp_status status;
    if (!parser || !out_packet)
        return -1;
    if (parser->count == 0u) {
        if (word != CDTP_MAGIC)
            return 0;
        parser->words[parser->count++] = word;
        return 0;
    }

    /* Header word 1 contains the version; fail early on unsupported peers. */
    if (parser->count == 1u &&
        ((word >> 24) & 0xFFu) != CDTP_PROTOCOL_VERSION) {
        parser->count = 0u;
        parser->expected = 0u;
        ++parser->rejected;
        return -1;
    }

    /* Word 2 provides a bounded byte count, before we accept payload words. */
    if (parser->count == 2u) {
        if (word > CDTP_MAX_PAYLOAD) {
            parser->count = 0u;
            parser->expected = 0u;
            ++parser->rejected;
            return -1;
        }
        parser->expected = CDTP_HEADER_WORDS + ((word + 3u) / 4u) + 1u;
    }

    if (parser->count >= CDTP_MAX_PACKET_WORDS ||
        (parser->expected != 0u && parser->count >= parser->expected)) {
        parser->count = 0u;
        parser->expected = 0u;
        ++parser->rejected;
        return -1;
    }

    parser->words[parser->count++] = word;
    if (parser->expected == 0u || parser->count < parser->expected)
        return 0;

    status = cdtp_decode(parser->words, parser->count, out_packet);
    parser->count = 0u;
    parser->expected = 0u;
    if (status != CDTP_OK) {
        ++parser->rejected;
        return -1;
    }
    ++parser->accepted;
    return 1;
}
