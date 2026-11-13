// ============================================================================
// platform.h - the only two things the mesh code needs from the outside world
//
// PLAIN LANGUAGE:
//   The mesh logic (routing, encryption) does not care what it runs on. It
//   only needs someone to hand it:
//     1. a RADIO    - "send these bytes" / "did any bytes arrive?"
//     2. a PLATFORM - "what time is it?" / "give me random bytes"
//
//   On the PC, sim/world.h provides pretend versions of both.
//   On a real board, firmware/ provides versions backed by the LoRa chip.
//   Everything in core/ stays exactly the same in both cases - that is what
//   makes the simulation transferable to hardware.
// ============================================================================
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace mesh {

class Radio {
public:
    virtual ~Radio() {}
    // Broadcast these bytes to whoever is in range. false = radio busy, try later.
    virtual bool send(const uint8_t* data, size_t len) = 0;
    // Copy one received packet into buf and return its length, or 0 if none.
    virtual size_t receive(uint8_t* buf, size_t cap) = 0;
};

class Platform {
public:
    virtual ~Platform() {}
    // Milliseconds since power-on. Allowed to wrap round.
    virtual uint32_t now_ms() = 0;
    // Fill out with random bytes. On real hardware this MUST be a true random
    // source (the ESP32 has one); message security depends on it.
    virtual void random_bytes(uint8_t* out, size_t len) = 0;
};

}  // namespace mesh
