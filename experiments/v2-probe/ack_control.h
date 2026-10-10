#ifndef CUBEDECK_ACK_PROBE_CONTROL_H
#define CUBEDECK_ACK_PROBE_CONTROL_H
#include <stdint.h>
/* Synthetic diagnostic control words, not part of the bulk transport API.
 * The SEND of a QUERY asks for the result of the frame already decoded
 * in the GBA main loop. Its next READ returns a sequence-bound verdict. */
#define CDAP_QUERY_BASE 0x7AD00000u
#define CDAP_ACK_BASE   0xACCD0000u
#define CDAP_NACK_BASE  0xBAD00000u
#define CDAP_QUERY(seq) (CDAP_QUERY_BASE | (uint32_t)(seq))
#define CDAP_ACK(seq)   (CDAP_ACK_BASE | (uint32_t)(seq))
#define CDAP_NACK(seq)  (CDAP_NACK_BASE | (uint32_t)(seq))
#define CDAP_PENDING    0xDEAD0000u
#define CDAP_IS_QUERY(word) (((word) & 0xFFFF0000u) == CDAP_QUERY_BASE)
#endif
