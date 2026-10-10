#include "cubedeck/session_guard.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

/* Mirrors the GameCube's exact seven-byte v2 sample and the GBA IRQ's
 * word-count calculation without touching any cart or save hardware. */
int main(void)
{
    const uint8_t sample[] = {0x43u,0x44u,0x42u,0x4Bu,0x10u,0x20u,0x30u};
    cdgu_message packet, decoded;
    cdtp_packet raw;
    uint32_t words[CDTP_MAX_PACKET_WORDS];
    uint32_t captured[CDTP_MAX_PACKET_WORDS];
    size_t n, i, received = 0u, expected = 0u;
    memset(&packet, 0, sizeof(packet));
    packet.session_id = 0x42CD2026u;
    packet.opcode = 0x01u;
    packet.sequence = 1u;
    packet.payload_length = sizeof(sample);
    memcpy(packet.payload, sample, sizeof(sample));
    CHECK(cdgu_encode(&packet, words, CDTP_MAX_PACKET_WORDS, &n) == 0);
    CHECK(n == 7u && words[0] == CDTP_MAGIC && words[2] == 11u);
    CHECK((words[1] & 0x00ff0000u) == 0x00810000u);
    for (i = 0u; i < n; ++i) {
        captured[received++] = words[i];
        if (received == 3u)
            expected = 4u + ((words[i] + 3u) / 4u);
        CHECK(expected == 0u || received <= expected);
    }
    CHECK(received == expected && expected == 7u);
    CHECK(cdtp_decode(captured, received, &raw) == CDTP_OK);
    CHECK(raw.payload_length == 11u && raw.opcode == 0x81u);
    CHECK(cdgu_decode(captured, received, packet.session_id, &decoded) == 0);
    CHECK(decoded.sequence == 1u && decoded.opcode == 1u);
    CHECK(decoded.payload_length == sizeof(sample));
    CHECK(memcmp(decoded.payload, sample, sizeof(sample)) == 0);
    CHECK(cdgu_decode(captured, received, packet.session_id + 1u,
                       &decoded) == -2);
    captured[n - 1u] ^= 1u;
    CHECK(cdtp_decode(captured, n, &raw) == CDTP_ERR_CRC);
    puts("Exact 7-word v2 hardware probe encode/IRQ capture/decode passed.");
    return 0;
}
