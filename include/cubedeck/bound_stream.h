#ifndef CUBEDECK_BOUND_STREAM_H
#define CUBEDECK_BOUND_STREAM_H

#include "cubedeck/session_guard.h"
#include "cubedeck/stream_sender.h"
#include "cubedeck/stream_wire.h"

/* Host-only bridge between reliable streaming and transfer-ID envelopes.
 * A nonzero caller-selected ID must be unique among sessions whose traffic
 * could overlap. Max logical DATA payload is 252 bytes.
 */
#define CDBS_MAX_BYTES ((uint32_t)CDST_MAX_CHUNKS * CDGU_MAX_PAYLOAD)

/* Initialize a sender with the v2-safe 252-byte chunk size. */
int cdbs_init_sender(cdtx_sender *sender, cdtx_source read, void *context,
                     uint32_t length, uint32_t checksum);

/* Serialize a sender's outstanding frame into its transfer-ID envelope. */
int cdbs_next_frame(cdtx_sender *sender, uint32_t session_id,
                    uint32_t *words, size_t capacity, size_t *count);

/* Validate and apply exactly one incoming request. On invalid CRC/session,
 * silently discard (return 0, response_count=0), allowing a sender timeout.
 * On accepted/rejected in-session requests return 1 with framed ACK/NACK.
 * -1 denotes invalid API arguments.
 */
int cdbs_receive(cdst_receiver *receiver, uint32_t session_id,
                 const uint32_t *words, size_t count,
                 uint32_t *response, size_t capacity,
                 size_t *response_count);

/* Decode response with matching session ID, then hand to sender.
 * Returns sender's cdtx_reply result; wrong session or CRC returns 0.
 */
int cdbs_sender_reply(cdtx_sender *sender, uint32_t session_id,
                      const uint32_t *words, size_t count);
#endif
