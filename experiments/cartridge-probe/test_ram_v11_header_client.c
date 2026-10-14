/* v11 client collector: rejects stale replies, partial headers and bad CRC. */
#include "ram_v11_header_client.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void make_header(uint8_t header[CD_V11_HEADER_LENGTH]) {
    uint8_t sum=0;
    unsigned i;
    memset(header,0,CD_V11_HEADER_LENGTH);
    memcpy(header+0xA0,"CUBEDECK    ",12);
    memcpy(header+0xAC,"CD01",4);
    memcpy(header+0xB0,"HB",2);
    for(i=0xA0;i<=0xBC;++i) sum=(uint8_t)(sum-header[i]);
    header[0xBD]=(uint8_t)(sum-0x19u);
}
static void run_case(int corrupt, int fail_at) {
    cd_v11_header_client client;
    uint8_t header[CD_V11_HEADER_LENGTH];
    unsigned i;
    make_header(header);
    if(corrupt) header[0xBD]^=1u;
    cd_v11_header_start(&client,241u);
    assert(client.stage==CD_V11_HEADER_WAIT);
    assert(cd_v11_header_accept(&client,0xD2000000u)==0);
    for(i=0;i<CD_V11_HEADER_LENGTH;++i) {
        uint8_t seq=(uint8_t)(241u+i);
        uint32_t reply=cd_v11_pack(CD_V11_DATA_TAG,seq,(uint8_t)i,header[i]);
        assert(cd_v11_header_next(&client)==cd_v11_request(seq,(uint8_t)i));
        assert(cd_v11_header_accept(&client,
            cd_v11_pack(CD_V11_DATA_TAG,(uint8_t)(seq-1u),(uint8_t)i,header[i]))==0);
        if(i) assert(cd_v11_header_accept(&client,
            cd_v11_pack(CD_V11_DATA_TAG,seq,(uint8_t)(i-1u),header[i]))==0);
        assert(client.offset==i);
        if((int)i==fail_at) {
            assert(cd_v11_header_accept(&client,
                cd_v11_pack(CD_V11_ERROR_TAG,seq,(uint8_t)i,CD_V11_ERROR_READ))==-1);
            assert(client.stage==CD_V11_HEADER_FAILED);
            assert(cd_v11_header_next(&client)==0);
            assert(cd_v11_header_accept(&client,reply)==-1);
            assert(client.parsed.title[0]==0);
            return;
        }
        int result=cd_v11_header_accept(&client,reply);
        assert(result==(i+1u==CD_V11_HEADER_LENGTH?(corrupt?-1:2):1));
        if(i+1u<CD_V11_HEADER_LENGTH) {
            assert(client.stage==CD_V11_HEADER_WAIT);
            assert(client.parsed.title[0]==0);
        }
    }
    assert(client.offset==CD_V11_HEADER_LENGTH);
    assert(cd_v11_header_next(&client)==0);
    assert(cd_v11_header_accept(&client,0)==-1);
    if(corrupt) {
        assert(client.stage==CD_V11_HEADER_FAILED);
        assert(client.parsed.title[0]==0);
    } else {
        assert(client.stage==CD_V11_HEADER_PASS);
        assert(client.parsed.checksum_valid);
        assert(strcmp(client.parsed.game_code,"CD01")==0);
    }
}
int main(void) {
    unsigned i;
    run_case(0,-1);
    run_case(1,-1);
    for(i=0;i<CD_V11_HEADER_LENGTH;++i) run_case(0,(int)i);
    puts("v11 header collector PASS: 192 interruptions, stale replies, checksum");
    return 0;
}
