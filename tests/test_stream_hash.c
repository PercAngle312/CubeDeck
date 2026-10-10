#include "cubedeck/stream_hash.h"
#include "cubedeck/image_integrity.h"
#include <stdio.h>
#include <string.h>

#define BYTES 131077u
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

typedef struct {
    const uint8_t *source;
    size_t length;
    unsigned int reads;
    size_t fail_at;
    int should_fail;
} reader_ctx;

static int source_read(void *context, size_t offset,
                       uint8_t *out, size_t length)
{
    reader_ctx *ctx = (reader_ctx *)context;
    if (offset > ctx->length || length > ctx->length - offset)
        return -1;
    if (ctx->should_fail && offset >= ctx->fail_at)
        return -1;
    memcpy(out, ctx->source + offset, length);
    ++ctx->reads;
    return 0;
}

static int crc_matrix(void)
{
    static uint8_t source[BYTES];
    const size_t sizes[] = {0u, 1u, 2u, 3u, 255u, 256u, 257u,
                            511u, 512u, 513u, 4096u, BYTES};
    reader_ctx context;
    size_t i, j;
    uint32_t crc = 0xDEADBEEFu;
    for (i = 0u; i < BYTES; ++i)
        source[i] = (uint8_t)(i * 37u ^ (i >> 8u));
    memset(&context, 0, sizeof(context));
    context.source = source;
    context.length = BYTES;
    CHECK(cdsh_source_crc32(NULL, &context, 0u, &crc) == -1);
    CHECK(cdsh_source_crc32(source_read, &context, 0u, NULL) == -1);
    CHECK(cdsh_source_crc32(source_read, &context,
                            CDST_MAX_BYTES + 1u, &crc) == -1);
    for (j = 0u; j < sizeof(sizes) / sizeof(sizes[0]); ++j) {
        context.reads = 0u;
        CHECK(cdsh_source_crc32(source_read, &context, sizes[j], &crc) == 0);
        CHECK(crc == cdim_crc32(source, sizes[j]));
        CHECK(context.reads ==
              (sizes[j] + CDTP_MAX_PAYLOAD - 1u) / CDTP_MAX_PAYLOAD);
    }
    context.should_fail = 1;
    context.fail_at = 256u;
    crc = 0xAABBCCDDu;
    CHECK(cdsh_source_crc32(source_read, &context, BYTES, &crc) == -1);
    CHECK(crc == 0xAABBCCDDu);
    return 0;
}

int main(void)
{
    if (crc_matrix())
        return 1;
    puts("Bounded source CRC preflight tests passed.");
    return 0;
}
