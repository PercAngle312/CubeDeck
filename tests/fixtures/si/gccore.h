#ifndef CUBEDECK_TEST_SI_GCCORE_H
#define CUBEDECK_TEST_SI_GCCORE_H
#include <stdint.h>
typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;
typedef void (*SICallback)(s32,u32);
#define ATTRIBUTE_ALIGN(n) __attribute__((aligned(n)))
u32 SI_Transfer(s32 channel,void *out,u32 out_length,void *in,u32 in_length,
                SICallback callback,u32 delay);
#endif
