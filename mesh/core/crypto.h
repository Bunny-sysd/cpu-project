// ============================================================================
// crypto.h - identity and end-to-end encryption
//
// PLAIN LANGUAGE:
//   Every node has a KEY PAIR: a secret key it never shares and a public key
//   it tells everyone. Two nodes can each combine "my secret + your public"
//   and arrive at the same shared key, while anyone listening who only has
//   the two public keys cannot (this is X25519 key exchange).
//
//   Messages are then locked with that shared key using an AEAD cipher
//   (XChaCha20-Poly1305). "AEAD" means two things at once:
//     - nobody without the key can read the message, and
//     - if even one bit is changed in transit, unlocking fails.
//   Relay nodes pass the locked bytes along without being able to do either.
//
//   The maths comes from Monocypher (vendor/monocypher), a small audited
//   library. Writing your own cipher for real use is how people get hacked;
//   the toy cipher in the CPU project is for learning and breaking.
//
// KNOWN LIMITS (see docs/SECURITY.md): no forward secrecy, first-seen keys
// are trusted, replay protection only covers recent messages.
// ============================================================================
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace mesh {

const size_t KEY_SIZE       = 32;
const size_t MAC_SIZE       = 16;   // the tamper-check tag added to each message
const size_t MSG_NONCE_SIZE = 8;    // per-message random number, sent in the clear

struct Identity {
    uint8_t  secret[KEY_SIZE];
    uint8_t  pub[KEY_SIZE];
    uint32_t id;                    // short address, derived from pub
};

// Build a full identity from 32 secret random bytes.
void identity_from_secret(Identity& out, const uint8_t secret[KEY_SIZE]);

// A node's 4-byte address is the start of the hash of its public key, so a
// node cannot claim an address without owning a matching key.
uint32_t id_from_public(const uint8_t pub[KEY_SIZE]);

// 16 hex characters (plus the ending 0) that two people can compare out loud
// to be sure they hold each other's real key.
void fingerprint(const uint8_t pub[KEY_SIZE], char out[17]);

// The key two nodes share. Same result on both sides.
void derive_shared_key(uint8_t key[KEY_SIZE], const Identity& me, const uint8_t their_pub[KEY_SIZE]);

// Lock: writes len bytes to cipher and 16 bytes to mac.
void seal(uint8_t* cipher, uint8_t mac[MAC_SIZE], const uint8_t key[KEY_SIZE],
          uint8_t type, uint32_t src, uint32_t dst, const uint8_t msg_nonce[MSG_NONCE_SIZE],
          const uint8_t* plain, size_t len);

// Unlock: returns false (and gives no plaintext) if the key is wrong or
// anything - message, type, sender or receiver - was altered.
bool open(uint8_t* plain, const uint8_t mac[MAC_SIZE], const uint8_t key[KEY_SIZE],
          uint8_t type, uint32_t src, uint32_t dst, const uint8_t msg_nonce[MSG_NONCE_SIZE],
          const uint8_t* cipher, size_t len);

}  // namespace mesh
