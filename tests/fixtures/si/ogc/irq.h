#ifndef CUBEDECK_TEST_SI_IRQ_H
#define CUBEDECK_TEST_SI_IRQ_H
#include <stdint.h>
uint32_t IRQ_Disable(void);
void IRQ_Restore(uint32_t state);
#endif
