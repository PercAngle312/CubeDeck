#ifndef CUBEDECK_HOST_IRQ_H
#define CUBEDECK_HOST_IRQ_H
static inline u32 IRQ_Disable(void) { return 0u; }
static inline void IRQ_Restore(u32 level) { (void)level; }
#endif
