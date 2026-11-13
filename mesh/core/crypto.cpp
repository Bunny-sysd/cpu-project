#include "crypto.h"

#include <string.h>

#include "monocypher.h"
#include "packet.h"

namespace mesh {

void identity_from_secret(Identity& out, const uint8_t secret[KEY_SIZE]) {
    memcpy(out.secret, secret, KEY_SIZE);
    crypto_x25519_public_key(out.pub, out.secret);
    out.id = id_from_public(out.pub);
}

uint32_t id_from_public(const uint8_t pub[KEY_SIZE]) {
    uint8_t h[8];
    crypto_blake2b(h, sizeof h, pub, KEY_SIZE);
    return rd32(h);
}

void fingerprint(const uint8_t pub[KEY_SIZE], char out[17]) {
    static const char hex[] = "0123456789abcdef";
    uint8_t h[8];
    crypto_blake2b(h, sizeof h, pub, KEY_SIZE);
    for (int i = 0; i < 8; i++) {
        out[2 * i]     = hex[h[i] >> 4];
        out[2 * i + 1] = hex[h[i] & 0x0F];
    }
    out[16] = 0;
}

void derive_shared_key(uint8_t key[KEY_SIZE], const Identity& me, const uint8_t their_pub[KEY_SIZE]) {
    // The raw X25519 result should not be used as a key directly. Hash it
    // together with both public keys (in a fixed order so both sides agree).
    uint8_t buf[3 * KEY_SIZE];
    crypto_x25519(buf, me.secret, their_pub);
    bool me_first = memcmp(me.pub, their_pub, KEY_SIZE) < 0;
    memcpy(buf + KEY_SIZE,     me_first ? me.pub : their_pub, KEY_SIZE);
    memcpy(buf + 2 * KEY_SIZE, me_first ? their_pub : me.pub, KEY_SIZE);
    crypto_blake2b(key, KEY_SIZE, buf, sizeof buf);
    crypto_wipe(buf, sizeof buf);
}

// The cipher needs a 24-byte "nonce" that is never used twice with the same
// key. We build it from who is talking to whom, the packet type and the
// random per-message number, so the two directions and message/ack never clash.
static void make_nonce(uint8_t nonce[24], uint8_t type, uint32_t src, uint32_t dst,
                       const uint8_t msg_nonce[MSG_NONCE_SIZE]) {
    memset(nonce, 0, 24);
    wr32(nonce, src);
    wr32(nonce + 4, dst);
    memcpy(nonce + 8, msg_nonce, MSG_NONCE_SIZE);
    nonce[16] = type;
}

// "Additional data": header fields that are sent readable but must not be
// altered. The hop limit and packet id are left out because relays and
// retries legitimately change them.
static void make_ad(uint8_t ad[9], uint8_t type, uint32_t src, uint32_t dst) {
    ad[0] = (uint8_t)((VERSION << 4) | (type & 0x0F));
    wr32(ad + 1, src);
    wr32(ad + 5, dst);
}

void seal(uint8_t* cipher, uint8_t mac[MAC_SIZE], const uint8_t key[KEY_SIZE],
          uint8_t type, uint32_t src, uint32_t dst, const uint8_t msg_nonce[MSG_NONCE_SIZE],
          const uint8_t* plain, size_t len) {
    uint8_t nonce[24], ad[9];
    make_nonce(nonce, type, src, dst, msg_nonce);
    make_ad(ad, type, src, dst);
    crypto_aead_lock(cipher, mac, key, nonce, ad, sizeof ad, plain, len);
}

bool open(uint8_t* plain, const uint8_t mac[MAC_SIZE], const uint8_t key[KEY_SIZE],
          uint8_t type, uint32_t src, uint32_t dst, const uint8_t msg_nonce[MSG_NONCE_SIZE],
          const uint8_t* cipher, size_t len) {
    uint8_t nonce[24], ad[9];
    make_nonce(nonce, type, src, dst, msg_nonce);
    make_ad(ad, type, src, dst);
    return crypto_aead_unlock(plain, mac, key, nonce, ad, sizeof ad, cipher, len) == 0;
}

}  // namespace mesh
