/* Exercise the real adapter; only libogc SI/IRQ entrypoints are replaced. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "joyboot_si_adapter.h"
static unsigned interrupts=1, calls, refused, complete_on_restore;
static SICallback pending_callback, last_callback;
static s32 pending_channel;
static u8 *pending_tx,*pending_rx;
static u32 pending_tx_length,pending_rx_length;
static const u8 bytes[5]={0x78,0x56,0x34,0x12,0x10};
static void finish(s32 channel,u32 result) {
    assert(pending_callback && pending_rx_length<=sizeof(bytes));
    memcpy(pending_rx,bytes,pending_rx_length);
    SICallback callback=pending_callback;
    pending_callback=NULL; last_callback=callback;
    unsigned saved=interrupts; interrupts=0; callback(channel,result); interrupts=saved;
}
u32 IRQ_Disable(void) { unsigned saved=interrupts; interrupts=0; return saved; }
void IRQ_Restore(u32 saved) {
    assert(!interrupts && saved<=1); interrupts=saved;
    if (interrupts && complete_on_restore && pending_callback) {
        complete_on_restore=0; finish(pending_channel,0);
    }
}
u32 SI_Transfer(s32 channel,void *out,u32 tx,void *in,u32 rx,SICallback cb,u32 delay) {
    assert(!interrupts && channel>=0 && channel<=3 && delay==50);
    assert(out && in && cb && tx && tx<=32 && rx<=32);
    assert((uintptr_t)out%32==0 && (uintptr_t)in%32==0);
    ++calls;
    if (refused) return 0;
    assert(!pending_callback);
    pending_callback=cb; pending_channel=channel;
    pending_tx=out; pending_tx_length=tx; pending_rx=in; pending_rx_length=rx;
    return 1;
}
static void drain(cd_gcn_si *b,u32 expected_token,u32 expected_result) {
    u32 token=0,result=0,word=0;
    assert(cd_gcn_si_take(b,&token,&result,&word)==0);
    assert(token==expected_token && result==expected_result && word==0x12345678);
    assert(!b->active && !b->done);
    assert(cd_gcn_si_take(b,&token,&result,&word)==-1);
}
int main(void) {
    cd_gcn_si a={0},b={0};
    u8 command[5]={0x15,1,2,3,4};
    u32 token=0,result=0,word=0;
    /* Invalid inputs must not enter the SDK or disturb IRQ state. */
    assert(cd_gcn_si_submit(NULL,1,command,5,5,1,0)==-1);
    assert(cd_gcn_si_submit(&a,1,NULL,5,5,1,0)==-1);
    assert(cd_gcn_si_submit(&a,4,command,5,5,1,0)==-1);
    assert(cd_gcn_si_submit(&a,1,command,0,5,1,0)==-1);
    assert(cd_gcn_si_submit(&a,1,command,33,5,1,0)==-1);
    assert(cd_gcn_si_submit(&a,1,command,5,33,1,0)==-1);
    assert(!calls && interrupts);
    assert(cd_gcn_si_take(NULL,&token,&result,&word)==-1);
    assert(cd_gcn_si_take(&a,NULL,&result,&word)==-1);
    /* Refusal releases only the never-accepted owner; a different bus may
     * immediately submit. Padding and response bytes are deterministic. */
    memset(a.command,0xa5,32); memset(a.response,0xa5,32); refused=1;
    assert(cd_gcn_si_submit(&a,1,command,5,5,10,7)==-1);
    assert(!a.active && !pending_callback && interrupts);
    assert(!memcmp(a.command,command,5));
    for (unsigned i=5;i<32;++i) assert(a.command[i]==0);
    for (unsigned i=0;i<32;++i) assert(a.response[i]==0);
    refused=0;
    assert(cd_gcn_si_submit(&b,1,command,5,5,11,8)==0);
    assert(b.active && !b.done && b.token==11 && b.submitted_frame==8);
    assert(pending_tx==b.command && pending_rx==b.response && pending_tx_length==5);
    command[1]=0xee; assert(b.command[1]==1); /* caller storage is independent */
    cd_gcn_si snapshot=b; unsigned before=calls;
    assert(cd_gcn_si_submit(&b,1,command,5,5,12,9)==-1);
    assert(cd_gcn_si_submit(&a,2,command,5,5,12,9)==-1);
    assert(calls==before && !memcmp(b.command,snapshot.command,32));
    assert(cd_gcn_si_take(&b,&token,&result,&word)==-1 && b.active);
    /* Wrong channel cannot complete an owner. SDK delivers its one correct
     * callback later. A duplicate before drainage cannot replace the result. */
    pending_callback(2,8); assert(!b.done && b.active);
    finish(1,8); assert(b.done && b.result==8 && b.active);
    last_callback(1,0); assert(b.result==8);
    assert(cd_gcn_si_take(&a,&token,&result,&word)==-1);
    assert(cd_gcn_si_submit(&a,1,command,5,5,12,10)==-1 && calls==before);
    drain(&b,11,8);
    last_callback(1,99); assert(!b.active && !b.done); /* no current owner */
    /* Completion at the outer IRQ restore boundary is visible immediately. */
    complete_on_restore=1;
    assert(cd_gcn_si_submit(&a,1,command,5,5,12,11)==0);
    assert(a.active && a.done && !pending_callback && interrupts);
    drain(&a,12,0);
    /* Neither submit nor take may enable IRQs that the caller had disabled. */
    interrupts=0;
    assert(cd_gcn_si_submit(&a,1,command,5,5,13,12)==0 && !interrupts);
    finish(1,0); drain(&a,13,0); assert(!interrupts); interrupts=1;
    /* Extended delay does not cancel SI or allow either owner's buffers to
     * be reused. This adapter has no timeout API; the caller must retain it. */
    assert(cd_gcn_si_submit(&a,1,command,5,5,14,13)==0);
    snapshot=a; before=calls;
    for (unsigned now=14;now<1000;++now) {
        assert(cd_gcn_si_take(&a,&token,&result,&word)==-1);
        assert(cd_gcn_si_submit(&b,1,command,5,5,15,now)==-1);
        assert(cd_gcn_si_submit(&a,1,command,5,5,15,now)==-1);
        assert(!memcmp(a.command,snapshot.command,32) && a.token==14);
    }
    assert(calls==before && a.active && !a.done && interrupts);
    finish(1,0); drain(&a,14,0);
    /* One-byte replies cannot leak older bytes into the LE32 decoded word. */
    assert(cd_gcn_si_submit(&b,1,command,5,1,15,1000)==0);
    finish(1,0);
    assert(cd_gcn_si_take(&b,&token,&result,&word)==0 && word==0x78 && token==15);
    assert(!b.active && interrupts && !pending_callback);
    puts("Actual SI adapter: aligned buffers, exclusive ownership, rejection, immediate/delayed callbacks, result preservation, IRQ masks and response clearing passed.");
}
