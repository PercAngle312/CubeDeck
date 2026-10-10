#include "cubedeck/transfer_segments.h"
#include <stdio.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)

static int check_length(size_t bytes)
{
    cdseg_planner planner;
    cdseg_span span;
    size_t total = 0u;
    uint32_t index = 0u;
    int result;
    cdseg_init(&planner, bytes);
    while ((result = cdseg_next(&planner, &span)) == 1) {
        CHECK(span.index == index++);
        CHECK(span.offset == total);
        CHECK(span.length > 0u && span.length <= CDST_MAX_BYTES);
        CHECK(span.data_packets >= 1u &&
              span.data_packets <= CDST_MAX_CHUNKS);
        CHECK((size_t)(span.data_packets - 1u) * CDTP_MAX_PAYLOAD <
              span.length);
        CHECK((size_t)span.data_packets * CDTP_MAX_PAYLOAD >= span.length);
        total += span.length;
    }
    CHECK(result == 0);
    CHECK(total == bytes);
    CHECK(planner.next_offset == bytes);
    CHECK(index == (bytes == 0u ? 0u :
           (uint32_t)((bytes - 1u) / CDST_MAX_BYTES + 1u)));
    return 0;
}

int main(void)
{
    cdseg_planner planner;
    cdseg_span span;
    const size_t sizes[] = {
        0u, 1u, 255u, 256u, 257u,
        CDST_MAX_BYTES - 1u, CDST_MAX_BYTES,
        CDST_MAX_BYTES + 1u, (size_t)CDST_MAX_BYTES * 2u,
        (size_t)CDST_MAX_BYTES * 2u + 1u,
        32u * 1024u * 1024u, 64u * 1024u * 1024u
    };
    size_t i;
    for (i = 0u; i < sizeof(sizes) / sizeof(sizes[0]); ++i)
        if (check_length(sizes[i]))
            return 1;
    CHECK(cdseg_next(NULL, &span) == -1);
    cdseg_init(&planner, 1u);
    CHECK(cdseg_next(&planner, NULL) == -1);
    planner.next_offset = 2u;
    CHECK(cdseg_next(&planner, &span) == -1);
    /* 32 MiB requires three sessions, not two: no sequence rollover. */
    cdseg_init(&planner, 32u * 1024u * 1024u);
    CHECK(cdseg_next(&planner, &span) == 1);
    CHECK(span.index == 0u && span.data_packets == CDST_MAX_CHUNKS);
    CHECK(cdseg_next(&planner, &span) == 1);
    CHECK(span.index == 1u && span.data_packets == CDST_MAX_CHUNKS);
    CHECK(cdseg_next(&planner, &span) == 1);
    CHECK(span.index == 2u && span.length == 512u);
    CHECK(cdseg_next(&planner, &span) == 0);
    puts("Sequence-safe 32 MiB+ segment planning passed.");
    return 0;
}
