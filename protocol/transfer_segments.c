#include "cubedeck/transfer_segments.h"

void cdseg_init(cdseg_planner *planner, size_t image_size)
{
    if (!planner)
        return;
    planner->image_size = image_size;
    planner->next_offset = 0u;
    planner->next_index = 0u;
}

int cdseg_next(cdseg_planner *planner, cdseg_span *span)
{
    size_t remaining, count;
    if (!planner || !span || planner->next_offset > planner->image_size)
        return -1;
    if (planner->next_offset == planner->image_size)
        return 0;
    remaining = planner->image_size - planner->next_offset;
    count = remaining <= CDST_MAX_BYTES ? remaining : CDST_MAX_BYTES;
    if (planner->next_index == UINT32_MAX || count == 0u)
        return -1;
    span->offset = planner->next_offset;
    span->length = (uint32_t)count;
    span->index = planner->next_index;
    span->data_packets = ((uint32_t)count + CDTP_MAX_PAYLOAD - 1u) /
                         CDTP_MAX_PAYLOAD;
    planner->next_offset += count;
    ++planner->next_index;
    return 1;
}
