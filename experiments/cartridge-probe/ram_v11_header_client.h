#ifndef CUBEDECK_RAM_V11_HEADER_CLIENT_H
#define CUBEDECK_RAM_V11_HEADER_CLIENT_H
/* Portable, bounded, one-request-at-a-time v11 header collector.
 * This is a protocol model, not a GameCube SI implementation.
 * A caller must finish each SI transaction before starting the next,
 * enforce a timeout, and reboot after terminal transport errors. */
#include <stdint.h>
#include <string.h>
#include "ram_v11_header_protocol.h"
#include "cubedeck/gba_cart_header.h"
enum cd_v11_header_stage {
    CD_V11_HEADER_IDLE, CD_V11_HEADER_WAIT, CD_V11_HEADER_PASS,
    CD_V11_HEADER_FAILED
};
typedef struct {
    uint8_t bytes[CD_V11_HEADER_LENGTH];
    cd_gba_cart_header parsed;
    uint8_t nonce;
    unsigned offset;
    enum cd_v11_header_stage stage;
} cd_v11_header_client;
static inline void cd_v11_header_start(cd_v11_header_client *client,
                                        uint8_t nonce) {
    memset(client,0,sizeof(*client));
    client->nonce=nonce;
    client->stage=CD_V11_HEADER_WAIT;
}
static inline uint32_t cd_v11_header_next(const cd_v11_header_client *client) {
    if (!client || client->stage!=CD_V11_HEADER_WAIT ||
        client->offset>=CD_V11_HEADER_LENGTH) return 0u;
    return cd_v11_request((uint8_t)(client->nonce+client->offset),
                          (uint8_t)client->offset);
}
/* 0: stale/unrelated reply, 1: next byte accepted, 2: complete valid
 * header, -1: terminal error. Never publish parsed fields before PASS. */
static inline int cd_v11_header_accept(cd_v11_header_client *client,
                                        uint32_t reply) {
    uint8_t value=0;
    int status;
    if (!client || client->stage!=CD_V11_HEADER_WAIT ||
        client->offset>=CD_V11_HEADER_LENGTH) return -1;
    status=cd_v11_decode(reply,
                         (uint8_t)(client->nonce+client->offset),
                         (uint8_t)client->offset,&value);
    if (status==0) return 0;
    if (status<0) {
        client->stage=CD_V11_HEADER_FAILED;
        return -1;
    }
    client->bytes[client->offset++]=value;
    if (client->offset!=CD_V11_HEADER_LENGTH) return 1;
    if (cd_gba_parse_header(client->bytes,sizeof(client->bytes),
                            &client->parsed)!=0 ||
        !client->parsed.checksum_valid) {
        memset(&client->parsed,0,sizeof(client->parsed));
        client->stage=CD_V11_HEADER_FAILED;
        return -1;
    }
    client->stage=CD_V11_HEADER_PASS;
    return 2;
}
#endif
