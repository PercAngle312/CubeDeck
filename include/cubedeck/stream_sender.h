#ifndef CUBEDECK_STREAM_SENDER_H
#define CUBEDECK_STREAM_SENDER_H

#include "cubedeck/stream_receiver.h"
#include "cubedeck/reliable_link.h"

/* Host-only bounded-memory streaming sender.
 * The caller supplies a known byte count and CRC32 manifest plus a
 * side-effect-free read callback; it is responsible for precomputing CRC.
 * Once read, a pending DATA frame is cached for bit-identical retries.
 */
typedef int (*cdtx_source)(void *context, size_t offset,
                           uint8_t *destination, size_t length);

typedef enum {
    CDTX_MANIFEST = 0,
    CDTX_DATA = 1,
    CDTX_FINISH = 2,
    CDTX_COMPLETE = 3,
    CDTX_FAILED = 4
} cdtx_phase;

typedef struct {
    cdtx_source read;
    void *context;
    uint32_t expected_length;
    uint32_t expected_crc32;
    size_t offset;
    uint32_t next_sequence;
    unsigned int retries;
    uint16_t chunk_capacity;
    cdtx_phase phase;
    cdtp_packet pending;
    int pending_valid;
} cdtx_sender;

int cdtx_init(cdtx_sender *sender, cdtx_source reader, void *context,
              uint32_t length, uint32_t crc32);
/* Select a nonzero chunk size at most 256. The caller must ensure\n * ceil(length / chunk_capacity) does not exceed 65535 sequences. */
int cdtx_init_chunked(cdtx_sender *sender, cdtx_source reader, void *context,
                      uint32_t length, uint32_t crc32, uint16_t chunk_capacity);
/* 1 = outstanding frame, 0 = completed, -1 = failed. */
int cdtx_next(cdtx_sender *sender, cdtp_packet *frame);
/* 1 = current frame acknowledged, 0 = ignored or retry, -1 = failed. */
int cdtx_reply(cdtx_sender *sender, const cdtp_packet *reply);
int cdtx_timeout(cdtx_sender *sender);
#endif
