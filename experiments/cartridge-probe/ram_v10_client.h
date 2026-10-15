#ifndef CUBEDECK_RAM_V10_CLIENT_H
#define CUBEDECK_RAM_V10_CLIENT_H
/* Portable GameCube-side application transaction policy.
 * SI operations are driven by a separate hardware adapter. Never treat
 * the GBA's READY marker or a BIOS reply as a PING acknowledgment. */
#include <stdint.h>
#include "ram_v10_protocol.h"
enum cd_v10_client_stage {
    CD_V10_CLIENT_IDLE, CD_V10_CLIENT_WAIT_READY,
    CD_V10_CLIENT_SEND_PING, CD_V10_CLIENT_WAIT_ACK,
    CD_V10_CLIENT_PASS, CD_V10_CLIENT_FAILED
};
typedef struct {
    enum cd_v10_client_stage stage;
    uint16_t sequence;
    uint32_t last_reply;
} cd_v10_client;
static inline void cd_v10_client_start(cd_v10_client *c, uint16_t seq) {
    c->stage=CD_V10_CLIENT_WAIT_READY;
    c->sequence=seq;
    c->last_reply=0;
}
/* Returns 1 only when a runtime READY word is observed. */
static inline int cd_v10_client_ready(cd_v10_client *c, uint32_t reply) {
    if (c->stage!=CD_V10_CLIENT_WAIT_READY) return -1;
    c->last_reply=reply;
    if (reply!=CD_V10_READY_WORD) return 0;
    c->stage=CD_V10_CLIENT_SEND_PING;
    return 1;
}
static inline int cd_v10_client_sent(cd_v10_client *c) {
    if (c->stage!=CD_V10_CLIENT_SEND_PING) return -1;
    c->stage=CD_V10_CLIENT_WAIT_ACK;
    return 0;
}
/* Wrong/old sequence never counts as success; caller manages timeout. */
static inline int cd_v10_client_reply(cd_v10_client *c, uint32_t reply) {
    if (c->stage!=CD_V10_CLIENT_WAIT_ACK) return -1;
    c->last_reply=reply;
    if (cd_v10_is_ack(reply,c->sequence)) {
        c->stage=CD_V10_CLIENT_PASS;
        return 1;
    }
    if ((reply & CD_V10_TAG_MASK)==CD_V10_ERROR_TAG) {
        c->stage=CD_V10_CLIENT_FAILED;
        return -1;
    }
    return 0;
}
#endif
