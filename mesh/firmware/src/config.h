// config.h - settings you may want to change before flashing a board.
#pragma once

// ---- Wi-Fi network the board creates for your phone ----
// The network name is "mesh-" plus the device name.
// CHANGE THIS PASSWORD before real use (8 characters or more). Anyone who
// joins this Wi-Fi can read your chats and send messages as you.
#define WIFI_PASSWORD "change-me-please"

// ---- LoRa radio: every device in the mesh must use identical values ----
#define LORA_FREQUENCY_MHZ   915.0   // Canada / USA band. Europe uses 868.0
#define LORA_BANDWIDTH_KHZ   125.0
#define LORA_SPREADING       9       // 7 = fast, short range ... 12 = slow, long range
#define LORA_CODING_RATE     5       // 5 means 4/5
#define LORA_SYNC_WORD       0x4D    // keeps other LoRa networks' packets out
#define LORA_TX_POWER_DBM    10      // modest power; read your country's rules before raising
#define LORA_TCXO_VOLTAGE    1.8     // the Heltec V3 radio's clock needs 1.8 V

// ---- mesh timing for real (slow) radio links ----
#define MESH_ACK_TIMEOUT_MS        30000
#define MESH_RELAY_DELAY_MIN_MS    200
#define MESH_RELAY_DELAY_MAX_MS    2000
#define MESH_ANNOUNCE_INTERVAL_MS  (5UL * 60UL * 1000UL)
