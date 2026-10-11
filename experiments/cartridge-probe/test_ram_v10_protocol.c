/* Portable 32-bit Joybus ping/ack contract test. No hardware involved. */
#include "ram_v10_protocol.h"
#include "ram_v10_client.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    uint8_t wire[4];
    uint32_t ping = cd_v10_ping(0x1234u);
    uint32_t ack = cd_v10_ack(0x1234u);
    const uint8_t expected_ping[4] = {0x34,0x12,0xA1,0xCD};
    cd_v10_store_le32(wire,ping);
    assert(sizeof(wire)==4u);
    assert(memcmp(wire,expected_ping,4)==0);
    assert(cd_v10_load_le32(wire)==ping);
    assert(cd_v10_is_ping(ping));
    assert(!cd_v10_is_ping(ack));
    assert(cd_v10_sequence(ping)==0x1234u);
    assert(cd_v10_is_ack(ack,0x1234u));
    assert(!cd_v10_is_ack(ack,0x1235u));
    assert(!cd_v10_is_ack(cd_v10_error(0x1234u),0x1234u));
    assert(!cd_v10_is_ack(CD_V10_READY_WORD,0u));
    assert(cd_v10_is_ack(cd_v10_ack(0xffffu),0xffffu));
    assert(!cd_v10_is_ack(cd_v10_ack(0u),0xffffu));
    for (uint32_t seq=0;seq<=65535u;++seq) {
        uint32_t request=cd_v10_ping((uint16_t)seq);
        uint32_t response=cd_v10_respond(request);
        assert(cd_v10_is_ack(response,(uint16_t)seq));
        assert(cd_v10_sequence(response)==seq);
        assert(!cd_v10_is_ack(response,(uint16_t)(seq+1u)));
        assert(cd_v10_respond(cd_v10_ack((uint16_t)seq))==
               cd_v10_error((uint16_t)seq));
    }
    assert(cd_v10_respond(CD_V10_READY_WORD)==cd_v10_error(0u));
    cd_v10_client client={0};
    cd_v10_client_start(&client,0x1234u);
    assert(client.stage==CD_V10_CLIENT_WAIT_READY);
    assert(cd_v10_client_ready(&client,CD_V10_ACK_TAG)==0);
    assert(client.stage==CD_V10_CLIENT_WAIT_READY);
    assert(cd_v10_client_ready(&client,CD_V10_READY_WORD)==1);
    assert(client.stage==CD_V10_CLIENT_SEND_PING);
    assert(cd_v10_client_sent(&client)==0);
    assert(cd_v10_client_reply(&client,cd_v10_ack(0x1233u))==0);
    assert(client.stage==CD_V10_CLIENT_WAIT_ACK);
    assert(cd_v10_client_reply(&client,CD_V10_READY_WORD)==0);
    assert(cd_v10_client_reply(&client,cd_v10_ack(0x1234u))==1);
    assert(client.stage==CD_V10_CLIENT_PASS);
    assert(cd_v10_client_reply(&client,cd_v10_ack(0x1234u))==-1);
    cd_v10_client_start(&client,42u);
    assert(cd_v10_client_ready(&client,CD_V10_READY_WORD)==1);
    assert(cd_v10_client_sent(&client)==0);
    assert(cd_v10_client_reply(&client,cd_v10_error(42u))==-1);
    assert(client.stage==CD_V10_CLIENT_FAILED);
    puts("v10 32-bit Joybus ping/ack contract PASS");
    return 0;
}
