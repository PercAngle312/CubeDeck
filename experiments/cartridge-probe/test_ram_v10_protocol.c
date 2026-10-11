/* Portable v10 frame-contract tests. Build:
 * cc -std=c11 -Wall -Wextra -Werror -pedantic -o /tmp/cd-v10-test \
 *    experiments/cartridge-probe/test_ram_v10_protocol.c
 * /tmp/cd-v10-test
 * Does not exercise SI hardware or a GBA. */
#include "ram_v10_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    uint8_t frame[CD_V10_FRAME_BYTES];
    static const uint8_t expected[8]={'C','D','P','A',0x78,0x56,0x34,0x12};
    cd_v10_encode(frame,CD_V10_ACK,0x12345678u);
    assert(sizeof(frame)==8u);
    assert(memcmp(frame,expected,sizeof(frame))==0);
    assert(cd_v10_load_le32(frame)==CD_V10_ACK);
    assert(cd_v10_load_le32(frame+4)==0x12345678u);
    assert(cd_v10_is_ack(frame,0x12345678u));
    assert(!cd_v10_is_ack(frame,0x12345679u));
    frame[0]^=1u;
    assert(!cd_v10_is_ack(frame,0x12345678u));
    cd_v10_encode(frame,CD_V10_PING,0xffffffffu);
    assert(cd_v10_load_le32(frame)==CD_V10_PING);
    assert(cd_v10_load_le32(frame+4)==0xffffffffu);
    assert(!cd_v10_is_ack(frame,0xffffffffu));
    puts("v10 portable frame tests PASS");
    return 0;
}
