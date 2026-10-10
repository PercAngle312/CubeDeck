#include "cubedeck/bound_stream.h"
#include <string.h>

static int pack(const cdtp_packet *plain, uint32_t id,
                uint32_t *words, size_t capacity, size_t *count)
{
    cdgu_message message;
    if (!plain || !id || plain->payload_length > CDGU_MAX_PAYLOAD)
        return -1;
    memset(&message, 0, sizeof(message));
    message.session_id = id;
    message.opcode = plain->opcode;
    message.sequence = plain->sequence;
    message.payload_length = plain->payload_length;
    memcpy(message.payload, plain->payload, plain->payload_length);
    return cdgu_encode(&message, words, capacity, count);
}

static void unpack(const cdgu_message *message, cdtp_packet *plain)
{
    memset(plain, 0, sizeof(*plain));
    plain->opcode = message->opcode;
    plain->sequence = message->sequence;
    plain->payload_length = message->payload_length;
    memcpy(plain->payload, message->payload, message->payload_length);
}

int cdbs_init_sender(cdtx_sender *sender, cdtx_source read, void *context,
                     uint32_t length, uint32_t checksum)
{
    if (length > CDBS_MAX_BYTES)
        return -1;
    return cdtx_init_chunked(sender, read, context, length, checksum,
                             CDGU_MAX_PAYLOAD);
}

int cdbs_next_frame(cdtx_sender *sender, uint32_t session_id,
                    uint32_t *words, size_t capacity, size_t *count)
{
    cdtp_packet plain;
    int result;
    if (!sender || !words || !count || !session_id)
        return -1;
    *count = 0u;
    result = cdtx_next(sender, &plain);
    if (result != 1)
        return result;
    if (pack(&plain, session_id, words, capacity, count) != 0)
        return -1;
    return 1;
}

int cdbs_receive(cdst_receiver *receiver, uint32_t session_id,
                 const uint32_t *words, size_t count,
                 uint32_t *response, size_t capacity,
                 size_t *response_count)
{
    cdgu_message request;
    cdtp_packet plain, reply;
    int result;
    if (!receiver || !session_id || !words || !response ||
        !response_count)
        return -1;
    *response_count = 0u;
    /* In particular, do NOT respond to a previous session's packet. */
    if (cdgu_decode(words, count, session_id, &request) != 0)
        return 0;
    unpack(&request, &plain);
    result = cdstw_receive_decoded(receiver, &plain, &reply);
    if (result < 0)
        return -1;
    if (pack(&reply, session_id, response, capacity, response_count) != 0)
        return -1;
    return 1;
}

int cdbs_sender_reply(cdtx_sender *sender, uint32_t session_id,
                      const uint32_t *words, size_t count)
{
    cdgu_message reply;
    cdtp_packet plain;
    if (!sender || !session_id || !words)
        return -1;
    if (cdgu_decode(words, count, session_id, &reply) != 0)
        return 0;
    unpack(&reply, &plain);
    return cdtx_reply(sender, &plain);
}
