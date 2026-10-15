/* Portable v11 ROM-header wire tests; no hardware or cartridge required. */
#include "ram_v11_header_protocol.h"
#include "cubedeck/gba_cart_header.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {
    uint8_t bytes[CD_V11_HEADER_LENGTH];
    unsigned reads;
    int fail_at;
} fixture;
static int read_rom(void *ctx, uint32_t offset, uint8_t *out) {
    fixture *f=(fixture *)ctx;
    assert(offset<CD_V11_HEADER_LENGTH);
    ++f->reads;
    if (f->fail_at==(int)offset) return -1;
    *out=f->bytes[offset];
    return 0;
}
int main(void) {
    fixture f={{0},0,-1};
    uint8_t collected[CD_V11_HEADER_LENGTH]={0};
    uint8_t value=0xEE;
    uint8_t sum=0;
    cd_gba_cart_header parsed;
    unsigned i;
    memcpy(f.bytes+0xA0,"CUBEDECK    ",12);
    memcpy(f.bytes+0xAC,"CD01",4);
    memcpy(f.bytes+0xB0,"HB",2);
    f.bytes[0xB2]=0x96;
    for(i=0xA0;i<=0xBC;++i) sum=(uint8_t)(sum-f.bytes[i]);
    f.bytes[0xBD]=(uint8_t)(sum-0x19u);
    assert(cd_v11_request(0x12,0x34)==0xD11234A5u);
    assert(cd_v11_pack(CD_V11_DATA_TAG,0x12,0x34,0x56)==0xD2123456u);
    for(i=0;i<CD_V11_HEADER_LENGTH;++i) {
        uint8_t seq=(uint8_t)(i+29u);
        uint32_t request=cd_v11_request(seq,(uint8_t)i);
        uint32_t reply=cd_v11_respond(request,read_rom,&f);
        assert(cd_v11_request_valid(request));
        assert(cd_v11_decode(reply,seq,(uint8_t)i,&value)==1);
        assert(value==f.bytes[i]);
        collected[i]=value;
        value=0xEE;
        assert(cd_v11_decode(reply,(uint8_t)(seq+1u),(uint8_t)i,&value)==0);
        assert(value==0xEE);
        assert(cd_v11_decode(reply,seq,(uint8_t)((i+1u)%192u),&value)==0);
        assert(value==0xEE);
    }
    assert(f.reads==CD_V11_HEADER_LENGTH);
    assert(cd_gba_parse_header(collected,sizeof(collected),&parsed)==0);
    assert(parsed.checksum_valid);
    assert(strcmp(parsed.game_code,"CD01")==0);
    /* Reject malformed and out-of-range requests without touching ROM. */
    for(i=CD_V11_HEADER_LENGTH;i<256u;++i) {
        unsigned before=f.reads;
        uint32_t reply=cd_v11_respond(cd_v11_request(7,(uint8_t)i),read_rom,&f);
        assert(f.reads==before);
        assert(cd_v11_decode(reply,7,(uint8_t)i,&value)==-1);
    }
    for(i=0;i<4u;++i) {
        uint32_t malformed=cd_v11_request(8,42);
        if(i==0) malformed^=0x01000000u;
        if(i==1) malformed^=0x00000001u;
        if(i==2) malformed^=0x80000000u;
        if(i==3) malformed^=0x000000FFu;
        unsigned before=f.reads;
        assert(!cd_v11_request_valid(malformed));
        assert(cd_v11_field(cd_v11_respond(malformed,read_rom,&f),24)==CD_V11_ERROR_TAG);
        assert(f.reads==before);
    }
    /* Callback failure is reported, never interpreted as cartridge data. */
    f.fail_at=91;
    assert(cd_v11_decode(cd_v11_respond(cd_v11_request(6,91),read_rom,&f),
                         6,91,&value)==-1);
    assert(value==0xEE);
    assert(cd_v11_decode(cd_v11_respond(cd_v11_request(6,90),NULL,&f),
                         6,90,&value)==-1);
    assert(cd_v11_decode(cd_v11_pack(CD_V11_DATA_TAG,4,5,0xFF),
                         4,5,NULL)==-1);
    assert(cd_v11_decode(cd_v11_pack(CD_V11_DATA_TAG,4,5,0xFF),
                         4,192,&value)==-1);
    puts("v11 read-only 192-byte header wire contract PASS");
    return 0;
}
