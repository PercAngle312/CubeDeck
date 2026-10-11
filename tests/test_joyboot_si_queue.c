#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "cubedeck/joyboot_si_queue.h"
int main(void)
{
    cd_js_queue q = {0};
    assert(cd_js_begin(&q, 0x12345678u, UINT32_MAX - 5u, 20u) == 0);
    assert(q.command_word == 0x12345678u && q.generation == 1u);
    assert(cd_js_begin(&q, 9u, 0u, 20u) == -1);
    assert(cd_js_poll(&q, 3u) == 0);
    assert(cd_js_complete(&q, 1u, 0u, 0xabcdef01u, 3u) == 0);
    assert(q.state == CD_JS_DONE && q.reply == 0xabcdef01u);
    assert(cd_js_begin(&q, 7u, 10u, 5u) == 0);
    assert(cd_js_complete(&q, 1u, 0u, 0u, 11u) == -1);
    assert(cd_js_poll(&q, 15u) == -1);
    assert(q.state == CD_JS_FAILED && cd_js_begin(&q, 8u, 16u, 5u) == -1);
    cd_js_reset(&q);
    assert(q.state == CD_JS_FAILED);
    q.state = CD_JS_IDLE; /* Simulates fresh process construction. */
    assert(cd_js_begin(&q, 7u, 22u, 5u) == 0);
    assert(cd_js_complete(&q, q.generation, 8u, 0u, 23u) == -1);
    puts("Single-owner Joyboot scheduler: callback, stale generation and timeout passed.");
    return 0;
}
