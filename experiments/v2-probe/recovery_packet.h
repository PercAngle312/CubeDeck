#ifndef CUBEDECK_RECOVERY_PACKET_H
#define CUBEDECK_RECOVERY_PACKET_H
#include "cubedeck/session_guard.h"

/* Synthetic three-packet fixture, not the bulk transport wire API.
 * Frame words travel as tagged low/high halves, so payload/CRC bits cannot
 * alias a session/restart/query marker. IDs are
 * 16-bit nonzero, never reused within one running GameCube process. */
#define CDRP_READY 0x43525059u
#define CDRP_XOR 0xA55A3CC3u
#define CDRP_SESSION 0x53000000u
#define CDRP_FRAME 0x54000000u
#define CDRP_LOW 0x56000000u
#define CDRP_HIGH 0x57000000u
#define CDRP_QUERY 0x7B000000u
#define CDRP_ACK 0xAC000000u
#define CDRP_NACK 0xBA000000u
#define CDRP_PENDING 0xDE000000u
#define CDRP_KEY(id, seq) (((uint32_t)(id) << 8) | (uint32_t)(seq))
#define CDRP_TRANSFER(id) (0x52CD0000u | (uint32_t)(id))
#define CDRP_WORDS 7u
#define CDRP_PACKETS 3u
#define CDRP_RETRIES 2u
#define CDRP_GAP 12u

typedef enum {
    CDRP_TX_SESSION = 0, CDRP_TX_FRAME, CDRP_TX_DATA,
    CDRP_TX_GAP, CDRP_TX_QUERY, CDRP_TX_DONE, CDRP_TX_FAILED
} cdrp_tx_phase;
typedef struct {
    uint32_t id, words[CDRP_WORDS], last_word, wait_tick;
    size_t count, index;
    unsigned int sequence, accepted, retries, recoveries, awaiting, high_half;
    cdrp_tx_phase phase;
} cdrp_tx;
int cdrp_tx_init(cdrp_tx *tx, uint32_t id);
int cdrp_tx_next(cdrp_tx *tx, uint32_t now, uint32_t *word);
int cdrp_tx_reply(cdrp_tx *tx, uint32_t word, uint32_t now);

typedef struct {
    uint32_t epoch, id, frame_sequence, words[CDRP_WORDS];
    size_t count;
} cdrp_work;
typedef struct {
    uint32_t epoch, id, sequence;
    int valid;
} cdrp_result;
typedef struct {
    uint32_t epoch, id, frame_sequence, verdict, reply;
    uint32_t words[CDRP_WORDS];
    size_t count;
    unsigned int accepted, forced_nack, pending, armed, exhausted, half_pending;
    uint32_t low_half;
} cdrp_peer;
/* First zero-initialize peer once. RESET/frame restart invalidates worker
 * snapshots. Epoch exhaustion fails closed rather than aliases old work. */
void cdrp_peer_reset(cdrp_peer *peer);
uint32_t cdrp_peer_word(cdrp_peer *peer, uint32_t word);
/* Owner serializes word/reset/take/publish. Validate may run outside IRQ
 * exclusion; publish checks the snapshot's epoch before changing verdict. */
int cdrp_peer_take(cdrp_peer *peer, cdrp_work *work);
void cdrp_peer_validate(const cdrp_work *work, cdrp_result *result);
int cdrp_peer_publish(cdrp_peer *peer, const cdrp_result *result);
#endif
