#ifndef CUBEDECK_BULK_PACKET_H
#define CUBEDECK_BULK_PACKET_H
#include "cubedeck/session_guard.h"

/* Synthetic 1 KiB RAM image fixture, not the bulk transport wire API.
 * Frame words travel as tagged low/high halves, so payload/CRC bits cannot
 * alias a session/restart/query marker. IDs are
 * 16-bit nonzero, never reused within one running GameCube process. */
#ifdef CUBEDECK_VERDICT_WAIT
#ifndef CUBEDECK_RAM16_TRANSFER
#error "Verdict wait requires the 16 KiB fixture"
#endif
#define CDBR_READY 0x43565736u
#elif defined(CUBEDECK_RAM16_TRANSFER)
#define CDBR_READY 0x43535236u
#else
#define CDBR_READY 0x43425259u
#endif
#define CDBR_XOR 0xA55A3CC3u
#define CDBR_SESSION 0x53000000u
#define CDBR_FRAME 0x54000000u
#define CDBR_LOW 0x56000000u
#define CDBR_HIGH 0x57000000u
#define CDBR_QUERY 0x7B000000u
#define CDBR_ACK 0xAC000000u
#define CDBR_NACK 0xBA000000u
#define CDBR_PENDING 0xDE000000u
#define CDBR_KEY(id, seq) (((uint32_t)(id) << 8) | (uint32_t)(seq))
#ifdef CUBEDECK_RAM16_TRANSFER
#ifdef CUBEDECK_VERDICT_WAIT
#define CDBR_TRANSFER(id) (0xB3CD0000u | (uint32_t)(id))
#else
#define CDBR_TRANSFER(id) (0xB2CD0000u | (uint32_t)(id))
#endif
#define CDBR_CHUNK_BYTES 128u
#define CDBR_IMAGE_BYTES 16384u
#define CDBR_IMAGE_CRC 0xE7638E98u
#else
#define CDBR_TRANSFER(id) (0xB1CD0000u | (uint32_t)(id))
#define CDBR_CHUNK_BYTES 64u
#define CDBR_IMAGE_BYTES 1024u
#define CDBR_IMAGE_CRC 0x6E99AE3Au
#endif
#define CDBR_WORDS (CDTP_HEADER_WORDS + ((CDGU_SESSION_BYTES + CDBR_CHUNK_BYTES + 3u) / 4u) + 1u)
#define CDBR_PACKETS (CDBR_IMAGE_BYTES / CDBR_CHUNK_BYTES)
#define CDBR_RETRIES 2u
#define CDBR_GAP 12u
#ifdef CUBEDECK_VERDICT_WAIT
#define CDBR_PENDING_POLLS 8u
/* Displayed numeric causes; matching PENDING is a bounded wait, not NACK. */
typedef enum {
    CDBR_RETRY_NONE = 0, CDBR_RETRY_NACK, CDBR_RETRY_VERDICT,
    CDBR_RETRY_ECHO, CDBR_RETRY_PENDING_LIMIT
} cdbr_retry_reason;
#endif
uint8_t cdbr_expected_byte(size_t offset);

typedef enum {
    CDBR_TX_SESSION = 0, CDBR_TX_FRAME, CDBR_TX_DATA,
    CDBR_TX_GAP, CDBR_TX_QUERY, CDBR_TX_DONE, CDBR_TX_FAILED
} cdbr_tx_phase;
typedef struct {
    uint32_t id, words[CDBR_WORDS], last_word, wait_tick;
    size_t count, index;
    unsigned int sequence, accepted, retries, recoveries, awaiting, high_half;
    cdbr_tx_phase phase;
#ifdef CUBEDECK_VERDICT_WAIT
    unsigned int pending_polls, pending_total, last_retry_sequence;
    cdbr_retry_reason last_retry_reason;
    uint32_t last_retry_reply;
#endif
} cdbr_tx;
int cdbr_tx_init(cdbr_tx *tx, uint32_t id);
int cdbr_tx_next(cdbr_tx *tx, uint32_t now, uint32_t *word);
int cdbr_tx_reply(cdbr_tx *tx, uint32_t word, uint32_t now);

typedef struct {
    uint32_t epoch, id, frame_sequence, words[CDBR_WORDS];
    size_t count;
    unsigned int accepted;
    uint8_t image_prefix[CDBR_IMAGE_BYTES - CDBR_CHUNK_BYTES];
} cdbr_work;
typedef struct {
    uint32_t epoch, id, sequence;
    int valid;
    uint32_t image_crc;
    uint8_t payload[CDBR_CHUNK_BYTES];
} cdbr_result;
typedef struct {
    uint32_t epoch, id, frame_sequence, verdict, reply;
    uint32_t words[CDBR_WORDS];
    size_t count;
    unsigned int accepted, forced_nack, pending, armed, exhausted, half_pending;
    uint32_t low_half, image_crc;
    uint8_t image[CDBR_IMAGE_BYTES];
} cdbr_peer;
/* First zero-initialize peer once. RESET/frame restart invalidates worker
 * snapshots. Epoch exhaustion fails closed rather than aliases old work. */
void cdbr_peer_reset(cdbr_peer *peer);
uint32_t cdbr_peer_word(cdbr_peer *peer, uint32_t word);
/* Owner serializes word/reset/take/publish. Validate may run outside IRQ
 * exclusion; publish checks the snapshot's epoch before changing verdict. */
int cdbr_peer_take(cdbr_peer *peer, cdbr_work *work);
void cdbr_peer_validate(const cdbr_work *work, cdbr_result *result);
int cdbr_peer_publish(cdbr_peer *peer, const cdbr_result *result);
#endif
