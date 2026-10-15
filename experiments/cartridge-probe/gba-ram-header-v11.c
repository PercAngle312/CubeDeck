/* Isolated v11 RAM-resident retail cartridge header responder.
 * ROM offsets 0..191 only, aligned halfword loads. No save/storage writes.
 * Loaded through the BIOS multiboot receiver; never boot this template
 * directly from a flashcart. Header commands require an explicit BEGIN. */
#include <gba.h>
#include <stdint.h>
#include "ram_v10_protocol.h"
#include "ram_v11_application.h"
#include "gba_rom_reader.h"
#define CD_JOYCNT (*(volatile uint16_t *)0x04000140u)
#define CD_RECV_LO (*(volatile uint16_t *)0x04000150u)
#define CD_RECV_HI (*(volatile uint16_t *)0x04000152u)
#define CD_SEND_LO (*(volatile uint16_t *)0x04000154u)
#define CD_SEND_HI (*(volatile uint16_t *)0x04000156u)
static cd_v11_application application;
static volatile unsigned progress, terminal_error, ping_pass;
static void publish(uint32_t word) {
    CD_SEND_HI=(uint16_t)(word>>16);
    CD_SEND_LO=(uint16_t)word;
}
static void serial_irq(void) {
    uint16_t flags=CD_JOYCNT;
    if (flags & 1u) {
        /* RESET aborts a live session; it cannot arm another scan. */
        publish(cd_v11_application_reset(&application));
        terminal_error=application.server.failed;
        ping_pass=0u;
    } else if (flags & 2u) {
        uint32_t request=(uint32_t)CD_RECV_LO|((uint32_t)CD_RECV_HI<<16);
        publish(cd_v11_application_reply(&application,request,cd_gba_rom_header_byte,0));
        progress=application.server.offset;
        terminal_error=application.server.failed;
        ping_pass=application.ping_pass;
    }
    CD_JOYCNT=(uint16_t)(0x40u | (flags & 7u));
}
int main(void) {
    unsigned old_color=0xffffu;
    irqInit(); irqEnable(IRQ_VBLANK);
    REG_DISPCNT=MODE_3|BG2_ENABLE;
    irqSet(IRQ_SERIAL,serial_irq);
    REG_RCNT=(uint16_t)((REG_RCNT&0x3fffu)|0xc000u);
    /* Reuse the unchanged uploader's startup marker. The unique D5 BEGIN
     * ACK, not this marker, identifies execution of the v11 application. */
    publish(CD_V10_READY_WORD);
    CD_JOYCNT=0x47u; irqEnable(IRQ_SERIAL);
    for (;;) {
        unsigned color=terminal_error ? RGB5(31,0,0) :
                       progress==CD_V11_HEADER_LENGTH ? RGB5(0,31,0) :
                       ping_pass ? RGB5(20,0,31) : RGB5(31,18,0);
        if (color!=old_color) {
            volatile uint16_t *vram=(volatile uint16_t *)0x06000000u;
            for (unsigned i=0;i<240u*160u;++i) vram[i]=(uint16_t)color;
            old_color=color;
        }
        VBlankIntrWait();
    }
}
