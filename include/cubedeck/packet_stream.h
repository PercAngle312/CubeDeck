#ifndef CUBEDECK_PACKET_STREAM_H
#define CUBEDECK_PACKET_STREAM_H

#include "cubedeck/transport_protocol.h"

/*
 * Host-only word-stream parser. Feed one 32-bit Joybus word at a time.
 * A magic word starts a frame only when the parser is idle. Malformed frames
 * are discarded and parsing resumes at the next magic word. The magic word
 * can also occur in valid payload data and must not reset an active frame.
 * Without timeout or explicit reset, a truncated frame can consume later
 * words; callers must reset the parser on transport timeout or new session.
 */
typedef struct {
    uint32_t words[CDTP_MAX_PACKET_WORDS];
    size_t count;
    size_t expected;
    uint32_t accepted;
    uint32_t rejected;
} cdps_parser;

void cdps_init(cdps_parser *parser);

/* Returns 1 for a complete CRC-valid packet, 0 for incomplete/no packet,
 * -1 for a discarded malformed frame or invalid arguments.
 * On success the output packet contains only validated bytes.
 */
int cdps_feed(cdps_parser *parser, uint32_t word, cdtp_packet *out_packet);
#endif
