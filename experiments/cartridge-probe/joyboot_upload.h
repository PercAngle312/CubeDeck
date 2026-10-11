#ifndef CUBEDECK_JOYBOOT_UPLOAD_H
#define CUBEDECK_JOYBOOT_UPLOAD_H
#include "joyboot_ready.h"
#include "cubedeck/joyboot_sender.h"
#define CD_RAM9_REPLY 0x39524443u
/* Static lifetime, zero-init once. No restart/cancel after accepted SI. */
enum { CD_JBU_IDLE, CD_JBU_READY, CD_JBU_CLIENT, CD_JBU_STREAM,
       CD_JBU_FINAL, CD_JBU_WAIT, CD_JBU_RUNTIME, CD_JBU_PASS, CD_JBU_FAILED };
typedef struct {
    cd_jbr ready;
    cd_gcn_si si;
    cd_jb_sender sender;
    const uint8_t *image;
    size_t length;
    unsigned phase, failed_phase, error, awaiting, refused, runtime_reads;
    uint32_t token, generation, deadline, expires, retry_at, bios_reply, last_reply;
} cd_jbu;
int cd_jbu_start(cd_jbu *u, const uint8_t *image, size_t length, uint32_t now);
/* PASS requires the RAM program's distinct reply, not the final BIOS read. */
int cd_jbu_step(cd_jbu *u, uint32_t now);
#endif
