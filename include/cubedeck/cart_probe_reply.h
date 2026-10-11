#ifndef CUBEDECK_CART_PROBE_REPLY_H
#define CUBEDECK_CART_PROBE_REPLY_H
#include <stdint.h>
/* Single-word read-only GBA probe status, not a ROM dump or game identity.
 * More detailed header reporting will require framed Joybus messages. */
#define CD_CART_PROBE_MAGIC 0xCDCA0000u
#define CD_CART_PROBE_VALID 0x00000100u
#define CD_CART_PROBE_FAILED 0x00000200u
#define CD_CART_PROBE_VERSION 1u
uint32_t cd_cart_probe_reply(int valid);
int cd_cart_probe_decode(uint32_t word, int *valid);
#endif
