/* Portable 32-bit Joybus ping/ack contract test. No hardware involved. */
#include "ram_v10_protocol.h"
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
    puts("v10 32-bit Joybus ping/ack contract PASS");
    return 0;
}
