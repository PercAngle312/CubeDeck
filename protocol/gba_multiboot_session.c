#include "cubedeck/gba_multiboot_session.h"
#include <string.h>
#define HEADER_BYTES 192u
static int fail(cd_mb_session *s) { s->phase = CD_MB_FAILED; return -1; }
int cd_mb_begin(cd_mb_session *s, uint32_t now, uint32_t timeout,
                uint32_t payload_bytes)
{
    uint32_t next_generation;
    if (!s || !timeout || timeout >= 0x80000000u ||
        !payload_bytes || payload_bytes > 0x3FE80u ||
        (s->phase != CD_MB_IDLE && s->phase != CD_MB_FAILED)) return -1;
    if (s->generation == UINT32_MAX) return fail(s);
    next_generation = s->generation + 1u;
    memset(s, 0, sizeof(*s));
    s->generation = next_generation;
    s->phase = CD_MB_RECOGNITION;
    s->deadline = now + timeout;
    s->expected_payload = payload_bytes;
    return 0;
}
int cd_mb_advance(cd_mb_session *s, cd_mb_phase expected,
                  uint32_t accepted_bytes, int validated_reply,
                  uint32_t now, uint32_t timeout)
{
    if (!s || !timeout || timeout >= 0x80000000u) return -1;
    if (s->phase != expected || !validated_reply ||
        (int32_t)(now - s->deadline) >= 0) return fail(s);
    if (expected == CD_MB_RECOGNITION && accepted_bytes == 0u)
        s->phase = CD_MB_HEADER;
    else if (expected == CD_MB_HEADER &&
             accepted_bytes == HEADER_BYTES) {
        s->header_bytes = HEADER_BYTES;
        s->phase = CD_MB_PAYLOAD;
    } else if (expected == CD_MB_PAYLOAD &&
               accepted_bytes == s->expected_payload) {
        s->payload_bytes = accepted_bytes;
        s->phase = CD_MB_VERIFY;
    } else if (expected == CD_MB_VERIFY && accepted_bytes == 0u)
        s->phase = CD_MB_READY;
    else return fail(s);
    s->deadline = now + timeout;
    return 0;
}
void cd_mb_abort(cd_mb_session *s)
{
    if (s) s->phase = CD_MB_FAILED;
}
