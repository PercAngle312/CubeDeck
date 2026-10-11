#include "sd2sp2_spi.h"
#include <cassert>
#include <cstdio>
#include <iostream>
#include <vector>
using cubedeck::SD2SP2SPI;
static void command(SD2SP2SPI& card, unsigned cmd, uint32_t arg, bool bad=false) {
  uint8_t packet[6] = {uint8_t(0x40|cmd),uint8_t(arg>>24),uint8_t(arg>>16),uint8_t(arg>>8),uint8_t(arg),0};
  packet[5] = SD2SP2SPI::crc7(packet,5) ^ (bad ? 2 : 0);
  for (auto b:packet) assert(card.transfer(b)==0xff);
  card.deselect();
}
static uint8_t clock(SD2SP2SPI& c) { return c.transfer(0xff); }
static void initialize(SD2SP2SPI& c) {
  command(c,0,0); assert(clock(c)==1);
  command(c,55,0); assert(clock(c)==1);
  command(c,41,0x40000000); assert(clock(c)==0);
}
static void drain_block(SD2SP2SPI& c) {
  assert(clock(c)==0xfe);
  std::array<uint8_t,512> data{};
  for(unsigned i=0;i<data.size();++i) {
    data[i]=clock(c); assert(data[i]==uint8_t(i));
  }
  uint16_t crc=uint16_t(clock(c))<<8; crc|=clock(c);
  assert(crc==SD2SP2SPI::crc16(data.data(),data.size()));
}
static void interrupted_sessions(const char* path) {
  // Drop CS at every incomplete command length. No command or read may
  // complete until a fresh six-byte command has been submitted.
  const uint8_t packet[6]={0x51,0,0,0,0,1};
  for(unsigned length=1;length<6;++length) {
    SD2SP2SPI c(path); initialize(c);
    const auto before=c.commands;
    for(unsigned i=0;i<length;++i) assert(c.transfer(packet[i])==0xff);
    c.deselect();
    assert(clock(c)==0xff && c.commands==before && c.reads==0);
    command(c,17,0); assert(clock(c)==0); drain_block(c);
    assert(clock(c)==0xff && c.reads==1);
  }
  // RESET must replace pending data at the token, body and CRC boundaries;
  // idle reads must stay idle until a new initialization sequence completes.
  for(unsigned consumed: {0u,1u,257u,513u,514u,515u}) {
    SD2SP2SPI c(path); initialize(c);
    command(c,18,0); assert(clock(c)==0);
    for(unsigned i=0;i<consumed;++i) clock(c);
    command(c,0,0); assert(clock(c)==1); assert(clock(c)==0xff);
    const auto reads=c.reads;
    command(c,17,0); assert(clock(c)==1);
    assert(clock(c)==0xff && c.reads==reads);
    initialize(c);
    command(c,17,1); assert(clock(c)==0); drain_block(c);
  }
  // Reading off the last sector terminates a stream, then a normal read
  // succeeds; stale bytes cannot appear after the error token.
  SD2SP2SPI c(path); initialize(c);
  command(c,18,1); assert(clock(c)==0); drain_block(c);
  assert(clock(c)==0x08);
  for(unsigned i=0;i<1024;++i) assert(clock(c)==0xff);
  assert(c.reads==1);
  command(c,12,0); assert(clock(c)==0xff); assert(clock(c)==0); assert(clock(c)==0xff);
  command(c,17,0); assert(clock(c)==0); drain_block(c);
  assert(c.reads==2);
  // A rejected write must never turn subsequent data into a writable stream.
  for(unsigned cmd: {24u,25u}) {
    command(c,cmd,0); assert(clock(c)==4);
    assert(c.transfer(0xfe)==0xff);
    for(unsigned i=0;i<514;++i) assert(clock(c)==0xff);
    command(c,17,0); assert(clock(c)==0); drain_block(c);
  }
  assert(c.rejected_writes==2);
}
int main() {
  const char* path="/tmp/cubedeck-sd-spi-test.img";
  { std::ofstream f(path,std::ios::binary); std::array<uint8_t,1024> b{};
    for (unsigned i=0;i<b.size();++i) b[i]=uint8_t(i);
    f.write(reinterpret_cast<char*>(b.data()),b.size()); }
  SD2SP2SPI c(path); assert(c.present());
  uint8_t c0[5]={0x40,0,0,0,0}, c8[5]={0x48,0,0,1,0xaa};
  assert(SD2SP2SPI::crc7(c0,5)==0x95 && SD2SP2SPI::crc7(c8,5)==0x87);
  command(c,0,0,true); assert(clock(c)==9);
  command(c,0,0); assert(clock(c)==1);
  command(c,8,0x1aa); assert(clock(c)==1); assert(clock(c)==0); assert(clock(c)==0); assert(clock(c)==1); assert(clock(c)==0xaa);
  command(c,41,0x40000000); assert(clock(c)==5);
  command(c,55,0); assert(clock(c)==1);
  command(c,41,0x40000000); assert(clock(c)==0);
  command(c,58,0); assert(clock(c)==0); assert(clock(c)==0xc0); assert(clock(c)==0xff); assert(clock(c)==0x80); assert(clock(c)==0);
  command(c,59,1); assert(clock(c)==0);
  command(c,17,0,true); assert(clock(c)==8 && c.reads==0);
  command(c,17,1); assert(clock(c)==0); c.deselect(); assert(clock(c)==0xfe);
  std::array<uint8_t,512> data{};
  for (unsigned i=0;i<512;++i) {data[i]=clock(c); assert(data[i]==uint8_t(i));}
  uint16_t crc = uint16_t(clock(c))<<8; crc |= clock(c);
  assert(crc==SD2SP2SPI::crc16(data.data(),512));
  command(c,17,2); assert(clock(c)==0x20);
  command(c,18,0); assert(clock(c)==0);
  for(unsigned block=0;block<2;++block) {
    assert(clock(c)==0xfe);
    for(unsigned i=0;i<512;++i) assert(clock(c)==uint8_t(i));
    clock(c);clock(c);
  }
  command(c,12,0); assert(clock(c)==0xff);assert(clock(c)==0);assert(clock(c)==0xff);
  for (auto cmd:{24,25,32,33,38,42}) {command(c,cmd,0);assert(clock(c)==4);}
  assert(c.rejected_writes==6);
  command(c,55,0);assert(clock(c)==0);
  command(c,13,0);assert(clock(c)==0);assert(clock(c)==0);assert(clock(c)==0xfe);
  for(unsigned i=0;i<64;++i)assert(clock(c)==0);
  assert(clock(c)==0&&clock(c)==0);
  // Incomplete command cannot mutate an image or poison next select.
  c.transfer(0x51); c.transfer(0); c.deselect();
  command(c,17,0);assert(clock(c)==0);
  interrupted_sessions(path);
  SD2SP2SPI missing("/no/such/cubedeck-image"); assert(!missing.present()&&clock(missing)==0xff);
  {std::ifstream f(path,std::ios::binary);std::array<uint8_t,1024>b{};f.read(reinterpret_cast<char*>(b.data()),b.size());for(unsigned i=0;i<b.size();++i)assert(b[i]==uint8_t(i));}
  std::remove(path);
  std::cout << "Read-only SD SPI: init, CRC, select boundaries, LBA bounds, streaming/stop, interrupted sessions and denied writes passed.\n";
}
