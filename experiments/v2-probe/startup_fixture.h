#ifndef CUBEDECK_STARTUP_FIXTURE_H
#define CUBEDECK_STARTUP_FIXTURE_H
#include <stdint.h>
/* RAM-only diagnostic contract, deliberately separate from packet ACK tests. */
#define CDSR_READY 0x43535259u
#define CDSR_WORD_BASE 0x534E0000u
#define CDSR_REPLY_XOR 0xA55A3CC3u
#define CDSR_WORD(generation) (CDSR_WORD_BASE | ((generation) & 0xFFFFu))
typedef struct {
    uint32_t reply, session_word, resets, received;
} cdsr_peer;
static inline void cdsr_reset(cdsr_peer *peer)
{
    peer->reply = CDSR_READY;
    peer->session_word = 0u;
    peer->received = 0u;
    ++peer->resets;
}
static inline void cdsr_receive(cdsr_peer *peer, uint32_t word)
{
    if ((word & 0xFFFF0000u) != CDSR_WORD_BASE) return;
    peer->session_word = word;
    peer->reply = word ^ CDSR_REPLY_XOR;
    ++peer->received;
}
#endif
