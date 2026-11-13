# Mesh messenger

Encrypted text chat between small radio devices, with no internet provider.
Each device passes other people's messages along, so a message can reach
someone who is out of direct radio range.

**Status:** the mesh logic and encryption are written and tested on a PC with
simulated radios. It has not run on real hardware yet. The steps to get it
onto boards are in [docs/HANDOFF.md](docs/HANDOFF.md).

## Run it

```
python mesh/build.py           # run the tests
python mesh/build.py --demo    # watch a message hop across four nodes
```

Needs g++ (`winget install BrechtSanders.WinLibs.POSIX.UCRT`, then open a new terminal).

## How it works, in plain language

1. **Identity.** Every device makes a secret key and a matching public key.
   Its 4-byte address is taken from the public key.
2. **Announce.** Every device regularly broadcasts its name and public key.
   Devices that hear it can now encrypt messages to it.
3. **Flooding.** A device that hears a packet it has not seen before waits a
   short random time and broadcasts it again. The packet spreads outward hop
   by hop. Each packet has a hop limit that drops by one per relay, and each
   device remembers recent packet ids, so packets cannot circle forever.
4. **Encryption.** Sender and receiver combine their keys into a shared key
   that only those two can compute. The message is locked with it. Relays
   carry the locked bytes but cannot read or alter them.
5. **Acknowledge and retry.** The receiver sends back a small ACK. If the
   sender hears none it resends, up to three times, then reports failure.

## Where things are

| Path | What it is |
|---|---|
| `core/platform.h` | the two things the mesh needs from a device: a radio and a clock/random source |
| `core/packet.*` | the byte layout of a packet |
| `core/crypto.*` | keys, addresses, locking and unlocking messages |
| `core/node.*` | one mesh node: announce, relay, deliver, retry |
| `sim/world.h` | pretend radios and clock for testing on a PC |
| `sim/tests.cpp` | tests |
| `sim/demo.cpp` | four-node demo |
| `vendor/monocypher/` | the cryptography library (third party, unmodified, v4.0.2) |
| `docs/SECURITY.md` | what the encryption does and does not protect against |
| `docs/HANDOFF.md` | what is left to do, written for whoever continues the work |

Everything in `core/` is the code that will run on the boards. Only `sim/`
gets replaced by a real radio.
