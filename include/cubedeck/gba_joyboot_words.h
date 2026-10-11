#ifndef CUBEDECK_GBA_JOYBOOT_WORDS_H
#define CUBEDECK_GBA_JOYBOOT_WORDS_H
#include <stddef.h>
#include <stdint.h>
/* GameCube Joybus GBA BIOS path, NOT SIO multiplayer 0x6200 multiboot.
 * All generated words are data for SI command 0x15, not SI opcodes.
 * Session-key acquisition and CRC reply checks remain caller responsibilities. */
typedef struct {
    uint32_t key;
    uint32_t crc;
    uint32_t offset;
    uint32_t padded_length;
} cd_jb_words;
int cd_jb_words_init(cd_jb_words *state, uint32_t session_key_raw,
                     size_t image_length);
int cd_jb_next(const uint8_t *image, size_t capacity, cd_jb_words *state,
               uint32_t *wire_word);
int cd_jb_finish(cd_jb_words *state, uint32_t *wire_crc);
#endif
