#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "joyboot_ready.h"
static unsigned submissions, deliver, refuse, command, response_length;
static uint32_t callback_result, callback_word, wrong_token;
int cd_gcn_si_submit(cd_gcn_si *b, unsigned port, const u8 *cmd,
                    u32 tx, u32 rx, uint32_t token, uint32_t frame)
{
    assert(port == 1u && tx == 1u && !b->active);
    assert(cmd[0] == 0xffu || cmd[0] == 0x00u || cmd[0] == 0x14u);
    assert(rx == (cmd[0] == 0x14u ? 5u : 3u));
    if (refuse) return -1;
    b->active = 1;
    b->token = token;
    b->submitted_frame = frame;
    b->command[0] = cmd[0];
    command = cmd[0];
    response_length = rx;
    ++submissions;
    return 0;
}
int cd_gcn_si_take(cd_gcn_si *b, uint32_t *token, uint32_t *result, u32 *word)
{
    if (!deliver || !b->active) return -1;
    b->active = 0;
    b->response[2] = (u8)(callback_word >> 16);
    *token = b->token + wrong_token;
    *result = callback_result;
    *word = callback_word;
    deliver = 0;
    return 0;
}
static void fresh(cd_jbr *r, uint32_t now, uint32_t timeout)
{
    memset(r, 0, sizeof(*r));
    submissions = deliver = refuse = wrong_token = callback_result = 0;
    callback_word = 0;
    assert(cd_jbr_start(r, 1, now, timeout) == 0);
    assert(cd_jbr_step(r, now, timeout) == 0);
    assert(command == 0xffu && response_length == 3u);
}
static int complete(cd_jbr *r, uint32_t now, uint32_t timeout, uint32_t word)
{
    callback_word = word;
    deliver = 1;
    return cd_jbr_step(r, now, timeout);
}
int main(void)
{
    cd_jbr r;
    fresh(&r, 0, 180);
    assert(complete(&r, 1, 180, 0x000004u) == 0);
    assert(command == 0x00u && r.phase == CD_JBR_STATUS);
    assert(complete(&r, 2, 180, 0) == 0);
    assert(r.status_polls == 1 && !r.si.active);
    assert(cd_jbr_step(&r, 16, 180) == 0 && submissions == 2);
    assert(cd_jbr_step(&r, 17, 180) == 0 && submissions == 3);
    assert(complete(&r, 18, 180, 0x100004u) == 0);
    assert(command == 0x14u && response_length == 5u);
    assert(complete(&r, 19, 180, 0x12345678u) == 1);
    assert(r.session_key == 0x12345678u && r.phase == CD_JBR_READY);
    assert(cd_jbr_step(&r, 999, 180) == 1 && submissions == 4);
    assert(cd_jbr_start(&r, 1, 999, 180) == -1);
    /* Wrong token, SI errors and late callbacks never advance to STATUS. */
    fresh(&r, 0, 10); wrong_token = 1;
    assert(complete(&r, 1, 10, 0) == -1 && r.error == CD_JBR_TOKEN_ERROR);
    fresh(&r, 0, 10); callback_result = 8;
    assert(complete(&r, 1, 10, 0) == -1 && r.error == CD_JBR_CALLBACK_ERROR);
    fresh(&r, 0, 10);
    assert(complete(&r, 10, 10, 0) == -1 && r.error == CD_JBR_TIMEOUT);
    /* No callback: hardware ownership and buffer contents survive timeout. */
    fresh(&r, 0, 10);
    assert(cd_jbr_step(&r, 9, 10) == 0);
    assert(cd_jbr_step(&r, 10, 10) == -1 && r.si.active);
    assert(r.awaiting && r.si.command[0] == 0xffu);
    assert(cd_jbr_start(&r, 1, 11, 10) == -1 && r.si.active);
    assert(cd_jbr_step(&r, 11, 10) == -1 && submissions == 1);
    /* Busy SI before submission: bounded retry, no token consumption. */
    memset(&r, 0, sizeof(r)); refuse = 1;
    assert(cd_jbr_start(&r, 1, 0, 10) == 0);
    assert(cd_jbr_step(&r, 0, 10) == 0 && r.token == 0 && !r.si.active);
    assert(cd_jbr_step(&r, 9, 10) == 0);
    assert(cd_jbr_step(&r, 10, 10) == -1 && r.error == CD_JBR_BUSY);
    /* Clock wraparound normal completion and exact timeout boundary. */
    fresh(&r, UINT32_MAX - 5u, 10);
    assert(complete(&r, UINT32_MAX - 4u, 10, 0) == 0);
    assert(complete(&r, UINT32_MAX - 3u, 10, 0x100004u) == 0);
    assert(complete(&r, 1, 10, 0xabcdef01u) == 1);
    fresh(&r, UINT32_MAX - 5u, 10);
    assert(cd_jbr_step(&r, 3, 10) == 0);
    assert(cd_jbr_step(&r, 4, 10) == -1 && r.si.active);
    /* Missing callbacks in every phase retain hardware buffer ownership. */
    for (unsigned phase = CD_JBR_RESET; phase <= CD_JBR_KEY; ++phase) {
        fresh(&r, 0, 10);
        if (phase >= CD_JBR_STATUS) assert(complete(&r, 1, 10, 0) == 0);
        if (phase >= CD_JBR_KEY) assert(complete(&r, 2, 10, 0x100004u) == 0);
        unsigned before = submissions;
        assert(cd_jbr_step(&r, r.deadline, 10) == -1);
        assert(r.error == CD_JBR_TIMEOUT && r.failed_phase == phase);
        assert(r.si.active && r.awaiting && submissions == before);
    }
    /* Overall cap cannot be extended by delayed, otherwise timely callbacks. */
    fresh(&r, 0, 180);
    assert(complete(&r, 179, 180, 0) == 0);
    assert(complete(&r, 358, 180, 0) == 0);
    assert(cd_jbr_step(&r, 373, 180) == 0);
    assert(complete(&r, 552, 180, 0) == 0);
    assert(cd_jbr_step(&r, 567, 180) == 0);
    assert(cd_jbr_step(&r, 720, 180) == -1 && r.si.active);
    assert(r.error == CD_JBR_TIMEOUT);
    /* STATUS can be initially unready; its attempts remain bounded. */
    fresh(&r, 0, 180);
    assert(complete(&r, 1, 180, 0) == 0);
    for (unsigned i = 0; i < 20; ++i) {
        uint32_t now = 2 + 16 * i;
        if (i) assert(cd_jbr_step(&r, now - 1, 180) == 0);
        assert(complete(&r, now, 180, 0) == (i == 19 ? -1 : 0));
    }
    assert(r.error == CD_JBR_NOT_READY && r.status_polls == 20);
    puts("Joyboot readiness: ordering, delayed readiness, busy bounds, errors, ownership and wraparound passed.");
    return 0;
}
