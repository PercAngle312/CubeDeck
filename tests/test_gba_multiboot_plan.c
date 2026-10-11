#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "cubedeck/gba_multiboot_plan.h"
static uint8_t image[CD_MB_MAX_IMAGE_BYTES + 16u];
static void set_checksum(void)
{
    uint8_t sum = 0u;
    unsigned int i;
    image[0xB2u] = 0x96u;
    for (i = 0xA0u; i <= 0xBCu; ++i) sum = (uint8_t)(sum - image[i]);
    image[0xBDu] = (uint8_t)(sum - 0x19u);
}
int main(void)
{
    cd_mb_plan plan = {123u, 456u, 789u}, before;
    set_checksum();
    assert(cd_mb_plan_image(image, 0x200u, &plan) == 0);
    assert(plan.image_bytes == 0x200u && plan.payload_bytes == 0x140u);
    assert(plan.halfwords_in_header == 96u);
    assert(cd_mb_plan_image(image, CD_MB_MAX_IMAGE_BYTES, &plan) == 0);
    before = plan;
    assert(cd_mb_plan_image(image, 0xF0u, &plan) == -1);
    assert(cd_mb_plan_image(image, 0x100u, &plan) == -1);
    assert(cd_mb_plan_image(image, 0x101u, &plan) == -1);
    assert(cd_mb_plan_image(image, sizeof(image), &plan) == -1);
    image[0xBDu] ^= 1u;
    assert(cd_mb_plan_image(image, 0x100u, &plan) == -1);
    assert(memcmp(&before, &plan, sizeof(plan)) == 0);
    assert(cd_mb_plan_image(NULL, 0x100u, &plan) == -1);
    assert(cd_mb_plan_image(image, 0x100u, NULL) == -1);
    puts("Multiboot image planning: 0x200..0x3FF40 bounds, alignment and checksum passed.");
    return 0;
}
