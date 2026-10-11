#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "joyboot_upload.h"
static unsigned deliver,refuse;
static uint32_t result,reply,wrong;
static unsigned command;
static u8 last[5];
int cd_gcn_si_submit(cd_gcn_si *b,unsigned port,const u8 *cmd,u32 tx,u32 rx,uint32_t token,uint32_t frame) {
    assert(port==1 && !b->active && tx<=5 && rx<=5);
    assert(cmd[0]==0xff || cmd[0]==0 || cmd[0]==0x14 || cmd[0]==0x15);
    memcpy(last,cmd,tx);
    if (refuse) return -1;
    b->active=1; b->token=token; b->submitted_frame=frame; command=cmd[0];
    memcpy(b->command,cmd,tx); return 0;
}
int cd_gcn_si_take(cd_gcn_si *b,uint32_t *token,uint32_t *r,u32 *w) {
    if (!deliver || !b->active) return -1;
    b->active=0; b->response[2]=(u8)(reply>>16);
    *token=b->token+wrong; *r=result; *w=reply; deliver=0; return 0;
}
static uint8_t image[512];
static void fresh(cd_jbu *u,uint32_t now) {
    memset(u,0,sizeof(*u)); deliver=refuse=result=reply=wrong=0;
    assert(cd_jbu_start(u,image,sizeof(image),now)==0);
    assert(cd_jbu_step(u,now)==0 && command==0xff);
}
static int complete(cd_jbu *u,uint32_t now,uint32_t value) {
    deliver=1; reply=value; return cd_jbu_step(u,now);
}
static uint32_t client(cd_jbu *u,uint32_t now) {
    fresh(u,now);
    assert(complete(u,++now,0)==0 && command==0);
    assert(complete(u,++now,0x100004)==0 && command==0x14);
    assert(complete(u,++now,0x12345678)==0 && u->phase==CD_JBU_CLIENT);
    /* Independently known conversion: LE adapter raw -> PPC raw -> key. */
    assert(u->sender.words.key==0x7d50330b);
    assert(command==0x15); return now;
}
int main(void) {
    image[0]=0x2e; image[3]=0xea; /* Asymmetric ARM entry branch bytes. */
    image[0xb2]=0x96; image[0xbd]=(uint8_t)(-0x96-0x19);
    cd_jbu u; uint32_t now=client(&u,0);
    assert(complete(&u,++now,0)==0 && u.phase==CD_JBU_STREAM);
    assert(!memcmp(last+1,image,4)); /* Header wire bytes, not host word order. */
    /* Refusal rolls back the next word without changing encrypted bytes. */
    refuse=1; assert(complete(&u,++now,0)==0 && !u.si.active);
    u8 rejected[5]; memcpy(rejected,last,5);
    assert(cd_jbu_step(&u,++now)==0 && !memcmp(rejected,last,5));
    refuse=0; assert(cd_jbu_step(&u,++now)==0 && !memcmp(rejected,last,5));
    while (u.phase==CD_JBU_STREAM) assert(complete(&u,++now,0)==0);
    assert(u.phase==CD_JBU_FINAL && command==0x14);
    assert(complete(&u,++now,0)==0 && u.phase==CD_JBU_WAIT);
    assert(cd_jbu_step(&u,now+59)==0 && !u.si.active);
    now+=60; assert(cd_jbu_step(&u,now)==0 && u.phase==CD_JBU_RUNTIME);
    assert(complete(&u,++now,0xdeadbeef)==0 && u.phase==CD_JBU_WAIT);
    now+=15; assert(cd_jbu_step(&u,now)==0);
    assert(complete(&u,++now,CD_RAM9_REPLY)==1 && u.phase==CD_JBU_PASS);
    assert(cd_jbu_start(&u,image,sizeof(image),now)==-1);
    /* Accepted writes remain owned after a missing/late callback. */
    now=client(&u,0);
    u8 owned[5]; memcpy(owned,u.si.command,5);
    assert(cd_jbu_step(&u,u.deadline)==-1 && u.si.active);
    assert(!memcmp(owned,u.si.command,5));
    assert(cd_jbu_start(&u,image,sizeof(image),999)==-1);
    assert(complete(&u,999,0)==-1 && u.si.active);
    now=client(&u,0); wrong=1;
    assert(complete(&u,++now,0)==-1 && u.phase==CD_JBU_FAILED);
    now=client(&u,0); result=8;
    assert(complete(&u,++now,0)==-1);
    now=client(&u,UINT32_MAX-3u);
    assert(complete(&u,++now,0)==0 && u.phase==CD_JBU_STREAM);
    /* Permanently busy client submit cannot extend its initial deadline. */
    fresh(&u,0); assert(complete(&u,1,0)==0); assert(complete(&u,2,0x100004)==0);
    refuse=1; assert(complete(&u,3,0x12345678)==0 && !u.si.active);
    assert(cd_jbu_step(&u,182)==0); assert(cd_jbu_step(&u,183)==-1);
    puts("RAM upload: key endian conversion, runtime acceptance, refusal rollback, watchdog ownership, stale/error callbacks and wrap passed.");
}
