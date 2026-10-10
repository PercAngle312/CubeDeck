#include "cubedeck/stream_hash.h"
#include "cubedeck/stream_wire.h"
#include <stdio.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)

/* Virtual 16.7 MB source/sink: no RAM buffer, storage device, or cartridge.
 * A per-byte function makes each chunk reproducible for retries.
 */
typedef struct {
    size_t next_offset;
    uint32_t writes;
    uint32_t reads;
} virtual_image;

static uint8_t at_byte(size_t offset)
{
    return (uint8_t)((offset * 37u) ^ (offset >> 8u) ^ (offset >> 16u));
}

static int generator(void *context, size_t offset, uint8_t *bytes,
                     size_t length)
{
    virtual_image *image = (virtual_image *)context;
    size_t i;
    for (i = 0u; i < length; ++i)
        bytes[i] = at_byte(offset + i);
    ++image->reads;
    return 0;
}

static int checker(void *context, size_t offset, const uint8_t *bytes,
                   size_t length)
{
    virtual_image *image = (virtual_image *)context;
    size_t i;
    if (offset != image->next_offset)
        return -1;
    for (i = 0u; i < length; ++i)
        if (bytes[i] != at_byte(offset + i))
            return -1;
    image->next_offset += length;
    ++image->writes;
    return 0;
}

int main(void)
{
    virtual_image image = {0u, 0u, 0u};
    cdtx_sender sender;
    cdst_receiver receiver;
    cdtp_packet frame, reply;
    uint32_t crc, count = 0u;
    CHECK(cdsh_source_crc32(generator, &image, CDST_MAX_BYTES, &crc) == 0);
    cdst_init(&receiver, checker, &image);
    CHECK(cdtx_init(&sender, generator, &image, CDST_MAX_BYTES, crc) == 0);
    while (sender.phase != CDTX_COMPLETE) {
        CHECK(cdtx_next(&sender, &frame) == 1);
        if (frame.opcode == CDCS_OP_DATA) {
            CHECK((uint32_t)frame.sequence == count);
            CHECK(frame.payload_length == CDTP_MAX_PAYLOAD);
            ++count;
        }
        CHECK(cdstw_receive_decoded(&receiver, &frame, &reply) == 1);
        CHECK(cdtx_reply(&sender, &reply) == 1);
    }
    CHECK(count == CDST_MAX_CHUNKS);
    CHECK(sender.next_sequence == CDST_MAX_CHUNKS);
    CHECK(receiver.next_sequence == CDST_MAX_CHUNKS);
    CHECK(sender.offset == CDST_MAX_BYTES);
    CHECK(receiver.offset == CDST_MAX_BYTES);
    CHECK(receiver.verified);
    CHECK(image.next_offset == CDST_MAX_BYTES);
    CHECK(image.writes == CDST_MAX_CHUNKS);
    CHECK(cdtx_next(&sender, &frame) == 0);
    puts("Max 65,535 data frames and 16,776,960 bytes verified without buffering.");
    return 0;
}
