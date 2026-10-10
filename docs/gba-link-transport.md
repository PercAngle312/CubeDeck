# CubeDeck GBA Link Transport — Design Notes

Status: **design only; no working GameCube/GBA link implementation is included yet.**

This document defines the first safe milestone: prove a repeatable bidirectional exchange before attempting ROM transfer, save access, or gameplay.

## Physical interface gate

A normal GBA-to-GBA link cable has GBA plugs on both ends; it does not plug directly into a GameCube controller port. A GameCube needs a Game Boy Advance link cable (the GameCube-side connector/adapter) or a documented custom electrical interface. Do not wire the GBA serial pins directly to a GameCube port based only on pin names. Confirm the exact cable/adapter and protocol before writing hardware-specific code.

The SuperCard occupies the GBA cartridge slot. That means the GBA cannot simultaneously hold a retail cartridge while the SuperCard is installed. A ROM stored on the SuperCard SD card is not automatically visible to the GameCube over the link cable.

## Milestone 1: harmless link probe

The first probe should exchange a small fixed test frame only. It must not write cartridge saves, erase data, or attempt to dump a ROM.

The probe is considered successful only when:
- the GBA program starts reliably;
- the GameCube receiver detects the expected interface state;
- a known test payload is received correctly more than once;
- invalid frames and timeouts are reported rather than hanging the dashboard;
- unplugging/reconnecting the cable does not crash either side.

## Proposed logical frame

This is an application-level proposal, not a claim about the electrical link protocol. The transport adapter must serialize these fields using the confirmed hardware protocol.

| Field | Size | Purpose |
| --- | ---: | --- |
| Magic | 4 bytes | ASCII `CDLK` |
| Version | 1 byte | Protocol version, initially 1 |
| Type | 1 byte | HELLO, HELLO_ACK, PING, PONG, DATA, ACK, ERROR |
| Sequence | 2 bytes | Wraparound packet sequence |
| Length | 2 bytes | Payload length, bounded by implementation |
| Payload | 0–128 bytes | Test data in the first milestone |
| Checksum | 2 bytes | CRC-16 over header and payload |

All multi-byte fields should use a single documented byte order. The receiver must reject an unknown version, excessive length, invalid checksum, and unexpected sequence number.

## Reliability rules

- Use a bounded timeout for every handshake and packet.
- Retry only a small, fixed number of times; then return an error to the UI.
- Acknowledge accepted frames and do not silently treat a timeout as success.
- Keep the UI responsive while waiting for link events.
- Use fixed-size buffers for the initial implementation.
- Never write to cartridge SRAM/Flash/EEPROM during the link-probe milestone.

## Implementation sequence

1. Identify the actual cable/adapter and supported GameCube-side serial interface.
2. Build a GBA probe ROM that can be launched from the user's available homebrew setup.
3. Implement the matching GameCube-side receive/transmit primitive.
4. Verify HELLO/PING/PONG with sequence numbers, checksums, retries, and disconnect handling.
5. Only after the probe is stable, design chunked ROM transfer and validate data integrity independently.
6. Treat save reading/writing as a separate project with backup and read-only-first tests.

## Current repository state

CubeDeck's existing GameCube DOL is a menu/controller diagnostic shell. This document is planning only; it does not add GBA communication or cartridge access. A cloud build and physical-hardware test are required for every new executable milestone.
