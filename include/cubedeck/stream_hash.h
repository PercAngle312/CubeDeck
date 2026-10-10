#ifndef CUBEDECK_STREAM_HASH_H
#define CUBEDECK_STREAM_HASH_H

#include "cubedeck/stream_sender.h"

/*
 * Compute whole-image IEEE CRC32 from a caller-controlled, read-only source
 * without allocating an image-sized buffer. No files or cartridge I/O here.
 *
 * The read callback must be deterministic and deliver exactly the requested
 * number of bytes on success. Caller must not modify its source between
 * this preflight pass and subsequent reliable transfer.
 *
 * Returns 0 on success, -1 on invalid arguments or callback failure.
 */
int cdsh_source_crc32(cdtx_source reader, void *context,
                      size_t length, uint32_t *out_crc32);
#endif
