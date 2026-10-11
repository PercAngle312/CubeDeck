/* Deterministic mock SI test of the v10 runtime driver; no console needed. */
#include "ram_v10_link.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t pending_token,pending_reply,pending_result;
static unsigned pending,submit_calls,write_calls;
static uint8_t last_command[5];
int cd_gcn_si_submit(cd_gcn_si *bus,unsigned port,const u8 *cmd,u32 tx,u32 rx,
                     uint32_t token,uint32_t frame) {
    assert(bus && port==1u && !pending && !bus->active);
    assert((cmd[0]==0x14 && tx==1u && rx==5u) ||
           (cmd[0]==0x15 && tx==5u && rx==1u));
    memset(last_command,0,sizeof(last_command));
    memcpy(last_command,cmd,tx);
    if (cmd[0]==0x15) ++write_calls;
    ++submit_calls;
    bus->active=1u;bus->token=token;bus->submitted_frame=frame;
    pending=1u;pending_token=token;
    return 0;
}
int cd_gcn_si_take(cd_gcn_si *bus,uint32_t *token,uint32_t *result,u32 *word) {
    if (!pending) return -1;
    *token=pending_token;*result=pending_result;*word=pending_reply;
    bus->active=0u;pending=0u;return 0;
}
int main(void) {
    cd_v10_link link={0};
    assert(cd_v10_link_start(&link,0x1234u,0u)==0);
    assert(cd_v10_link_step(&link,0u)==0);
    assert(last_command[0]==0x14 && pending);
    pending_reply=0u;
    assert(cd_v10_link_step(&link,1u)==0);
    assert(link.client.stage==CD_V10_CLIENT_WAIT_READY);
    assert(cd_v10_link_step(&link,12u)==0);
    assert(cd_v10_link_step(&link,13u)==0);
    pending_reply=CD_V10_READY_WORD;
    assert(cd_v10_link_step(&link,14u)==0);
    assert(link.client.stage==CD_V10_CLIENT_SEND_PING);
    assert(cd_v10_link_step(&link,15u)==0);
    assert(last_command[0]==0x15);
    assert(last_command[1]==0xcd && last_command[2]==0xa1 &&
           last_command[3]==0x12 && last_command[4]==0x34);
    pending_reply=0u;
    assert(cd_v10_link_step(&link,16u)==0);
    assert(link.client.stage==CD_V10_CLIENT_WAIT_ACK);
    assert(cd_v10_link_step(&link,28u)==0);
    pending_reply=cd_v10_ack(0x1233u);
    assert(cd_v10_link_step(&link,29u)==0);
    assert(link.client.stage==CD_V10_CLIENT_WAIT_ACK);
    assert(cd_v10_link_step(&link,41u)==0);
    pending_reply=cd_v10_ack(0x1234u);
    assert(cd_v10_link_step(&link,42u)==1);
    assert(link.client.stage==CD_V10_CLIENT_PASS);
    assert(write_calls==1u && submit_calls==5u);
    assert(cd_v10_link_step(&link,43u)==1);
    /* A timeout must not silently release a previously accepted SI transfer.
     * Restart both systems rather than recycling its DMA/IRQ buffers. */
    cd_v10_link stalled={0};
    assert(cd_v10_link_start(&stalled,7u,100u)==0);
    assert(cd_v10_link_step(&stalled,100u)==0);
    assert(stalled.si.active && pending);
    assert(cd_v10_link_step(&stalled,280u)==-1);
    assert(stalled.error==1u && stalled.si.active && pending);
    puts("v10 mock SI runtime link PASS");
    return 0;
}
