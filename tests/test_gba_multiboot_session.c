#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "cubedeck/gba_multiboot_session.h"
int main(void)
{
    cd_mb_session s = {0};
    assert(cd_mb_begin(&s, UINT32_MAX - 10u, 100u, 64u) == 0);
    assert(cd_mb_advance(&s, CD_MB_RECOGNITION, 0u, 1, UINT32_MAX - 9u, 100u) == 0);
    assert(cd_mb_advance(&s, CD_MB_HEADER, 192u, 1, 2u, 100u) == 0);
    assert(cd_mb_advance(&s, CD_MB_PAYLOAD, 64u, 1, 3u, 100u) == 0);
    assert(cd_mb_advance(&s, CD_MB_VERIFY, 0u, 1, 4u, 100u) == 0);
    assert(s.phase == CD_MB_READY && s.header_bytes == 192u && s.payload_bytes == 64u);
    cd_mb_abort(&s);
    assert(cd_mb_begin(&s, 100u, 10u, 64u) == 0 && s.generation == 2u);
    assert(cd_mb_advance(&s, CD_MB_RECOGNITION, 0u, 0, 101u, 10u) == -1);
    assert(s.phase == CD_MB_FAILED);
    assert(cd_mb_begin(&s, 100u, 10u, 64u) == 0);
    assert(cd_mb_advance(&s, CD_MB_RECOGNITION, 0u, 1, 110u, 10u) == -1);
    assert(cd_mb_begin(&s, 100u, 10u, 64u) == 0);
    assert(cd_mb_advance(&s, CD_MB_HEADER, 192u, 1, 101u, 10u) == -1);
    assert(cd_mb_begin(&s, 100u, 10u, 64u) == 0);
    assert(cd_mb_advance(&s, CD_MB_RECOGNITION, 1u, 1, 101u, 10u) == -1);
    assert(cd_mb_begin(&s, 100u, 10u, 64u) == 0);
    assert(cd_mb_advance(&s, CD_MB_RECOGNITION, 0u, 1, 101u, 10u) == 0);
    assert(cd_mb_advance(&s, CD_MB_HEADER, 191u, 1, 102u, 10u) == -1);
    assert(cd_mb_begin(&s, 100u, 10u, 64u) == 0);
    assert(cd_mb_advance(&s, CD_MB_RECOGNITION, 0u, 1, 101u, 10u) == 0);
    assert(cd_mb_advance(&s, CD_MB_HEADER, 192u, 1, 102u, 10u) == 0);
    assert(cd_mb_advance(&s, CD_MB_PAYLOAD, 63u, 1, 103u, 10u) == -1);
    puts("Multiboot phase gate: ordered replies, lengths, deadline and wrap checked.");
    return 0;
}
