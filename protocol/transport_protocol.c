#include "cubedeck/transport_protocol.h"

#define CDTP_CRC32_POLYNOMIAL 0xEDB88320u

static uint32_t crc32_byte(uint32_t crc, uint8_t byte)
{
    unsigned int bit;
    crc ^= byte;
    for (bit = 0; bit < 8u; ++bit)
        crc = (crc >> 1) ^ ((crc & 1u) ? CDTP_CRC32_POLYNOMIAL : 0u);
    return crc;
}

static uint32_t crc32_word_le(uint32_t crc, uint32_t word)
{
    crc = crc32_byte(crc, (uint8_t)word);
    crc = crc32_byte(crc, (uint8_t)(word >> 8));
    crc = crc32_byte(crc, (uint8_t)(word >> 16));
    crc = crc32_byte(crc, (uint8_t)(word >> 24));
    return crc;
}

static uint32_t packet_crc(const uint32_t *words, const uint8_t *payload,
                           size_t payload_length)
{
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;

    crc = crc32_word_le(crc, words[0]);
    crc = crc32_word_le(crc, words[1]);
    crc = crc32_word_le(crc, words[2]);
    for (i = 0; i < payload_length; ++i)
        crc = crc32_byte(crc, payload[i]);
    return crc ^ 0xFFFFFFFFu;
}

static size_t payload_word_count(size_t payload_length)
{
    return (payload_length + 3u) / 4u;
}

cdtp_status cdtp_encode(const cdtp_packet *packet, uint32_t *words,
                        size_t word_capacity, size_t *out_word_count)
{
    size_t payload_words;
    size_t total_words;
    size_t i;

    if (packet == NULL || words == NULL || out_word_count == NULL)
        return CDTP_ERR_ARGUMENT;
    *out_word_count = 0;
    if (packet->payload_length > CDTP_MAX_PAYLOAD)
        return CDTP_ERR_TOO_LARGE;
    payload_words = payload_word_count(packet->payload_length);
    total_words = CDTP_HEADER_WORDS + payload_words + 1u;
    if (word_capacity < total_words)
        return CDTP_ERR_CAPACITY;

    words[0] = CDTP_MAGIC;
    words[1] = ((uint32_t)CDTP_PROTOCOL_VERSION << 24) |
               ((uint32_t)packet->opcode << 16) | packet->sequence;
    words[2] = packet->payload_length;
    for (i = 0; i < payload_words; ++i)
        words[CDTP_HEADER_WORDS + i] = 0u;

    for (i = 0; i < packet->payload_length; ++i) {
        size_t word_index = CDTP_HEADER_WORDS + (i / 4u);
        unsigned int shift = (unsigned int)((i % 4u) * 8u);
        words[word_index] |= (uint32_t)packet->payload[i] << shift;
    }

    words[total_words - 1u] = packet_crc(words, packet->payload,
                                         packet->payload_length);
    *out_word_count = total_words;
    return CDTP_OK;
}

cdtp_status cdtp_decode(const uint32_t *words, size_t word_count,
                        cdtp_packet *out_packet)
{
    uint32_t metadata;
    uint16_t payload_length;
    uint8_t payload[CDTP_MAX_PAYLOAD];
    size_t payload_words;
    size_t expected_words;
    size_t i;

    if (words == NULL || out_packet == NULL)
        return CDTP_ERR_ARGUMENT;
    if (word_count < CDTP_HEADER_WORDS + 1u)
        return CDTP_ERR_FORMAT;
    if (words[0] != CDTP_MAGIC)
        return CDTP_ERR_FORMAT;

    metadata = words[1];
    if (((metadata >> 24) & 0xFFu) != CDTP_PROTOCOL_VERSION)
        return CDTP_ERR_FORMAT;
    if (words[2] > CDTP_MAX_PAYLOAD)
        return CDTP_ERR_TOO_LARGE;

    payload_length = (uint16_t)words[2];
    payload_words = payload_word_count(payload_length);
    expected_words = CDTP_HEADER_WORDS + payload_words + 1u;
    if (word_count != expected_words)
        return CDTP_ERR_FORMAT;

    for (i = 0; i < payload_length; ++i) {
        uint32_t packed = words[CDTP_HEADER_WORDS + (i / 4u)];
        payload[i] = (uint8_t)(packed >> ((i % 4u) * 8u));
    }

    /* Require deterministic zero padding for a partial final word. */
    if ((payload_length % 4u) != 0u && payload_words != 0u) {
        unsigned int used_bits = (unsigned int)((payload_length % 4u) * 8u);
        uint32_t last_payload_word = words[CDTP_HEADER_WORDS + payload_words - 1u];
        if ((last_payload_word >> used_bits) != 0u)
            return CDTP_ERR_FORMAT;
    }

    if (words[word_count - 1u] != packet_crc(words, payload, payload_length))
        return CDTP_ERR_CRC;

    out_packet->opcode = (uint8_t)(metadata >> 16);
    out_packet->sequence = (uint16_t)metadata;
    out_packet->payload_length = payload_length;
    for (i = 0; i < payload_length; ++i)
        out_packet->payload[i] = payload[i];
    return CDTP_OK;
}
