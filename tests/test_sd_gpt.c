#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "sd_gpt.h"
#include "cubedeck/image_integrity.h"
#include "fixtures/sd/gpt_fixture.h"
static uint8_t header[512], table[16384];
static unsigned reads;
static uint32_t fail_lba;
static cd_sd_gpt_work work;
static cd_sd_gpt_volumes volumes;
static void put32(uint8_t *p, uint32_t n)
{ for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(n >> (8 * i)); }
static int read_sector(void *ctx, uint32_t lba, void *buf)
{
    (void)ctx; ++reads;
    assert(((uintptr_t)buf % 32) == 0);
    if (lba == fail_lba) return 0;
    if (lba == 1) memcpy(buf, header, 512);
    else { assert(lba >= 2 && lba < 34); memcpy(buf, table + (lba - 2)*512, 512); }
    return 1;
}
static void header_crc(void)
{
    memset(header + 16, 0, 4);
    put32(header + 16, cdim_crc32(header, 92));
}
static void table_crc(void)
{ put32(header + 88, cdim_crc32(table, 16384)); header_crc(); }
static void reset(void)
{
    memcpy(header, cd_sd_fixture_header, 512);
    memcpy(table, cd_sd_fixture_table, 16384);
    memset(&volumes, 0xaa, sizeof(volumes)); reads = 0; fail_lba = UINT32_MAX;
}
static void fails(int error)
{
    assert(cd_sd_gpt_read(read_sector, NULL, &work, &volumes) == error);
    const uint8_t *p = (const uint8_t *)&volumes;
    for (unsigned i = 0; i < sizeof(volumes); ++i) assert(!p[i]);
}
int main(void)
{
    assert(cdim_crc32((const uint8_t *)"123456789", 9) == 0xcbf43926u);
    reset();
    assert(cd_sd_gpt_read(read_sector, NULL, &work, &volumes) == 0);
    assert(reads == 33 && volumes.entries == 128 && volumes.count == 1);
    assert(volumes.index[0] == 2 && volumes.lba[0] == 4096 && volumes.extent[0] == 1000000);
    for (unsigned i = 1; i < 34; ++i) { reset(); fail_lba = i; fails(-1); }
    reset(); header[0] ^= 1; fails(-2);
    reset(); header[56] ^= 1; fails(-2);
    reset(); put32(header + 12, 91); fails(-2);
    reset(); put32(header + 12, 513); fails(-2);
    reset(); put32(header + 8, 0x20000); header_crc(); fails(-2);
    reset(); put32(header + 24, 2); header_crc(); fails(-2);
    reset(); put32(header + 20, 1); header_crc(); fails(-2);
    reset(); put32(header + 80, 129); header_crc(); fails(-3); assert(reads == 1);
    reset(); put32(header + 80, 0); header_crc(); fails(-3);
    reset(); put32(header + 84, 256); header_crc(); fails(-3);
    reset(); put32(header + 72, UINT32_MAX); put32(header + 76, UINT32_MAX);
    header_crc(); fails(-3); assert(reads == 1);
    reset(); put32(header + 72, 34); header_crc(); fails(-3);
    reset(); put32(header + 40, 33); header_crc(); fails(-3);
    reset(); put32(header + 36, 1); header_crc(); fails(-3);
    reset(); put32(header + 48, 2000000); header_crc(); fails(-3);
    reset(); table[16383] ^= 1; fails(-4);
    reset(); put32(table + 128 + 32, 3000); table_crc(); fails(-5); /* overlap EFI */
    reset(); put32(table + 128 + 40, 4000); table_crc(); fails(-5); /* reversed */
    reset(); put32(table + 128 + 32, 33); table_crc(); fails(-5);
    reset(); put32(table + 128 + 40, 1999968); table_crc(); fails(-5);
    reset(); put32(table + 128 + 44, 1); table_crc(); fails(-5); /* >32-bit */
    reset(); memset(table + 128 + 16, 0, 16); table_crc(); fails(-5);
    reset(); memcpy(table + 128 + 16, table + 16, 16); table_crc(); fails(-5);
    reset(); memset(table, 0, sizeof(table)); table_crc();
    assert(cd_sd_gpt_read(read_sector, NULL, &work, &volumes) == 0 && !volumes.count);
    reset(); assert(cd_sd_gpt_read(NULL, NULL, &work, &volumes) == -2);
    reset(); assert(cd_sd_gpt_read(read_sector, NULL, NULL, &volumes) == -2);
    assert(cd_sd_gpt_read(read_sector, NULL, &work, NULL) == -2);
    puts("GPT fault matrix passed: reference CRCs, 33 I/O interruptions, corruption, bounds, overlaps, atomic publication.");
}
