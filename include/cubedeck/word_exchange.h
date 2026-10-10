#ifndef CUBEDECK_WORD_EXCHANGE_H
#define CUBEDECK_WORD_EXCHANGE_H

#include "cubedeck/bound_stream.h"
#include "cubedeck/packet_stream.h"

/* Host-only, non-blocking 32-bit word send/reply scheduler.
 * A hardware driver owns the actual Joybus SI calls, timeout tick source,
 * interrupt safety, and session ID negotiation. No hardware or file I/O.
 */
typedef enum {
    CDWX_IDLE = 0,
    CDWX_SENDING = 1,
    CDWX_WAIT_REPLY = 2,
    CDWX_COMPLETE = 3,
    CDWX_FAILED = 4
} cdwx_phase;

typedef struct {
    cdtx_sender *sender;
    uint32_t session_id;
    uint32_t words[CDTP_MAX_PACKET_WORDS];
    size_t word_count;
    size_t next_word;
    cdps_parser replies;
    cdwx_phase phase;
} cdwx_link;

int cdwx_init(cdwx_link *link, cdtx_sender *sender, uint32_t session_id);
/* 1 means one outgoing word ready, 0 means waiting/done, -1 fatal. */
int cdwx_next_word(cdwx_link *link, uint32_t *word);
/* Feed incoming reply word: 1 advances to next packet, 0 pending/ignored,
 * -1 fatal. Requires an external timeout when words stop arriving. */
int cdwx_reply_word(cdwx_link *link, uint32_t word);
/* Restart the exact cached outgoing frame; -1 on retry exhaustion. */
int cdwx_timeout(cdwx_link *link);
#endif
