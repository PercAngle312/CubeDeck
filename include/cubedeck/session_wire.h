#ifndef CUBEDECK_SESSION_WIRE_H
#define CUBEDECK_SESSION_WIRE_H

#include "cubedeck/verified_session.h"
#include "cubedeck/reliable_link.h"

/*
 * Host-only bridge: validated CDTP wire frames -> manifest-first receive
 * session -> framed ACK/NACK reply. No SI, Joybus, cartridge, or card I/O.
 *
 * Returns 1 for accepted or replayed request, 0 for rejected request,
 * -1 for API error. Always produces an ACK/NACK on a valid invocation.
 * NACK sequence is the next expected data index after manifest acquisition.
 */
int cdsw_receive_frame(cdvs_receiver *receiver, const uint32_t *words,
                       size_t word_count, cdtp_packet *reply);
#endif
