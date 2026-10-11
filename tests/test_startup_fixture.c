#include <assert.h>
#include <stdio.h>
#include "../experiments/v2-probe/startup_fixture.h"
int main(void)
{
    cdsr_peer peer = {0};
    for (uint32_t generation = 1u; generation <= 1000u; ++generation) {
        cdsr_reset(&peer);
        assert(peer.reply == CDSR_READY && peer.received == 0u);
        assert(peer.session_word == 0u && peer.resets == generation);
        cdsr_receive(&peer, 0xDEADBEEFu);
        assert(peer.reply == CDSR_READY && peer.received == 0u);
        cdsr_receive(&peer, CDSR_WORD(generation));
        assert(peer.reply == (CDSR_WORD(generation) ^ CDSR_REPLY_XOR));
        assert(peer.received == 1u);
        /* Next RESET must discard both the old reply and session word. */
    }
    puts("Startup peer: 1000 interrupted/fresh RAM sessions passed.");
    return 0;
}
