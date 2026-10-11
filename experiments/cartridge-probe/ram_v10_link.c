/* Runtime-only v10 Joybus SI adapter. Deliberately independent of v9 uploader. */
#include "ram_v10_link.h"
#include <limits.h>
#define CD_V10_TIMEOUT 180u
#define CD_V10_MAX_READS 24u
#define CD_V10_READ_DELAY 12u
static int due(uint32_t frame, uint32_t until) {
    return (int32_t)(frame-until)>=0;
}
static int fail(cd_v10_link *link, unsigned error) {
    link->error=error;
    link->client.stage=CD_V10_CLIENT_FAILED;
    return -1;
}
int cd_v10_link_start(cd_v10_link *link, uint16_t sequence, uint32_t frame) {
    if (!link || link->si.active || link->awaiting ||
        link->client.stage!=CD_V10_CLIENT_IDLE) return -1;
    cd_v10_client_start(&link->client,sequence);
    link->deadline=frame+CD_V10_TIMEOUT;
    link->retry_at=frame;
    return 0;
}
int cd_v10_link_step(cd_v10_link *link, uint32_t frame) {
    uint32_t token,result,reply,request;
    u8 command[5]={0x14,0,0,0,0};
    u32 tx=1u,rx=5u;
    unsigned operation;
    if (!link) return -1;
    if (link->client.stage==CD_V10_CLIENT_PASS) return 1;
    if (link->client.stage==CD_V10_CLIENT_FAILED) return -1;
    if (link->client.stage==CD_V10_CLIENT_IDLE) return -1;
    if (due(frame,link->deadline)) return fail(link,1);
    if (link->awaiting) {
        if (cd_gcn_si_take(&link->si,&token,&result,&reply)) return 0;
        link->awaiting=0u;
        link->last_result=result;
        if (token!=link->token || result) return fail(link,2);
        if (link->active_phase==CD_V10_CLIENT_WAIT_READY) {
            int good=cd_v10_client_ready(&link->client,reply);
            if (good<0) return fail(link,3);
            if (!good) {
                if (link->reads>=CD_V10_MAX_READS) return fail(link,4);
                link->retry_at=frame+CD_V10_READ_DELAY;
            }
        } else if (link->active_phase==CD_V10_CLIENT_SEND_PING) {
            if (cd_v10_client_sent(&link->client)) return fail(link,3);
            link->retry_at=frame+CD_V10_READ_DELAY;
        } else if (link->active_phase==CD_V10_CLIENT_WAIT_ACK) {
            int good=cd_v10_client_reply(&link->client,reply);
            if (good>0) return 1;
            if (good<0) return fail(link,5);
            if (link->reads>=CD_V10_MAX_READS) return fail(link,4);
            link->retry_at=frame+CD_V10_READ_DELAY;
        } else return fail(link,3);
        link->deadline=frame+CD_V10_TIMEOUT;
    }
    if (!due(frame,link->retry_at)) return 0;
    operation=link->client.stage;
    if (operation==CD_V10_CLIENT_SEND_PING) {
        request=cd_v10_ping(link->client.sequence);
        command[0]=0x15u;
        /* GameCube Joybus WRITE encodes the word MSB-first on the wire,
         * matching the proven v9 client-key WRITE sequence. */
        for (unsigned i=0;i<4u;++i)
            command[i+1]=(u8)(request>>(24u-8u*i));
        tx=5u; rx=1u;
    } else if (operation!=CD_V10_CLIENT_WAIT_READY &&
               operation!=CD_V10_CLIENT_WAIT_ACK) return fail(link,3);
    if (link->token==UINT32_MAX) return fail(link,6);
    if (cd_gcn_si_submit(&link->si,1u,command,tx,rx,
                         link->token+1u,frame)) {
        ++link->refusals;
        return 0;
    }
    link->active_phase=operation;
    link->awaiting=1u;
    ++link->token;
    if (operation!=CD_V10_CLIENT_SEND_PING) ++link->reads;
    link->deadline=frame+CD_V10_TIMEOUT;
    return 0;
}
