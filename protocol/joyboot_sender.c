#include "cubedeck/joyboot_sender.h"
#include <string.h>
int cd_jb_sender_init(cd_jb_sender *s, const uint8_t *image, size_t length,
                      uint32_t session_key_raw)
{
    cd_jb_sender clean;
    if (!s || !image) return -1;
    memset(&clean, 0, sizeof(clean));
    if (cd_jb_words_init(&clean.words, session_key_raw, length) != 0)
        return -1;
    clean.image = image;
    clean.capacity = length;
    clean.phase = 1;
    memcpy(s, &clean, sizeof(clean));
    return 0;
}
int cd_jb_sender_prepare(cd_jb_sender *s, uint32_t now, uint32_t timeout,
                         uint8_t output[5], uint32_t *generation)
{
    cd_jb_words next;
    uint32_t word;
    unsigned int i;
    if (!s || !output || !generation || s->phase < 1 || s->phase > 2 ||
        s->queue.state == CD_JS_OWNED || s->queue.state == CD_JS_FAILED)
        return -1;
    next = s->words;
    if (s->phase == 1) {
        if (cd_jb_next(s->image, s->capacity, &next, &word) != 0)
            return -1;
    } else if (cd_jb_finish(&next, &word) != 0) return -1;
    if (cd_js_begin(&s->queue, word, now, timeout) != 0) return -1;
    s->before_submit = s->words;
    s->words = next;
    s->pending_generation = s->queue.generation;
    s->pending[0] = 0x15u;
    for (i = 0u; i < 4u; ++i)
        s->pending[i + 1u] = (uint8_t)(word >> (8u * i));
    memcpy(output, s->pending, sizeof(s->pending));
    *generation = s->pending_generation;
    return 0;
}
int cd_jb_sender_complete(cd_jb_sender *s, uint32_t generation,
                          uint32_t si_result, uint32_t now)
{
    if (!s || generation != s->pending_generation) return -1;
    if (cd_js_complete(&s->queue, generation, si_result, 0u, now) != 0) {
        s->phase = -1;
        return -1;
    }
    if (s->phase == 1) {
        ++s->acknowledged_words;
        if (s->words.offset == s->words.padded_length) s->phase = 2;
    } else if (s->phase == 2) s->phase = 3;
    return 0;
}
int cd_jb_sender_poll(cd_jb_sender *s, uint32_t now)
{
    if (!s) return -1;
    if (cd_js_poll(&s->queue, now) != 0) {
        s->phase = -1;
        return -1;
    }
    return 0;
}

int cd_jb_sender_prepare_final_read(cd_jb_sender *s, uint32_t now,
                                    uint32_t timeout, uint32_t *generation)
{
    if (!s || !generation || s->phase != 3 || s->final_read_done) return -1;
    if (cd_js_begin(&s->queue, 0x14u, now, timeout) != 0) return -1;
    s->pending_generation = s->queue.generation;
    *generation = s->pending_generation;
    return 0;
}
int cd_jb_sender_complete_final_read(cd_jb_sender *s, uint32_t generation,
                                     uint32_t si_result, uint32_t now)
{
    if (!s || s->phase != 3 || s->queue.command_word != 0x14u ||
        generation != s->pending_generation) return -1;
    if (cd_js_complete(&s->queue, generation, si_result, 0u, now) != 0) {
        s->phase = -1;
        return -1;
    }
    s->final_read_done = 1u;
    return 0;
}
int cd_jb_sender_final_reply(cd_jb_sender *s, int validated_reply)
{
    if (!s || s->phase != 3 || !s->final_read_done || s->queue.state != CD_JS_DONE)
        return -1;
    if (!validated_reply) {
        s->phase = -1;
        return -1;
    }
    s->phase = 4;
    return 0;
}

/* A native SI rejection is the only event that may roll back a prepared
 * word. A timeout or late callback must NEVER invoke this operation. */
int cd_jb_sender_reject_unsent(cd_jb_sender *s, uint32_t generation)
{
    if (!s || (s->phase != 1 && s->phase != 2) ||
        s->queue.state != CD_JS_OWNED ||
        generation != s->pending_generation ||
        generation != s->queue.generation) return -1;
    s->words = s->before_submit;
    s->queue.state = CD_JS_IDLE;
    s->queue.command_word = 0u;
    s->pending_generation = 0u;
    memset(s->pending, 0, sizeof(s->pending));
    return 0;
}
