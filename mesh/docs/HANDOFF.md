# Handoff: getting the mesh onto real hardware

Written for Aaron and for whichever assistant continues this work. Aaron is
new to hardware; explain steps in plain language.

## State on 2026-11-13

Tested on a PC (`python mesh/build.py`, 66 checks):
- packet format, flooding with hop limit and duplicate suppression
- announces and peer table
- end-to-end encryption, ACKs, retries, failure reporting

Written and compiling for the Heltec V3, but NEVER RUN ON A BOARD
(`mesh/firmware`, build with `python -m platformio run`):
- LoRa radio driver glue (`src/lora_radio.h`)
- key and name storage in flash, Wi-Fi access point, chat page (`src/main.cpp`,
  `src/chat_page.h`)

Expect the first real test to find problems in the firmware folder. That is
normal; the logic in `mesh/core` is the tested part.

## The one rule

Do not change `mesh/core/` to make hardware work. It is tested and portable.
Hardware-specific code belongs in `mesh/firmware/src/`. If `core` truly must
change, run `python mesh/build.py` afterwards and keep all tests passing.

## Hardware to buy

Heltec WiFi LoRa 32 V3 (ESP32-S3 with an SX1262 LoRa radio), about $25 each.
Two boards minimum, three to see a relay hop. Buy the 915 MHz version for
Canada. ALWAYS attach the antenna before powering on: transmitting without
one can damage the radio. You also need a USB-C data cable.

## First test, step by step

1. Edit `mesh/firmware/src/config.h`: change `WIFI_PASSWORD`.
2. Plug in board 1. From `mesh/firmware` run
   `python -m platformio run -t upload`, then
   `python -m platformio device monitor`.
   Expected log: `device node-xxxx address ... fingerprint ...` and
   `radio ready. Join Wi-Fi "mesh-node-xxxx" and open http://192.168.4.1`.
   - `LoRa begin failed, code N`: look N up in RadioLib's status codes. -2
     means the chip did not answer (wrong pins or wrong board); -707 or
     similar points at the TCXO voltage setting.
   - Upload fails: hold the board's PRG button while plugging in USB.
3. Flash board 2 the same way.
4. With both powered, each log should print `found device node-yyyy` within
   about 10 seconds. If not, the radios are not hearing each other: check
   antennas, and that both were flashed with the same `config.h` radio values.
5. Join board 1's Wi-Fi with a phone, open http://192.168.4.1, tap the other
   device's name, send a message. Do the same on a second phone or laptop
   joined to board 2. The sender's bubble should change to "delivered".
6. Relay test with three boards: put the middle one between two that are too
   far apart to hear each other directly. Use distance or walls for this;
   never remove an antenna to shorten the range.

## Things that will differ from the simulation

- Real LoRa is slow. At the default settings a full-size packet takes
  roughly a third of a second on air; at long-range settings, seconds.
  `MESH_ACK_TIMEOUT_MS` in `config.h` is 30 s for that reason.
- `LoRaRadio::send` blocks while transmitting, so the chat page pauses
  briefly during each send or relay.
- Two devices transmitting at once destroy both packets. The simulator does
  not model this. The random relay delay reduces it; if it is still a
  problem, listen before transmitting (RadioLib `scanChannel`).
- Canadian rules for the 902-928 MHz band limit transmit power and how the
  channel is used. The default 10 dBm is modest; read the rules before
  raising it. This has not been checked against the regulations.
- Range depends heavily on antenna height and line of sight.

## Not done

- Setting the device name from the chat page (the firmware reads a stored
  `name` but nothing writes it yet; default is `node-xxxx`).
- Showing name / Wi-Fi details on the board's small OLED screen.
- Chat history is lost on restart.
- The security gaps listed in `SECURITY.md`.
- Battery and sleep handling.

## Facts checked while writing this

- Board definition `heltec_wifi_lora_32_V3` gives LoRa pins SS 8, SCK 9,
  MOSI 10, MISO 11, RESET 12, BUSY 13, and "DIO0" 14 (wired to the SX1262's
  DIO1). The firmware uses those names from the board definition.
- Firmware size: about 36% of flash and 18% of RAM.

## Facts NOT checked

- TCXO voltage 1.8 V and DIO2 as antenna switch are what the V3 is commonly
  configured with; unverified here.
- Whether the web server stays responsive under heavy mesh traffic.
