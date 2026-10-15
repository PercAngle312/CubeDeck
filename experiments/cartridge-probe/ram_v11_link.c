#include "ram_v11_link.h"
#include <limits.h>
#define CD_V11_TIMEOUT 180u
#define CD_V11_SCAN_TIMEOUT 9000u
#define CD_V11_MAX_READS 24u
#define CD_V11_READ_DELAY 12u
static int due(uint32_t frame, uint32_t deadline) {
    return (int32_t)(frame-deadline)>=0;
}
static int fail(cd_v11_link *link, unsigned error) {
    link->error=error;
    link->stage=CD_V11_LINK_FAILED;
    link->client.stage=CD_V11_HEADER_FAILED;
    memset(&link->client.parsed,0,sizeof(link->client.parsed));
    return -1;
}
int cd_v11_link_start(cd_v11_link *link, uint8_t nonce, uint32_t frame) {
    if (!link || link->stage!=CD_V11_LINK_IDLE || link->si.active ||
        link->awaiting) return -1;
    cd_v11_header_start(&link->client,nonce);
    link->stage=CD_V11_LINK_BEGIN;
    link->deadline=frame+CD_V11_TIMEOUT;
    link->expires=frame+CD_V11_SCAN_TIMEOUT;
    link->retry_at=frame+1u;
    return 0;
}
int cd_v11_link_step(cd_v11_link *link, uint32_t frame) {
    uint32_t token,result,reply,request;
    u8 command[5]={0x14,0,0,0,0};
    u32 tx=1u,rx=5u;
    if (!link) return -1;
    if (link->stage==CD_V11_LINK_PASS) return 1;
    if (link->stage==CD_V11_LINK_FAILED || link->stage==CD_V11_LINK_IDLE) return -1;
    if (due(frame,link->deadline) || due(frame,link->expires)) return fail(link,1);
    if (link->awaiting) {
        if (cd_gcn_si_take(&link->si,&token,&result,&reply)) return 0;
        link->awaiting=0u; link->last_result=result; link->last_reply=reply;
        if (token!=link->token || result) return fail(link,2);
        if (!link->reading) {
            link->reading=1u; link->reads=0u;
            link->retry_at=frame+CD_V11_READ_DELAY;
        } else {
            int accepted=0;
            if (link->stage==CD_V11_LINK_BEGIN) {
                if (reply==cd_v11_begin_ack(link->client.nonce)) {
                    link->stage=CD_V11_LINK_HEADER;
                    accepted=1;
                } else if (cd_v11_field(reply,24)==CD_V11_ERROR_TAG &&
                           cd_v11_field(reply,16)==link->client.nonce &&
                           cd_v11_field(reply,8)==0u) return fail(link,3);
            } else {
                accepted=cd_v11_header_accept(&link->client,reply);
                if (accepted<0) return fail(link,3);
                if (accepted==2) { link->stage=CD_V11_LINK_PASS; return 1; }
            }
            if (accepted) {
                link->reading=0u;
                link->retry_at=frame+1u;
            } else {
                if (link->reads>=CD_V11_MAX_READS) return fail(link,4);
                link->retry_at=frame+CD_V11_READ_DELAY;
            }
        }
        /* Deadline covers one request including all stale READ replies.
         * Reset only for a newly accepted byte/session, not for stale data. */
        if (!link->reading) link->deadline=frame+CD_V11_TIMEOUT;
    }
    if (!due(frame,link->retry_at)) return 0;
    if (!link->reading) {
        request=link->stage==CD_V11_LINK_BEGIN ? cd_v11_begin(link->client.nonce) :
                                                cd_v11_header_next(&link->client);
        command[0]=0x15u;
        for (unsigned i=0;i<4u;++i) command[i+1]=(u8)(request>>(8u*i));
        tx=5u; rx=1u;
    }
    if (link->token==UINT32_MAX) return fail(link,5);
    if (cd_gcn_si_submit(&link->si,1u,command,tx,rx,link->token+1u,frame)) {
        ++link->refusals;
        return 0;
    }
    ++link->token; link->awaiting=1u;
    if (link->reading) ++link->reads;
    return 0;
}
