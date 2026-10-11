#ifndef CUBEDECK_JOYBOOT_SI_QUEUE_H
#define CUBEDECK_JOYBOOT_SI_QUEUE_H
#include <stdint.h>
typedef enum { CD_JS_IDLE, CD_JS_OWNED, CD_JS_DONE, CD_JS_FAILED } cd_js_state;
typedef struct {
    cd_js_state state;
    uint32_t generation, deadline, reply;
    uint32_t command_word;
    uint32_t error;
} cd_js_queue;
/* Portable single-owner SI mailbox. Caller owns the real DMA buffers and
 * must not release/reuse them until the native callback has returned.
 * No native SI call is made in this module. */
int cd_js_begin(cd_js_queue *q, uint32_t word, uint32_t now, uint32_t timeout);
int cd_js_complete(cd_js_queue *q, uint32_t generation, uint32_t si_result,
                   uint32_t reply, uint32_t now);
int cd_js_poll(cd_js_queue *q, uint32_t now);
void cd_js_reset(cd_js_queue *q);
#endif
