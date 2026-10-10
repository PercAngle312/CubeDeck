#ifndef CUBEDECK_TRANSFER_SEGMENTS_H
#define CUBEDECK_TRANSFER_SEGMENTS_H

#include "cubedeck/stream_receiver.h"

/*
 * Host-only planning of large image transfers into independent, bounded
 * manifest/DATA/FINISH sessions. No on-wire segment identifier yet:
 * do NOT connect this planner to hardware without adding a unique session ID
 * and ordering validation to protect against stale ACKs across sessions.
 */
typedef struct {
    size_t image_size;
    size_t next_offset;
    uint32_t next_index;
    uint16_t chunk_capacity;
} cdseg_planner;

typedef struct {
    size_t offset;
    uint32_t length;
    uint32_t index;
    uint32_t data_packets;
} cdseg_span;

void cdseg_init(cdseg_planner *planner, size_t image_size);
/* 0 success, -1 invalid chunk capacity/pointers. For transfer-ID packets,
 * pass CDGU_MAX_PAYLOAD (252), not the legacy 256-byte size. */
int cdseg_init_chunked(cdseg_planner *planner, size_t image_size,
                       uint16_t chunk_capacity);
/* 1 = span available; 0 = no spans left; -1 = invalid pointers/state. */
int cdseg_next(cdseg_planner *planner, cdseg_span *span);
#endif
