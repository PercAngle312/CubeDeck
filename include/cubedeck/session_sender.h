#ifndef CUBEDECK_SESSION_SENDER_H
#define CUBEDECK_SESSION_SENDER_H

#include "cubedeck/verified_session.h"
#include "cubedeck/reliable_link.h"

/* Host-only manifest -> DATA -> FINISH stop-and-wait state machine.
 * Caller supplies timeouts; no hardware, storage, or card accesses.
 */
typedef enum {
    CDSS_MANIFEST = 0,
    CDSS_DATA = 1,
    CDSS_FINISH = 2,
    CDSS_COMPLETE = 3,
    CDSS_FAILED = 4
} cdss_phase;

typedef struct {
    const uint8_t *source;
    size_t length;
    size_t offset;
    uint32_t crc32;
    uint16_t sequence;
    unsigned int retries;
    cdss_phase phase;
} cdss_sender;

int cdss_init(cdss_sender *s, const uint8_t *source, size_t length);
/* 1 packet to send, 0 done, -1 failure */
int cdss_next(const cdss_sender *s, cdtp_packet *out);
/* 1 acknowledged, 0 stale/retry needed, -1 fatal error */
int cdss_reply(cdss_sender *s, const cdtp_packet *reply);
/* 0 retry allowed, -1 exhausted */
int cdss_timeout(cdss_sender *s);
#endif
