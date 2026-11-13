// ============================================================================
// lora_radio.h - the real radio and clock, for the Heltec V3 board
//
// PLAIN LANGUAGE:
//   This is the hardware twin of sim/world.h. It gives the mesh code the two
//   things it asks for in core/platform.h, but backed by real parts:
//     LoRaRadio   -> the SX1262 LoRa chip on the board (through RadioLib)
//     EspPlatform -> the ESP32's millisecond clock and hardware random source
//
// NOT YET TESTED ON A REAL BOARD. It compiles; that is all that is proven.
// ============================================================================
#pragma once
#include <Arduino.h>
#include <RadioLib.h>

#include "config.h"
#include "platform.h"

// Set the instant the radio chip signals "something happened" (an interrupt).
// Interrupt code must be tiny, so it only raises this flag; the real work
// happens later in receive(). Include this header from main.cpp only.
static volatile bool lora_irq_flag = false;
static void IRAM_ATTR lora_on_irq() { lora_irq_flag = true; }

// Pin names (SS, DIO0, RST_LoRa, BUSY_LoRa) come from the board definition.
// On the V3 the pin the board files call "DIO0" is wired to the radio's DIO1.
class LoRaRadio : public mesh::Radio {
public:
    LoRaRadio() : lora_(new Module(SS, DIO0, RST_LoRa, BUSY_LoRa)) {}

    // Returns true if the radio chip answered and accepted the settings.
    bool begin() {
        int state = lora_.begin(LORA_FREQUENCY_MHZ, LORA_BANDWIDTH_KHZ, LORA_SPREADING,
                                LORA_CODING_RATE, LORA_SYNC_WORD, LORA_TX_POWER_DBM,
                                8, LORA_TCXO_VOLTAGE, false);
        if (state != RADIOLIB_ERR_NONE) {
            Serial.printf("LoRa begin failed, code %d\n", state);
            return false;
        }
        lora_.setDio2AsRfSwitch(true);   // the V3 uses DIO2 to switch the antenna
        lora_.setPacketReceivedAction(lora_on_irq);
        return lora_.startReceive() == RADIOLIB_ERR_NONE;
    }

    bool send(const uint8_t* data, size_t len) override {
        // Blocks until the packet has left the antenna, then listens again.
        int state = lora_.transmit(data, len);
        lora_irq_flag = false;           // the "done sending" signal is not a received packet
        lora_.startReceive();
        return state == RADIOLIB_ERR_NONE;
    }

    size_t receive(uint8_t* buf, size_t cap) override {
        if (!lora_irq_flag) return 0;
        lora_irq_flag = false;
        size_t len = lora_.getPacketLength();
        int state = (len > 0 && len <= cap) ? lora_.readData(buf, len) : RADIOLIB_ERR_PACKET_TOO_LONG;
        lora_.startReceive();
        return state == RADIOLIB_ERR_NONE ? len : 0;   // damaged packets are dropped
    }

private:
    SX1262 lora_;
};

class EspPlatform : public mesh::Platform {
public:
    uint32_t now_ms() override { return millis(); }
    // True hardware randomness (valid while Wi-Fi is on, which it always is here).
    void random_bytes(uint8_t* out, size_t len) override { esp_fill_random(out, len); }
};
