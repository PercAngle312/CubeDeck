// SPDX-License-Identifier: MIT
// CubeDeck test-only read-only SDHC SPI model. Not an SD timing simulator.
#pragma once
#include <array>
#include <cstdint>
#include <deque>
#include <fstream>
#include <string>

namespace cubedeck {
class SD2SP2SPI {
public:
  explicit SD2SP2SPI(const std::string& path) : image(path, std::ios::binary) {
    if (image) {
      image.seekg(0, std::ios::end);
      const auto size = image.tellg();
      if (size >= 512 && (static_cast<uint64_t>(size) % 512) == 0 &&
          static_cast<uint64_t>(size) / 512 <= UINT32_MAX)
        sectors = static_cast<uint32_t>(size / 512);
      image.seekg(0);
    }
  }
  bool present() const { return sectors != 0; }
  uint64_t commands = 0, reads = 0, rejected_writes = 0;
  uint8_t last_command = 0;
  uint32_t last_argument = 0;
  static uint8_t crc7(const uint8_t* data, size_t size) {
    uint8_t crc = 0;
    for (size_t i = 0; i < size; ++i) {
      uint8_t v = data[i];
      for (unsigned b = 0; b < 8; ++b) {
        crc <<= 1;
        if ((v ^ crc) & 0x80) crc ^= 9;
        v <<= 1;
      }
    }
    return static_cast<uint8_t>((crc << 1) | 1);
  }
  static uint16_t crc16(const uint8_t* data, size_t size) {
    uint16_t crc = 0;
    for (size_t i = 0; i < size; ++i) {
      crc ^= static_cast<uint16_t>(data[i]) << 8;
      for (unsigned b = 0; b < 8; ++b)
        crc = static_cast<uint16_t>((crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0));
    }
    return crc;
  }
  // libogc deselects between command, R1 and data: preserve queued replies.
  // Only an incomplete input packet is discarded when chip select drops.
  void deselect() { packet_size = 0; }
  uint8_t transfer(uint8_t input) {
    if (!present()) return 0xff;
    if (packet_size) {
      packet[packet_size++] = input;
      if (packet_size == packet.size()) { packet_size = 0; execute(); }
      return 0xff;
    }
    if ((input & 0xc0) == 0x40) {
      // A new command, notably CMD12, interrupts queued multi-block data.
      output.clear();
      packet[0] = input;
      packet_size = 1;
      return 0xff;
    }
    if (output.empty() && streaming) {
      if (!read_block(next_sector++)) streaming = false;
    }
    if (output.empty()) return 0xff;
    const auto reply = output.front();
    output.pop_front();
    return reply;
  }
private:
  std::ifstream image; // No write handle exists.
  uint32_t sectors = 0, next_sector = 0;
  bool idle = true, app = false, streaming = false, check_crc = false;
  std::array<uint8_t, 6> packet{};
  size_t packet_size = 0;
  std::deque<uint8_t> output;
  void data(const uint8_t* bytes, size_t size) {
    output.push_back(0xfe);
    for (size_t i = 0; i < size; ++i) output.push_back(bytes[i]);
    const auto crc = crc16(bytes, size);
    output.push_back(static_cast<uint8_t>(crc >> 8));
    output.push_back(static_cast<uint8_t>(crc));
  }
  bool read_block(uint32_t lba) {
    std::array<uint8_t, 512> bytes{};
    if (lba >= sectors) { output.push_back(0x08); return false; }
    image.clear();
    image.seekg(static_cast<uint64_t>(lba) * 512);
    image.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    if (!image) { output.push_back(0x01); return false; }
    ++reads;
    data(bytes.data(), bytes.size());
    return true;
  }
  void execute() {
    ++commands;
    last_command = packet[0] & 0x3f;
    last_argument = (uint32_t(packet[1]) << 24) | (uint32_t(packet[2]) << 16) |
                    (uint32_t(packet[3]) << 8) | packet[4];
    const bool application = app;
    app = false;
    streaming = false;
    const uint8_t r1 = idle ? 1 : 0;
    if (((packet[5] & 1) == 0) ||
        ((check_crc || last_command == 0 || last_command == 8) &&
         crc7(packet.data(), 5) != packet[5])) {
      output.push_back(r1 | 0x08); return;
    }
    switch (last_command) {
    case 0: idle = true; check_crc = false; output.push_back(1); break;
    case 8:
      output.push_back(r1);
      output.push_back(0); output.push_back(0);
      output.push_back(1); output.push_back(0xaa); break;
    case 55: app = true; output.push_back(r1); break;
    case 41:
      if (!application) { output.push_back(r1 | 4); break; }
      idle = false; output.push_back(0); break;
    case 58:
      output.push_back(r1);
      output.push_back(idle ? 0x40 : 0xc0);
      output.push_back(0xff); output.push_back(0x80); output.push_back(0); break;
    case 59: check_crc = (last_argument & 1) != 0; output.push_back(r1); break;
    case 16: output.push_back(last_argument == 512 || last_argument == 64 ? r1 : r1 | 0x40); break;
    case 9: {
      std::array<uint8_t, 16> csd{};
      csd[0] = 0x40; csd[5] = 9;
      const uint32_t size = sectors >= 1024 ? sectors / 1024 - 1 : 0;
      csd[7] = (size >> 16) & 0x3f; csd[8] = (size >> 8) & 0xff; csd[9] = size & 0xff;
      csd[12] = 2; csd[13] = 0x40;
      csd[15] = crc7(csd.data(), 15);
      output.push_back(r1); data(csd.data(), csd.size()); break;
    }
    case 10: {
      std::array<uint8_t, 16> cid{0x7f,'C','D','T','E','S','T','0',0x10,0,0,0,1,0,0x11,0};
      cid[15] = crc7(cid.data(), 15);
      output.push_back(r1); data(cid.data(), cid.size()); break;
    }
    case 13:
      output.push_back(r1); output.push_back(0);
      if (application) { std::array<uint8_t, 64> status{}; data(status.data(), status.size()); }
      break;
    case 17: case 18:
      if (idle) { output.push_back(1); break; }
      if (last_argument >= sectors) { output.push_back(0x20); break; }
      output.push_back(0);
      if (last_command == 17) read_block(last_argument);
      else { streaming = true; next_sector = last_argument; }
      break;
    case 12: output.push_back(0xff); output.push_back(r1); output.push_back(0xff); break;
    case 24: case 25: case 32: case 33: case 38: case 42:
      ++rejected_writes; output.push_back(r1 | 4); break;
    default: output.push_back(r1 | 4); break;
    }
  }
};
} // namespace cubedeck
