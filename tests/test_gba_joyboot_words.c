#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "cubedeck/gba_joyboot_words.h"
int main(void)
{
    uint8_t image[0x208u];
    cd_jb_words s, saved;
    uint32_t word, finish, first_payload, client_key;
    unsigned int i;
    for (i = 0u; i < sizeof(image); ++i) image[i] = (uint8_t)i;
    assert(cd_jb_client_key(0x200u, &client_key) == 0);
    assert(cd_jb_client_key(0x201u, &word) == 0);
    assert(word != client_key);
    assert(cd_jb_client_key(0x1ffu, &word) == -1);
    assert(cd_jb_client_key(0x40000u, &word) == -1);
    assert(cd_jb_client_key(0x200u, NULL) == -1);
    assert(cd_jb_words_init(&s, 0x12345678u, 0x200u) == 0);
    assert(cd_jb_finish(&s, &finish) == -1);
    assert(cd_jb_next(image, sizeof(image), &s, &word) == 0);
    assert(word == 0x00010203u); /* first header word in wire byte order */
    while (s.offset < 0xc0u) assert(cd_jb_next(image, sizeof(image), &s, &word) == 0);
    assert(cd_jb_next(image, sizeof(image), &s, &first_payload) == 0);
    assert(first_payload != 0xc3c2c1c0u); /* encrypted after header */
    while (s.offset < s.padded_length)
        assert(cd_jb_next(image, sizeof(image), &s, &word) == 0);
    assert(cd_jb_finish(&s, &finish) == 0);
    assert(cd_jb_next(image, sizeof(image), &s, &word) == -1);
    saved = s;
    assert(cd_jb_next(NULL, sizeof(image), &s, &word) == -1);
    assert(memcmp(&saved, &s, sizeof(s)) == 0);
    assert(cd_jb_words_init(&s, 0u, 0x1ffu) == -1);
    assert(cd_jb_words_init(&s, 0u, 0x3ff41u) == -1);
    assert(cd_jb_words_init(&s, 0u, 0x201u) == 0);
    while (s.offset < s.padded_length)
        assert(cd_jb_next(image, sizeof(image), &s, &word) == 0);
    assert(cd_jb_finish(&s, &finish) == 0);
    puts("Joyboot word encoder: header, payload, padding, bounds and finish passed.");
    return 0;
}
