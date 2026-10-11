#include "cubedeck/joyboot_si_queue.h"
#include <string.h>
void cd_js_reset(cd_js_queue *q)
{
    if (q && q->state != CD_JS_OWNED && q->state != CD_JS_FAILED)
        q->state = CD_JS_IDLE;
}
int cd_js_begin(cd_js_queue *q, uint32_t word, uint32_t now, uint32_t timeout)
{
    if (!q || !timeout || timeout >= 0x80000000u ||
        (q->state != CD_JS_IDLE && q->state != CD_JS_DONE) ||
        q->generation == UINT32_MAX) return -1;
    ++q->generation;
    q->command_word = word;
    q->reply = q->error = 0u;
    q->deadline = now + timeout;
    q->state = CD_JS_OWNED;
    return 0;
}
int cd_js_complete(cd_js_queue *q, uint32_t generation, uint32_t si_result,
                   uint32_t reply, uint32_t now)
{
    if (!q || q->state != CD_JS_OWNED || generation != q->generation) return -1;
    if ((int32_t)(now - q->deadline) >= 0 || si_result) {
        q->error = si_result ? si_result : 1u;
        q->state = CD_JS_FAILED;
        return -1;
    }
    q->reply = reply;
    q->state = CD_JS_DONE;
    return 0;
}
int cd_js_poll(cd_js_queue *q, uint32_t now)
{
    if (!q) return -1;
    if (q->state == CD_JS_OWNED &&
        (int32_t)(now - q->deadline) >= 0) {
        /* Terminal: real SI ownership still requires callback drainage.
         * Never submit another command against potentially owned buffers. */
        q->state = CD_JS_FAILED;
        q->error = 1u;
        return -1;
    }
    return q->state == CD_JS_FAILED ? -1 : 0;
}
