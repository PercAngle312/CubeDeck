#include "cubedeck/image_integrity.h"
#include "cubedeck/reliable_link.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)

static int check_manifest_cases(void)
{
    static const uint8_t abc[] = {'1','2','3','4','5','6','7','8','9'};
    cdim_manifest manifest;
    uint8_t modified[sizeof(abc)];
    CHECK(cdim_crc32(abc, sizeof(abc)) == 0xCBF43926u);
    CHECK(cdim_crc32(NULL, 0u) == 0u);
    manifest.expected_bytes = sizeof(abc);
    manifest.expected_crc32 = 0xCBF43926u;
    CHECK(cdim_verify(&manifest, abc, sizeof(abc)) == 1);
    CHECK(cdim_verify(&manifest, abc, sizeof(abc) - 1u) == 0);
    memcpy(modified, abc, sizeof(abc));
    modified[2] ^= 0x40u;
    CHECK(cdim_verify(&manifest, modified, sizeof(modified)) == -1);
    CHECK(cdim_verify(&manifest, abc, sizeof(abc) + 1u) == -1);
    CHECK(cdim_verify(NULL, abc, sizeof(abc)) == -1);
    CHECK(cdim_verify(&manifest, NULL, sizeof(abc)) == -1);
    manifest.expected_bytes = CDCS_MAX_IMAGE + 1u;
    CHECK(cdim_verify(&manifest, abc, sizeof(abc)) == -1);
    manifest.expected_bytes = 0;
    manifest.expected_crc32 = 0;
    CHECK(cdim_verify(&manifest, NULL, 0) == 1);
    return 0;
}

static int check_simulated_full_transfer(void)
{
    uint8_t source[CDCS_MAX_IMAGE];
    cdcs_receiver receiver;
    cdrl_sender sender;
    cdtp_packet data, response;
    uint32_t words[CDTP_MAX_PACKET_WORDS];
    cdim_manifest manifest;
    size_t count, i;
    for (i = 0; i < sizeof(source); ++i)
        source[i] = (uint8_t)((i * 13u) ^ (i >> 7));
    manifest.expected_bytes = sizeof(source);
    manifest.expected_crc32 = cdim_crc32(source, sizeof(source));
    cdcs_receiver_init(&receiver);
    CHECK(cdrl_sender_init(&sender, source, sizeof(source)) == 0);
    CHECK(cdim_verify(&manifest, receiver.data, receiver.length) == 0);
    while (sender.state != CDRL_DONE) {
        CHECK(cdrl_sender_next(&sender, &data) == 1);
        CHECK(cdtp_encode(&data, words, CDTP_MAX_PACKET_WORDS, &count) == CDTP_OK);
        CHECK(cdrl_receive_frame(&receiver, words, count, &response) == 1);
        CHECK(cdrl_sender_reply(&sender, &response) == 1);
    }
    CHECK(cdim_verify(&manifest, receiver.data, receiver.length) == 1);
    receiver.data[2048] ^= 1u;
    CHECK(cdim_verify(&manifest, receiver.data, receiver.length) == -1);
    return 0;
}

int main(void)
{
    if (check_manifest_cases() || check_simulated_full_transfer())
        return 1;
    puts("Whole-image CRC and transfer simulation passed.");
    return 0;
}
