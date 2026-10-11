#include <stdbool.h>
#include <stdint.h>
#define ATTRIBUTE_ALIGN(n) __attribute__((aligned(n)))
typedef uint32_t sec_t;
#define FEATURE_MEDIUM_CANWRITE 2u
typedef struct {
    unsigned long ioType, features;
    bool (*startup)(void), (*isInserted)(void);
    bool (*readSectors)(sec_t, sec_t, void *);
    bool (*writeSectors)(sec_t, sec_t, const void *);
    bool (*clearStatus)(void), (*shutdown)(void);
} DISC_INTERFACE;
