#ifndef CUBEDECK_VERIFIED_SESSION_H
#define CUBEDECK_VERIFIED_SESSION_H

#include "cubedeck/chunk_session.h"
#include "cubedeck/image_integrity.h"

/* Host-only application framing above CDTP: declare expected image before data,
 * then prove length and whole-image CRC before marking it complete.
 * No storage I/O; maximum accepted image is the 4096-byte RAM buffer.
 */
#define CDVS_OP_MANIFEST 0x30u
#define CDVS_OP_FINISH   0x31u
#define CDVS_MANIFEST_BYTES 8u

typedef enum {
    CDVS_ACCEPTED = 0,
    CDVS_DUPLICATE = 1,
    CDVS_VERIFIED = 2,
    CDVS_ERR_ARGUMENT = -1,
    CDVS_ERR_STATE = -2,
    CDVS_ERR_SEQUENCE = -3,
    CDVS_ERR_MANIFEST = -4,
    CDVS_ERR_INTEGRITY = -5,
    CDVS_ERR_CAPACITY = -6
} cdvs_result;

typedef struct {
    cdcs_receiver chunks;
    cdim_manifest manifest;
    int has_manifest;
    int verified;
} cdvs_receiver;

void cdvs_init(cdvs_receiver *receiver);
int cdvs_make_manifest(size_t length, uint32_t crc32, cdtp_packet *packet);
int cdvs_make_finish(uint16_t sequence, cdtp_packet *packet);
cdvs_result cdvs_receive(cdvs_receiver *receiver, const cdtp_packet *packet);
#endif
