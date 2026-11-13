# Mesh messenger: design

Date: 2026-11-13. Approved by Aaron in chat ("nothing, let's do it").

## Intent

Aaron wants to build text chat that works with no internet provider: each
person connects a phone to a small radio device he built, and the devices
carry messages to each other. He has a cybersecurity background, is new to
hardware, and wants to learn from it. He has no radio hardware yet and a small
budget, and his time with the current assistant ends around 2026-11-16, so the
hard parts must be done and tested now and be straightforward for him and a
less capable assistant to move onto hardware later.

## Goals

- Messages travel hop by hop across devices that cannot reach each other directly.
- Messages are encrypted end to end; relays cannot read or alter them.
- The sender learns whether a message arrived.
- The mesh logic runs unchanged on a PC (for testing) and on a microcontroller.
- A handoff guide explains exactly what remains for hardware.

## Non-goals

- Voice. Long-range radios cannot carry it.
- Compatibility with Meshtastic or other existing networks.
- Province-wide range. Range is however far Aaron's own devices reach.

## Anti-goals

- A simulation that cannot be moved to hardware.
- Home-made cryptography in the real message path.

## Decisions

Aaron's: mesh network; text; phone connects to a device he built; must be
transferable to hardware; a simpler assistant does the transfer.

The builder's (Aaron accepted these as proposed, they were not his ideas):
- Target board: Heltec WiFi LoRa 32 V3.
- Language: C++ for the core, no dynamic memory, so it fits a microcontroller.
- Phone connects over the board's own Wi-Fi to a chat page; no app.
- Own protocol: flooding with hop limit and duplicate suppression,
  per-message ACK with retries.
- Cryptography: Monocypher (X25519, XChaCha20-Poly1305, BLAKE2b).
- Trust model: first-seen key is kept; fingerprints for manual checking.
- Lives in `mesh/` inside the CPU repository.

## Structure

`mesh/core` (portable logic), `mesh/sim` (PC radio and tests), `mesh/vendor`
(Monocypher), later `mesh/firmware` (board-specific radio, Wi-Fi, chat page).

## Left to the builder

Firmware, chat page, key storage, radio tuning. See `mesh/docs/HANDOFF.md`.
Security gaps are listed in `mesh/docs/SECURITY.md`.
