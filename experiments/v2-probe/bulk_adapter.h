#ifndef CUBEDECK_BULK_ADAPTER_H
#define CUBEDECK_BULK_ADAPTER_H
#include "bulk_packet.h"
/* Compile-time adapter names only: the larger fixture is a separate module
 * and executable, never linked into the existing recovery pair. */
#define CDRP_READY CDBR_READY
#define CDRP_PACKETS CDBR_PACKETS
#define CDRP_NACK CDBR_NACK
#define cdrp_tx cdbr_tx
#define cdrp_peer cdbr_peer
#define cdrp_work cdbr_work
#define cdrp_result cdbr_result
#define cdrp_tx_init cdbr_tx_init
#define cdrp_tx_next cdbr_tx_next
#define cdrp_tx_reply cdbr_tx_reply
#define CDRP_TX_DONE CDBR_TX_DONE
#define cdrp_peer_reset cdbr_peer_reset
#define cdrp_peer_word cdbr_peer_word
#define cdrp_peer_take cdbr_peer_take
#define cdrp_peer_validate cdbr_peer_validate
#define cdrp_peer_publish cdbr_peer_publish
#endif
