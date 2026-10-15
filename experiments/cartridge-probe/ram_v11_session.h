#ifndef CUBEDECK_RAM_V11_SESSION_H
#define CUBEDECK_RAM_V11_SESSION_H
/* One scan per RAM-program boot. BEGIN binds the nonce before ROM access.
 * Duplicate last-byte requests return cached data without another bus read.
 * No reset/restart command exists; terminal errors require a power cycle. */
#include "ram_v11_header_protocol.h"
#include <string.h>
#define CD_V11_BEGIN_TAG 0xD4u
#define CD_V11_BEGIN_ACK_TAG 0xD5u
#define CD_V11_BEGIN_ACK_MAGIC 0x5Au
#define CD_V11_ERROR_SESSION 3u
typedef struct {
    unsigned started, failed, offset;
    uint8_t nonce;
    uint32_t last_request, last_reply;
} cd_v11_server;
static inline uint32_t cd_v11_begin(uint8_t nonce) {
    return cd_v11_pack(CD_V11_BEGIN_TAG,nonce,0,CD_V11_REQUEST_MAGIC);
}
static inline uint32_t cd_v11_begin_ack(uint8_t nonce) {
    return cd_v11_pack(CD_V11_BEGIN_ACK_TAG,nonce,0,CD_V11_BEGIN_ACK_MAGIC);
}
static inline uint32_t cd_v11_server_reply(cd_v11_server *s, uint32_t request,
                                          cd_gba_read_byte reader, void *ctx) {
    uint8_t seq=cd_v11_field(request,16), offset=cd_v11_field(request,8);
    uint32_t error=cd_v11_pack(CD_V11_ERROR_TAG,seq,offset,CD_V11_ERROR_SESSION);
    if (!s || s->failed) return error;
    if (!s->started) {
        if (request!=cd_v11_begin(seq)) return error;
        s->started=1u; s->nonce=seq;
        s->last_request=request; s->last_reply=cd_v11_begin_ack(seq);
        return s->last_reply;
    }
    if (request==s->last_request) return s->last_reply;
    if (!cd_v11_request_valid(request) || offset!=s->offset ||
        seq!=(uint8_t)(s->nonce+s->offset) || s->offset>=CD_V11_HEADER_LENGTH) {
        s->failed=1u;
        return error;
    }
    s->last_request=request;
    s->last_reply=cd_v11_respond(request,reader,ctx);
    if (cd_v11_field(s->last_reply,24)==CD_V11_ERROR_TAG) s->failed=1u;
    else ++s->offset;
    return s->last_reply;
}
#endif
