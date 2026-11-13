// ============================================================================
// packet.h - what a packet looks like on the air
//
// PLAIN LANGUAGE:
//   A LoRa radio sends at most 255 bytes at a time. Every packet starts with
//   a 14-byte header that any node can read (it has to, to pass it along):
//
//     byte  0      version (top 4 bits) and type (bottom 4 bits)
//     byte  1      hop limit: how many more relays may pass this on
//     bytes 2-5    source node id
//     bytes 6-9    destination node id (FFFFFFFF = everyone)
//     bytes 10-13  packet id: a random number, so nodes can tell
//                  "I have already seen this one" and not relay it twice
//     bytes 14-    payload (encrypted for TEXT - relays cannot read it)
//
//   Multi-byte numbers are stored lowest byte first (little-endian).
// ============================================================================
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace mesh {

const size_t   MAX_PACKET  = 255;
const size_t   HEADER_SIZE = 14;
const size_t   MAX_PAYLOAD = MAX_PACKET - HEADER_SIZE;   // 241
const uint8_t  VERSION     = 1;
const uint32_t BROADCAST   = 0xFFFFFFFFu;

enum PacketType {
    PKT_ANNOUNCE = 1,   // "I exist, here is my name and public key"
    PKT_TEXT     = 2,   // an encrypted chat message
    PKT_ACK      = 3    // "I received your message"
};

struct Packet {
    uint8_t  type;
    uint8_t  hop_limit;
    uint32_t src;
    uint32_t dst;
    uint32_t id;
    uint8_t  payload[MAX_PAYLOAD];
    size_t   payload_len;
};

inline uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
inline void wr32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

// Turn a Packet into bytes. out must hold MAX_PACKET bytes. Returns the length.
size_t encode(const Packet& p, uint8_t* out);

// Turn received bytes into a Packet. Returns false if they are not a valid
// packet (too short, too long, wrong version, unknown type).
bool decode(const uint8_t* data, size_t len, Packet& out);

}  // namespace mesh
