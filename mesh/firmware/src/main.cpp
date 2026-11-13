// ============================================================================
// main.cpp - firmware for one mesh device (Heltec WiFi LoRa 32 V3)
//
// PLAIN LANGUAGE:
//   What the board does when it has power:
//     1. Loads its secret key from flash memory (makes one on first boot).
//     2. Starts the LoRa radio and the mesh node from mesh/core.
//     3. Creates a Wi-Fi network called "mesh-<name>". Join it with a phone
//        and open http://192.168.4.1 to get the chat page.
//     4. Forever: let the mesh node work, and answer the chat page.
//
// NOT YET TESTED ON A REAL BOARD. It compiles; that is all that is proven.
// ============================================================================
#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

#include "chat_page.h"
#include "config.h"
#include "lora_radio.h"
#include "node.h"

// ---- chat history shown on the page (kept in memory only) ----
enum { ST_SENDING = 0, ST_DELIVERED = 1, ST_FAILED = 2, ST_RECEIVED = 3 };
struct ChatMessage {
    bool     used;
    bool     out;        // true = I sent it
    uint8_t  status;
    uint32_t peer;
    uint32_t handle;
    char     text[mesh::MAX_TEXT + 1];
};
static const size_t HISTORY = 24;
static ChatMessage history[HISTORY];
static size_t      history_next = 0;

static ChatMessage& add_message(bool out, uint32_t peer, uint32_t handle, uint8_t status,
                                const char* text, size_t len) {
    ChatMessage& m = history[history_next];
    history_next = (history_next + 1) % HISTORY;      // oldest is overwritten
    m.used = true; m.out = out; m.peer = peer; m.handle = handle; m.status = status;
    if (len > mesh::MAX_TEXT) len = mesh::MAX_TEXT;
    memcpy(m.text, text, len);
    m.text[len] = 0;
    return m;
}

static void set_status(uint32_t handle, uint8_t status) {
    for (size_t i = 0; i < HISTORY; i++)
        if (history[i].used && history[i].out && history[i].handle == handle) history[i].status = status;
}

// ---- what the mesh node tells us ----
class App : public mesh::Events {
public:
    void on_text(uint32_t from, const char* text, size_t len) override {
        add_message(false, from, 0, ST_RECEIVED, text, len);
        Serial.printf("message from %08x\n", (unsigned)from);
    }
    void on_delivered(uint32_t, uint32_t handle) override { set_status(handle, ST_DELIVERED); }
    void on_failed(uint32_t, uint32_t handle) override { set_status(handle, ST_FAILED); }
    void on_peer(uint32_t id, const char* name) override {
        Serial.printf("found device %s (%08x)\n", name, (unsigned)id);
    }
};

static mesh::Config make_config() {
    mesh::Config c;
    c.ack_timeout_ms       = MESH_ACK_TIMEOUT_MS;
    c.relay_delay_min_ms   = MESH_RELAY_DELAY_MIN_MS;
    c.relay_delay_max_ms   = MESH_RELAY_DELAY_MAX_MS;
    c.announce_interval_ms = MESH_ANNOUNCE_INTERVAL_MS;
    return c;
}

static LoRaRadio   radio;
static EspPlatform platform;
static App         app;
static mesh::Node  node(radio, platform, app, make_config());
static WebServer   server(80);
static bool        radio_ok = false;

// ---- JSON helpers for the chat page ----
static String hex8(uint32_t v) {
    char b[9];
    snprintf(b, sizeof b, "%08x", (unsigned)v);
    return String(b);
}

// Put text inside JSON quotes safely (so a message cannot break the page data).
static String json_string(const char* s) {
    String out = "\"";
    for (; *s; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\') { out += '\\'; out += (char)ch; }
        else if (ch == '\n') out += "\\n";
        else if (ch < 0x20) { char b[7]; snprintf(b, sizeof b, "\\u%04x", ch); out += b; }
        else out += (char)ch;
    }
    return out + "\"";
}

static void handle_state() {
    char fp[17];
    mesh::fingerprint(node.identity().pub, fp);
    String j = "{\"id\":\"" + hex8(node.identity().id) + "\",\"name\":" + json_string(node.name()) +
               ",\"fingerprint\":\"" + fp + "\",\"radio\":" + (radio_ok ? "true" : "false") + ",\"peers\":[";
    bool first = true;
    for (size_t i = 0; i < mesh::MAX_PEERS; i++) {
        const mesh::Peer* p = node.peer_at(i);
        if (!p) continue;
        mesh::fingerprint(p->pub, fp);
        if (!first) j += ',';
        first = false;
        j += "{\"id\":\"" + hex8(p->id) + "\",\"name\":" + json_string(p->name) + ",\"fingerprint\":\"" + fp + "\"}";
    }
    j += "],\"messages\":[";
    first = true;
    for (size_t k = 0; k < HISTORY; k++) {
        const ChatMessage& m = history[(history_next + k) % HISTORY];   // oldest first
        if (!m.used) continue;
        if (!first) j += ',';
        first = false;
        j += "{\"peer\":\"" + hex8(m.peer) + "\",\"out\":" + (m.out ? "true" : "false") +
             ",\"status\":" + String(m.status) + ",\"text\":" + json_string(m.text) + "}";
    }
    j += "]}";
    server.send(200, "application/json", j);
}

static void handle_send() {
    String to = server.arg("to"), text = server.arg("text");
    uint32_t peer = (uint32_t)strtoul(to.c_str(), 0, 16);
    uint32_t handle = 0;
    switch (node.send_text(peer, text.c_str(), text.length(), &handle)) {
        case mesh::SEND_OK:
            add_message(true, peer, handle, ST_SENDING, text.c_str(), text.length());
            server.send(200, "text/plain", "ok");
            break;
        case mesh::SEND_UNKNOWN_PEER: server.send(404, "text/plain", "that device is not known yet"); break;
        case mesh::SEND_TOO_LONG:     server.send(413, "text/plain", "message too long"); break;
        default:                      server.send(503, "text/plain", "still sending earlier messages, try again"); break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);

    // Wi-Fi first: the hardware random source is only trustworthy once a radio is on.
    WiFi.mode(WIFI_AP);

    // Secret key and name live in flash so the device keeps its identity.
    Preferences prefs;
    prefs.begin("mesh", false);
    uint8_t secret[mesh::KEY_SIZE];
    if (prefs.getBytes("secret", secret, sizeof secret) != sizeof secret) {
        esp_fill_random(secret, sizeof secret);
        prefs.putBytes("secret", secret, sizeof secret);
        Serial.println("first boot: made a new secret key");
    }
    String name = prefs.getString("name", "");
    if (name.length() == 0) {
        // Default name: "node-" plus 4 hex digits of the chip's serial number.
        char b[12];
        snprintf(b, sizeof b, "node-%04x", (unsigned)(ESP.getEfuseMac() >> 32) & 0xFFFF);
        name = b;
    }
    prefs.end();

    radio_ok = radio.begin();
    node.begin(secret, name.c_str());
    memset(secret, 0, sizeof secret);

    String ssid = "mesh-" + name;
    WiFi.softAP(ssid.c_str(), WIFI_PASSWORD);

    server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", CHAT_PAGE); });
    server.on("/api/state", HTTP_GET, handle_state);
    server.on("/api/send", HTTP_POST, handle_send);
    server.begin();

    char fp[17];
    mesh::fingerprint(node.identity().pub, fp);
    Serial.printf("device %s  address %08x  fingerprint %s\n", name.c_str(), (unsigned)node.identity().id, fp);
    Serial.printf("radio %s. Join Wi-Fi \"%s\" and open http://%s\n", radio_ok ? "ready" : "FAILED",
                  ssid.c_str(), WiFi.softAPIP().toString().c_str());
}

void loop() {
    if (radio_ok) node.tick();
    server.handleClient();
}
