#include "cubedeck/transfer_segments.h"

int cdseg_init_chunked(cdseg_planner *planner, size_t image_size,
                       uint16_t chunk_capacity)
{
    if (!planner || chunk_capacity == 0u ||
        chunk_capacity > CDTP_MAX_PAYLOAD)
        return -1;
    planner->image_size = image_size;
    planner->next_offset = 0u;
    planner->next_index = 0u;
    planner->chunk_capacity = chunk_capacity;
    return 0;
}

void cdseg_init(cdseg_planner *planner, size_t image_size)
{
    (void)cdseg_init_chunked(planner, image_size, CDTP_MAX_PAYLOAD);
}

int cdseg_next(cdseg_planner *planner, cdseg_span *span)
{
    size_t remaining, count, max_window;
    if (!planner || !span || planner->next_offset > planner->image_size ||
        planner->chunk_capacity == 0u ||
        planner->chunk_capacity > CDTP_MAX_PAYLOAD)
        return -1;
    if (planner->next_offset == planner->image_size)
        return 0;
    remaining = planner->image_size - planner->next_offset;
    max_window = (size_t)CDST_MAX_CHUNKS * planner->chunk_capacity;
    count = remaining <= max_window ? remaining : max_window;
    if (planner->next_index == UINT32_MAX || count == 0u)
        return -1;
    span->offset = planner->next_offset;
    span->length = (uint32_t)count;
    span->index = planner->next_index;
    span->data_packets = ((uint32_t)count + planner->chunk_capacity - 1u) /
                         planner->chunk_capacity;
    planner->next_offset += count;
    ++planner->next_index;
    return 1;
}
