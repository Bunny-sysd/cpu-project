#include "packet.h"

#include <string.h>

namespace mesh {

size_t encode(const Packet& p, uint8_t* out) {
    out[0] = (uint8_t)((VERSION << 4) | (p.type & 0x0F));
    out[1] = p.hop_limit;
    wr32(out + 2, p.src);
    wr32(out + 6, p.dst);
    wr32(out + 10, p.id);
    memcpy(out + HEADER_SIZE, p.payload, p.payload_len);
    return HEADER_SIZE + p.payload_len;
}

bool decode(const uint8_t* data, size_t len, Packet& out) {
    if (len < HEADER_SIZE || len > MAX_PACKET) return false;
    if ((data[0] >> 4) != VERSION) return false;
    out.type = data[0] & 0x0F;
    if (out.type != PKT_ANNOUNCE && out.type != PKT_TEXT && out.type != PKT_ACK) return false;
    out.hop_limit   = data[1];
    out.src         = rd32(data + 2);
    out.dst         = rd32(data + 6);
    out.id          = rd32(data + 10);
    out.payload_len = len - HEADER_SIZE;
    memcpy(out.payload, data + HEADER_SIZE, out.payload_len);
    return true;
}

}  // namespace mesh
