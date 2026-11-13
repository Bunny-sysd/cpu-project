# Security notes

## What is protected

- **Reading.** Message text is encrypted end to end with XChaCha20-Poly1305
  under a key only the two endpoints can compute (X25519). Relays and
  eavesdroppers see ciphertext.
- **Altering.** Any change to the text, the sender, the receiver or the packet
  type makes unlocking fail, and the message is dropped.
- **Fake ACKs.** An ACK carries a tag only the real receiver can produce.
- **Address spoofing by a wrong key.** An announce is rejected unless the
  address equals the hash of the key it carries.

The algorithms come from Monocypher 4.0.2. Do not replace them with
home-made ones.

## What is NOT protected (known gaps)

1. **Who talks to whom.** Source and destination addresses, packet sizes and
   timing are visible to anyone listening.
2. **First contact.** The first key heard for an address is trusted. An
   attacker who is present before the real device is heard can announce their
   own key. Defence today: compare the 16-character fingerprint in person.
3. **Short addresses.** An address is only 32 bits of the key hash. An attacker
   can search for a key whose hash matches someone's address (about 4 billion
   tries). It only helps them against people who have not yet heard the real
   device (see 2), because a known address never has its key replaced; the
   mismatch is counted in `key_conflicts`.
4. **No forward secrecy.** Keys are long-term. If a device's secret key is
   stolen, recorded past messages to and from it can be decrypted.
5. **Replay.** A receiver remembers the last 8 message numbers per peer. An
   attacker who recorded an older message can resend it later and it will be
   shown again as new.
6. **Announces are not signed** and anyone can flood the network with packets
   (denial of service). Radio jamming cannot be prevented by software.
7. **Random numbers.** Security depends on `Platform::random_bytes` being
   truly random on the device. The simulator's version is predictable on
   purpose and must never be used on hardware.

## Good next exercises

- Add a handshake so each conversation gets fresh keys (fixes 4 and 5).
- Sign announces with EdDSA (Monocypher has it).
- Show the fingerprint in the chat page and add a "verified" mark.
