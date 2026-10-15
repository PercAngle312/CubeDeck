/* Actual v11 SI client + GBA server over a deterministic Joybus mock. */
#include "ram_v11_link.h"
#include "ram_v11_application.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static cd_v11_server server;
static uint8_t header[CD_V11_HEADER_LENGTH];
static unsigned reads, writes, pending, deliver, refuse, wrong_token;
static uint32_t reply, result, pending_token;
static int fail_at, stale_at, stalled_at, si_error_at;
static int read_rom(void *ctx, uint32_t offset, uint8_t *out) {
    (void)ctx;
    assert(offset<192u);
    ++reads;
    if ((int)offset==fail_at) return -1;
    *out=header[offset]; return 0;
}
static void checksum(void) {
    uint8_t sum=0;
    for (unsigned i=0xa0;i<=0xbc;++i) sum=(uint8_t)(sum-header[i]);
    header[0xbd]=(uint8_t)(sum-0x19u);
}
static void reset(void) {
    memset(&server,0,sizeof(server)); memset(header,0,sizeof(header));
    memcpy(header+0xa0,"CUBEDECK    ",12); memcpy(header+0xac,"CD11",4);
    memcpy(header+0xb0,"HB",2); header[0xb2]=0x96u; checksum();
    reads=writes=pending=refuse=wrong_token=0; reply=result=0; deliver=1;
    fail_at=stale_at=stalled_at=si_error_at=-1;
}
int cd_gcn_si_submit(cd_gcn_si *bus,unsigned port,const u8 *cmd,u32 tx,u32 rx,
                     uint32_t token,uint32_t frame) {
    assert(bus && port==1u && !pending && !bus->active);
    if (refuse) return -1;
    assert((cmd[0]==0x14u && tx==1u && rx==5u) ||
           (cmd[0]==0x15u && tx==5u && rx==1u));
    if (cmd[0]==0x15u) {
        uint32_t request=(uint32_t)cmd[1]|((uint32_t)cmd[2]<<8)|
                         ((uint32_t)cmd[3]<<16)|((uint32_t)cmd[4]<<24);
        if (writes==0) {
            assert(cmd[1]==0xa5 && cmd[2]==0 && cmd[3]==0xf1 && cmd[4]==0xd4);
            assert(request==cd_v11_begin(241u));
        }
        else assert(request==cd_v11_request((uint8_t)(241u+writes-1u),
                                           (uint8_t)(writes-1u)));
        reply=cd_v11_server_reply(&server,request,read_rom,0);
        if ((int)writes==si_error_at) result=8u;
        if ((int)writes==stalled_at) deliver=0;
        ++writes;
    }
    bus->active=1u; bus->token=token; bus->submitted_frame=frame;
    pending_token=token; pending=1u;
    return 0;
}
int cd_gcn_si_take(cd_gcn_si *bus,uint32_t *token,uint32_t *error,u32 *word) {
    if (!pending || !deliver) return -1;
    *token=pending_token+(wrong_token?1u:0u); *error=result;
    /* Stale response has the right tag and offset but wrong transaction ID. */
    *word=(int)(writes-1u)==stale_at ? reply^0x00010000u : reply;
    bus->active=0u; pending=0u;
    return 0;
}
static int run(cd_v11_link *link, uint32_t start) {
    int status=0;
    assert(cd_v11_link_start(link,241u,start)==0);
    assert(cd_v11_link_start(link,241u,start)==-1);
    assert(cd_v11_link_step(link,start)==0 && !pending);
    for (unsigned n=1;n<9500u && !status;++n) {
        status=cd_v11_link_step(link,start+n);
        if (link->stage!=CD_V11_LINK_PASS) assert(link->client.parsed.title[0]==0);
    }
    return status;
}
static void server_tests(void) {
    reset();
    assert(cd_v11_field(cd_v11_server_reply(&server,cd_v11_request(241,0),
                                           read_rom,0),24)==CD_V11_ERROR_TAG);
    assert(!reads && !server.started);
    assert(cd_v11_server_reply(&server,cd_v11_begin(241),read_rom,0)==cd_v11_begin_ack(241));
    assert(cd_v11_server_reply(&server,cd_v11_begin(241),read_rom,0)==cd_v11_begin_ack(241));
    for (unsigned i=0;i<192u;++i) {
        uint32_t request=cd_v11_request((uint8_t)(241+i),(uint8_t)i);
        uint32_t expected=cd_v11_pack(CD_V11_DATA_TAG,(uint8_t)(241+i),(uint8_t)i,header[i]);
        assert(cd_v11_server_reply(&server,request,read_rom,0)==expected);
        assert(cd_v11_server_reply(&server,request,read_rom,0)==expected);
        assert(reads==i+1u);
    }
    assert(cd_v11_field(cd_v11_server_reply(&server,cd_v11_begin(241),read_rom,0),24)==CD_V11_ERROR_TAG);
    assert(server.failed && reads==192u);
    /* Wrong nonce/order/out-of-range/magic cannot touch the cartridge. */
    const uint32_t invalid[]={0xD1F101A5u,0xD1F200A5u,0xD1F1C0A5u,0xD1F100A4u};
    for (unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
        reset(); (void)cd_v11_server_reply(&server,cd_v11_begin(241),read_rom,0);
        assert(cd_v11_field(cd_v11_server_reply(&server,invalid[i],read_rom,0),24)==CD_V11_ERROR_TAG);
        assert(server.failed && !reads);
    }
}
int main(void) {
    server_tests();
    reset(); cd_v11_application app={0};
    assert(cd_v11_application_reset(&app)==CD_V10_READY_WORD);
    assert(cd_v11_application_reply(&app,cd_v10_ping(0x1234),read_rom,0)==cd_v10_ack(0x1234));
    assert(app.ping_pass && !reads && !app.server.started);
    assert(cd_v11_application_reply(&app,cd_v10_ping(0x5678),read_rom,0)==cd_v10_ack(0x5678));
    assert(!reads);
    assert(cd_v11_application_reply(&app,cd_v11_begin(241),read_rom,0)==cd_v11_begin_ack(241));
    assert(cd_v11_application_reset(&app)==CD_V10_READY_WORD);
    assert(app.server.failed && !app.ping_pass);
    assert(cd_v11_field(cd_v11_application_reply(&app,cd_v11_request(241,0),read_rom,0),24)==CD_V11_ERROR_TAG);
    assert(!reads);
    reset(); cd_v11_link good={0}; assert(run(&good,0)==1);
    assert(good.client.offset==192u && reads==192u && writes==193u);
    assert(!pending && !good.si.active && strcmp(good.client.parsed.game_code,"CD11")==0);
    assert(cd_v11_link_step(&good,4000)==1 && writes==193u);
    reset(); cd_v11_link wrap={0}; assert(run(&wrap,UINT32_MAX-25u)==1);
    /* Refused operations never acquire DMA ownership and have a deadline. */
    reset(); refuse=1; cd_v11_link refused={0}; assert(run(&refused,0)==-1);
    assert(refused.error==1u && refused.refusals && !pending && !reads);
    reset(); wrong_token=1; cd_v11_link wrong={0}; assert(run(&wrong,0)==-1);
    assert(wrong.error==2u && writes==1u);
    /* Every accepted WRITE can stall or fail, including BEGIN. */
    for (unsigned i=0;i<=192u;++i) {
        reset(); stalled_at=(int)i; cd_v11_link stalled={0};
        assert(run(&stalled,0)==-1);
        assert(stalled.error==1u && stalled.si.active && pending);
        unsigned before=writes;
        assert(cd_v11_link_step(&stalled,10000)==-1 && writes==before);
        reset(); si_error_at=(int)i; cd_v11_link bad={0};
        assert(run(&bad,0)==-1 && bad.error==2u);
        assert(!bad.si.active && !pending && writes==i+1u);
    }
    for (unsigned i=0;i<192u;++i) {
        reset(); fail_at=(int)i; cd_v11_link bad={0};
        assert(run(&bad,0)==-1 && bad.error==3u && bad.client.offset==i);
        reset(); stale_at=(int)i+1; cd_v11_link stale={0};
        assert(run(&stale,0)==-1 && stale.error==1u && stale.client.offset==i);
        assert(writes==i+2u); /* No accepted WRITE is ever retried. */
    }
    reset(); stale_at=0; cd_v11_link stale_begin={0};
    assert(run(&stale_begin,0)==-1 && !reads && writes==1u);
    reset(); header[0xbd]^=1; cd_v11_link checksum_bad={0};
    assert(run(&checksum_bad,0)==-1 && checksum_bad.error==3u);
    reset(); header[0xb2]=0; checksum(); cd_v11_link fixed_bad={0};
    assert(run(&fixed_bad,0)==-1 && fixed_bad.error==3u);
    reset(); memset(header,0xff,sizeof(header)); cd_v11_link empty={0};
    assert(run(&empty,0)==-1);
    assert(cd_v11_link_start(NULL,0,0)==-1 && cd_v11_link_step(NULL,0)==-1);
    puts("v11 runtime PASS: full scan, session/byte order, 193 SI stalls/errors, 192 stale/read failures, checksum, fixed byte, frame wrap");
    return 0;
}
