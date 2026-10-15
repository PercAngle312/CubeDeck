#ifndef CUBEDECK_RAM_V11_LINK_H
#define CUBEDECK_RAM_V11_LINK_H
#include "joyboot_si_adapter.h"
#include "ram_v11_header_client.h"
#include "ram_v11_session.h"
enum { CD_V11_LINK_IDLE, CD_V11_LINK_BEGIN, CD_V11_LINK_HEADER,
       CD_V11_LINK_PASS, CD_V11_LINK_FAILED };
typedef struct {
    cd_gcn_si si;
    cd_v11_header_client client;
    uint32_t token, deadline, expires, retry_at, last_reply, last_result;
    unsigned stage, awaiting, reading, reads, refusals, error;
} cd_v11_link;
/* Zero-init static storage. Start only after uploader callbacks are drained.
 * No restart, cancellation, or resubmission of accepted WRITEs. */
int cd_v11_link_start(cd_v11_link *link, uint8_t nonce, uint32_t frame);
int cd_v11_link_step(cd_v11_link *link, uint32_t frame);
#endif
