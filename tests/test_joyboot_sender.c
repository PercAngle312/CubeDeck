#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "cubedeck/joyboot_sender.h"
int main(void)
{
    uint8_t image[0x200] = {0};
    uint8_t transfer[5];
    cd_jb_sender sender;
    uint32_t id, words = 0u;
    assert(cd_jb_sender_init(&sender, image, sizeof(image), 0x12345678u) == 0);
    assert(cd_jb_sender_prepare(&sender, 1u, 20u, transfer, &id) == 0);
    assert(transfer[0] == 0x15u && id == 1u);
    assert(cd_jb_sender_prepare(&sender, 2u, 20u, transfer, &id) == -1);
    assert(cd_jb_sender_complete(&sender, id + 1u, 0u, 3u) == -1);
    assert(cd_jb_sender_complete(&sender, id, 0u, 3u) == 0);
    ++words;
    while (sender.phase == 1) {
        assert(cd_jb_sender_prepare(&sender, 4u, 20u, transfer, &id) == 0);
        assert(cd_jb_sender_complete(&sender, id, 0u, 5u) == 0);
        ++words;
    }
    assert(words == 128u && sender.phase == 2);
    assert(cd_jb_sender_prepare(&sender, 6u, 20u, transfer, &id) == 0);
    assert(cd_jb_sender_complete(&sender, id, 0u, 7u) == 0);
    assert(sender.phase == 3 && sender.acknowledged_words == 128u);
    assert(cd_jb_sender_prepare(&sender, 8u, 20u, transfer, &id) == -1);
    assert(cd_jb_sender_final_reply(&sender, 0) == -1);
    assert(sender.phase == 3);
    assert(cd_jb_sender_init(&sender, image, sizeof(image), 0u) == 0);
    while (sender.phase == 1 || sender.phase == 2) {
        assert(cd_jb_sender_prepare(&sender, 10u, 20u, transfer, &id) == 0);
        assert(cd_jb_sender_complete(&sender, id, 0u, 11u) == 0);
    }
    assert(sender.phase == 3);
    assert(cd_jb_sender_final_reply(&sender, 1) == -1);
    assert(cd_jb_sender_prepare_final_read(&sender, 12u, 20u, &id) == 0);
    assert(cd_jb_sender_complete_final_read(&sender, id, 0u, 13u) == 0);
    assert(cd_jb_sender_final_reply(&sender, 1) == 0);
    assert(sender.phase == 4);
    assert(cd_jb_sender_final_reply(&sender, 1) == -1);
    assert(cd_jb_sender_init(&sender, image, sizeof(image), 0u) == 0);
    assert(cd_jb_sender_prepare(&sender, 10u, 5u, transfer, &id) == 0);
    assert(cd_jb_sender_poll(&sender, 15u) == -1);
    assert(sender.phase == -1);
    assert(cd_jb_sender_complete(&sender, id, 0u, 16u) == -1);
    puts("Integrated Joyboot encoder/queue: 128 words, CRC and timeout checked.");
    return 0;
}
