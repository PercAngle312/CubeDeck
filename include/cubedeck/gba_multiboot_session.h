#ifndef CUBEDECK_GBA_MULTIBOOT_SESSION_H
#define CUBEDECK_GBA_MULTIBOOT_SESSION_H
#include <stdint.h>
typedef enum {
    CD_MB_IDLE = 0, CD_MB_RECOGNITION, CD_MB_HEADER,
    CD_MB_PAYLOAD, CD_MB_VERIFY, CD_MB_READY, CD_MB_FAILED
} cd_mb_phase;
typedef struct {
    cd_mb_phase phase;
    uint32_t deadline, header_bytes, payload_bytes, expected_payload;
    uint32_t generation;
} cd_mb_session;
/* Transport-independent gate. Callers must validate real BIOS responses
 * before calling acknowledge; this is NOT a multiboot wire implementation. */
int cd_mb_begin(cd_mb_session *s, uint32_t now, uint32_t timeout,
                uint32_t payload_bytes);
int cd_mb_advance(cd_mb_session *s, cd_mb_phase expected,
                  uint32_t accepted_bytes, int validated_reply,
                  uint32_t now, uint32_t timeout);
void cd_mb_abort(cd_mb_session *s);
#endif
