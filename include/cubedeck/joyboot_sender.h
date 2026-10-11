#ifndef CUBEDECK_JOYBOOT_SENDER_H
#define CUBEDECK_JOYBOOT_SENDER_H
#include <stddef.h>
#include <stdint.h>
#include "cubedeck/gba_joyboot_words.h"
#include "cubedeck/joyboot_si_queue.h"
typedef struct {
    cd_jb_words words;
    cd_js_queue queue;
    const uint8_t *image;
    size_t capacity;
    uint32_t pending_generation;
    uint32_t acknowledged_words;
    uint8_t final_read_done;
    uint8_t pending[5];
    int phase; /* 1=words, 2=checksum, 3=await final BIOS read, 4=finished, -1=failed */
} cd_jb_sender;
/* Starts AFTER BIOS readiness/key exchange. No physical SI calls or memory
 * writes are performed here. Caller sends the returned 5 bytes via SI 0x15
 * and must drain callbacks before retiring DMA buffers. */
int cd_jb_sender_init(cd_jb_sender *s, const uint8_t *image, size_t length,
                      uint32_t session_key_raw);
int cd_jb_sender_prepare(cd_jb_sender *s, uint32_t now, uint32_t timeout,
                         uint8_t output[5], uint32_t *generation);
int cd_jb_sender_complete(cd_jb_sender *s, uint32_t generation,
                          uint32_t si_result, uint32_t now);
int cd_jb_sender_poll(cd_jb_sender *s, uint32_t now);
/* Final BIOS response is not presumed successful. Only an external,
 * protocol-specific validator may advance phase 3 to phase 4. */
/* SI 0x14 read after CRC. Caller must deliver and validate its response. */
int cd_jb_sender_prepare_final_read(cd_jb_sender *s, uint32_t now,
                                    uint32_t timeout, uint32_t *generation);
int cd_jb_sender_complete_final_read(cd_jb_sender *s, uint32_t generation,
                                     uint32_t si_result, uint32_t now);
int cd_jb_sender_final_reply(cd_jb_sender *s, int validated_reply);
#endif
