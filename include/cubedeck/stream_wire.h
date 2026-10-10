#ifndef CUBEDECK_STREAM_WIRE_H
#define CUBEDECK_STREAM_WIRE_H

#include "cubedeck/stream_receiver.h"
#include "cubedeck/reliable_link.h"

/*
 * Host-only bridge between CRC-valid packets and a streaming receiver.
 * The sink is caller-supplied (CI uses a RAM sink). A corrupted frame is
 * NACKed without updating the receiver; the exact previous DATA frame can
 * be replayed safely after a lost ACK.
 */
int cdstw_receive_decoded(cdst_receiver *receiver,
                          const cdtp_packet *packet, cdtp_packet *reply);
int cdstw_receive_frame(cdst_receiver *receiver,
                        const uint32_t *words, size_t word_count,
                        cdtp_packet *reply);
#endif
