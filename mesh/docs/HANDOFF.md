# Handoff: getting the mesh onto real hardware

Written for Aaron and for whichever assistant continues this work. Aaron is
new to hardware; explain steps in plain language.

## State on 2026-11-13

Done and tested on a PC (`python mesh/build.py`, 66 checks):
- packet format, flooding with hop limit and duplicate suppression
- announces and peer table
- end-to-end encryption, ACKs, retries, failure reporting

Not done:
- firmware for a board (nothing has run on hardware)
- the chat page a phone connects to
- storing the device's secret key across restarts

## The one rule

Do not change `mesh/core/` to make hardware work. It is tested and portable:
C++ with no dynamic memory, no exceptions, no OS calls. Hardware support
means writing two small classes that implement `core/platform.h`:

- `Radio`: `send(bytes)` and `receive(buffer)`
- `Platform`: `now_ms()` and `random_bytes()`

`sim/world.h` is the reference example of both.

## Suggested hardware

Heltec WiFi LoRa 32 V3 (ESP32-S3 with an SX1262 LoRa radio), about $25 each.
Two boards minimum, three to see a relay hop. Buy the 915 MHz version for
Canada, and always attach the antenna before powering on (transmitting
without one can damage the radio).

## Steps

1. Install PlatformIO (`pip install platformio`). Create `mesh/firmware/`
   with board `heltec_wifi_lora_32_V3`, framework `arduino`, library
   `jgromes/RadioLib`.
2. Make `core/` and `vendor/monocypher/` visible to the firmware build
   (PlatformIO `lib_extra_dirs` or `build_src_filter`). `monocypher.c` must
   be compiled as C.
3. Write `LoRaRadio : mesh::Radio` using RadioLib's `SX1262`:
   - `send` -> `radio.transmit(data, len)`, then go back to receive mode
   - `receive` -> if a packet-received flag is set, `radio.readData`
   - Pin numbers believed correct for the V3 but NOT verified here:
     NSS 8, DIO1 14, RESET 12, BUSY 13. Check the Heltec pinout.
   - Starting radio settings: 915.0 MHz, bandwidth 125 kHz, spreading factor
     9, coding rate 4/5. All devices must use identical settings.
4. Write `EspPlatform : mesh::Platform`: `millis()` and `esp_fill_random()`.
5. On first boot make a 32-byte secret with `esp_fill_random`, store it with
   the `Preferences` library, and reuse it on later boots.
6. In `loop()`, call `node.tick()` continuously.
7. Test with two boards on a desk: each should list the other as a peer
   within a few seconds, then exchange a message.
8. Chat page: `WiFi.softAP("mesh-<name>")`, a small web server that serves
   one HTML page, lists peers, sends via `node.send_text`, and shows
   `on_text` / `on_delivered` / `on_failed` events.

## Things that will differ from the simulation

- Real LoRa is slow. One packet can take over a second at long-range
  settings. `ack_timeout_ms` must be raised to match (start with 30 s).
- Two devices transmitting at once destroy both packets. The simulator does
  not model this; the random relay delay reduces it but does not remove it.
  If it becomes a problem, listen before transmitting (RadioLib `scanChannel`).
- Canadian rules for the 902-928 MHz band limit transmit power; keep the
  default power unless you have read the regulations.
- Range depends heavily on antenna height and line of sight.

## Known security gaps

See `SECURITY.md`. None of them block a first hardware test.
