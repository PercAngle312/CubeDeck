#include "joyboot_upload_v10.h"
#include "ram_v10_protocol.h"
#include "cubedeck/gba_multiboot_plan.h"
#define TIMEOUT 180u
static int expired(uint32_t now, uint32_t deadline) { return (int32_t)(now-deadline)>=0; }
static int fail(cd_jbu10 *u, unsigned error) {
    u->failed_phase=u->phase; u->error=error; u->phase=CD_JBU_FAILED; return -1;
}
static uint32_t swap32(uint32_t x) {
    return (x<<24)|((x<<8)&0xff0000u)|((x>>8)&0xff00u)|(x>>24);
}
int cd_jbu10_start(cd_jbu10 *u, const uint8_t *image, size_t length, uint32_t now) {
    cd_mb_plan plan;
    if (!u || u->phase!=CD_JBU_IDLE || u->si.active || u->ready.si.active ||
        length>16384u || cd_mb_plan_image(image,length,&plan)) return -1;
    if (cd_jbr_start(&u->ready,1u,now,TIMEOUT)) return -1;
    u->image=image; u->length=length; u->phase=CD_JBU_READY;
    u->expires=now+9000u; return 0;
}
int cd_jbu10_step(cd_jbu10 *u, uint32_t now) {
    uint32_t token,result,word,client;
    u8 cmd[5]={0}; u32 tx=1u,rx=5u;
    if (!u) return -1;
    if (u->phase==CD_JBU_PASS) return 1;
    if (u->phase==CD_JBU_FAILED) return -1;
    if (u->phase==CD_JBU_IDLE) return fail(u,1);
    if (expired(now,u->expires)) return fail(u,2);
    if (u->phase==CD_JBU_READY) {
        int r=cd_jbr_step(&u->ready,now,TIMEOUT);
        if (r<0) return fail(u,3);
        if (!r) return 0;
        /* SI adapter returns LE32; original PPC sender's raw key is BE32. */
        if (cd_jb_sender_init(&u->sender,u->image,u->length,
                             swap32(u->ready.session_key))) return fail(u,4);
        u->token=u->ready.token; u->phase=CD_JBU_CLIENT; u->deadline=now+TIMEOUT;
    }
    if (u->awaiting) {
        if (cd_gcn_si_take(&u->si,&token,&result,&word)) {
            if (expired(now,u->deadline)) return fail(u,2);
            return 0; /* Never release buffers because of a watchdog. */
        }
        u->awaiting=0; u->last_reply=word;
        if (token!=u->token || result || expired(now,u->deadline)) return fail(u,5);
        if (u->phase==CD_JBU_CLIENT) u->phase=CD_JBU_STREAM;
        else if (u->phase==CD_JBU_STREAM) {
            if (cd_jb_sender_complete(&u->sender,u->generation,result,now)) return fail(u,6);
            if (u->sender.phase==3) u->phase=CD_JBU_FINAL;
        } else if (u->phase==CD_JBU_FINAL) {
            if (cd_jb_sender_complete_final_read(&u->sender,u->generation,result,now)) return fail(u,6);
            u->bios_reply=word; /* Not treated as proof of execution. */
            u->phase=CD_JBU_WAIT; u->retry_at=now+60u;
        } else if (u->phase==CD_JBU_RUNTIME) {
            if (word==CD_V10_READY_WORD) { u->phase=CD_JBU_PASS; return 1; }
            if (u->runtime_reads>=20u) return fail(u,7);
            u->phase=CD_JBU_WAIT; u->retry_at=now+15u;
        } else return fail(u,1);
        u->deadline=now+TIMEOUT;
    }
    if (expired(now,u->deadline)) return fail(u,2);
    if (u->phase==CD_JBU_WAIT) {
        if (!expired(now,u->retry_at)) return 0;
        u->phase=CD_JBU_RUNTIME;
    }
    if (u->phase==CD_JBU_CLIENT) {
        if (cd_jb_client_key(u->length,&client)) return fail(u,4);
        cmd[0]=0x15; tx=5; rx=1;
        /* Client key is transmitted most-significant byte first. */
        for (unsigned i=0;i<4;++i) cmd[i+1]=(u8)(client>>(24u-8u*i));
    } else if (u->phase==CD_JBU_STREAM) {
        if (cd_jb_sender_prepare(&u->sender,now,TIMEOUT,cmd,&u->generation)) return fail(u,6);
        /* The portable encoder reverses header words. SI wire bytes must
         * preserve the image's byte order, as the original PPC sender does.
         * Payload encryption/CRC bytes are already in their wire order. */
        if (u->sender.before_submit.offset<0xc0u) {
            u8 a=cmd[1],b=cmd[2]; cmd[1]=cmd[4]; cmd[2]=cmd[3];
            cmd[3]=b; cmd[4]=a;
        }
        tx=5; rx=1;
    } else if (u->phase==CD_JBU_FINAL) {
        cmd[0]=0x14;
        /* Queue final-read ownership only after native submission accepts. */
    } else if (u->phase==CD_JBU_RUNTIME) cmd[0]=0x14;
    else return fail(u,1);
    if (u->token==UINT32_MAX) return fail(u,8);
    if (cd_gcn_si_submit(&u->si,1u,cmd,tx,rx,u->token+1u,now)) {
        ++u->refused;
        if (u->phase==CD_JBU_STREAM &&
            cd_jb_sender_reject_unsent(&u->sender,u->generation)) return fail(u,6);
        return 0; /* Keep fixed deadline; retry only never-submitted bytes. */
    }
    ++u->token; u->awaiting=1; u->deadline=now+TIMEOUT;
    if (u->phase==CD_JBU_FINAL &&
        cd_jb_sender_prepare_final_read(&u->sender,now,TIMEOUT,&u->generation)) return fail(u,6);
    if (u->phase==CD_JBU_RUNTIME) ++u->runtime_reads;
    return 0;
}
