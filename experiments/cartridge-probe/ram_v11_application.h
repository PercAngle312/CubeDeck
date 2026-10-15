#ifndef CUBEDECK_RAM_V11_APPLICATION_H
#define CUBEDECK_RAM_V11_APPLICATION_H
#include "ram_v10_protocol.h"
#include "ram_v11_session.h"
typedef struct {
    cd_v11_server server;
    unsigned ping_pass;
} cd_v11_application;
/* PING is allowed before BEGIN only. It never calls the cartridge reader. */
static inline uint32_t cd_v11_application_reply(cd_v11_application *app,
        uint32_t request, cd_gba_read_byte reader, void *ctx) {
    if (!app->server.started && cd_v10_is_ping(request)) {
        app->ping_pass=1u;
        return cd_v10_respond(request);
    }
    return cd_v11_server_reply(&app->server,request,reader,ctx);
}
static inline uint32_t cd_v11_application_reset(cd_v11_application *app) {
    if (app->server.started) app->server.failed=1u;
    app->ping_pass=0u;
    return CD_V10_READY_WORD;
}
#endif
