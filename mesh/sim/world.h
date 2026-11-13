// ============================================================================
// world.h - a pretend world for testing the mesh on a PC
//
// PLAIN LANGUAGE:
//   This stands in for the two things real hardware provides: radios and a
//   clock. Nodes are placed at (x, y) positions. When one transmits, every
//   node within `range` hears it a moment later - unless the packet is
//   "lost" (set `loss` above 0 to imitate a bad link). Time is pretend too,
//   so an hour of mesh traffic runs in a fraction of a second.
//
//   The Node code being tested is the SAME code that goes on the boards.
//   Only this file gets swapped for the real radio.
//
//   Not modelled: two packets colliding in the air, and signal strength.
// ============================================================================
#pragma once
#include <cmath>
#include <deque>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "node.h"

namespace sim {

class World;

class SimRadio : public mesh::Radio {
public:
    World* world = nullptr;
    int    index = 0;
    std::deque<std::vector<uint8_t>> inbox;

    bool   send(const uint8_t* data, size_t len) override;
    size_t receive(uint8_t* buf, size_t cap) override {
        if (inbox.empty()) return 0;
        std::vector<uint8_t> p = inbox.front();
        inbox.pop_front();
        size_t n = p.size() < cap ? p.size() : cap;
        std::copy(p.begin(), p.begin() + n, buf);
        return n;
    }
};

class SimPlatform : public mesh::Platform {
public:
    World*       world = nullptr;
    std::mt19937 rng;
    uint32_t now_ms() override;
    void     random_bytes(uint8_t* out, size_t len) override {
        for (size_t i = 0; i < len; i++) out[i] = (uint8_t)rng();
    }
};

// Records everything a node tells its app, so tests can check it.
class Recorder : public mesh::Events {
public:
    struct Text { uint32_t from; std::string text; };
    std::vector<Text>     texts;
    std::vector<uint32_t> delivered;
    std::vector<uint32_t> failed;
    std::vector<uint32_t> peers;

    void on_text(uint32_t from, const char* text, size_t len) override { texts.push_back({from, std::string(text, len)}); }
    void on_delivered(uint32_t, uint32_t handle) override { delivered.push_back(handle); }
    void on_failed(uint32_t, uint32_t handle) override { failed.push_back(handle); }
    void on_peer(uint32_t id, const char*) override { peers.push_back(id); }
};

struct SimNode {
    double      x, y;
    SimRadio    radio;
    SimPlatform platform;
    Recorder    events;
    mesh::Node  node;
    SimNode(const mesh::Config& cfg) : x(0), y(0), node(radio, platform, events, cfg) {}
};

class World {
public:
    uint32_t now   = 0;
    double   range = 1.2;
    double   loss  = 0.0;                  // 0.0 = perfect links, 1.0 = nothing gets through
    std::mt19937 rng{12345};

    std::vector<std::unique_ptr<SimNode>> nodes;

    struct Transmission { uint32_t time; int from; std::vector<uint8_t> bytes; };
    std::vector<Transmission> log;         // everything ever put on the air

    // Optional hooks: watch each transmission / alter a packet in flight.
    std::function<void(const Transmission&)> on_transmit;
    std::function<void(int from, int to, std::vector<uint8_t>& bytes)> tamper;

    // Add a node at (x, y). Its secret key is made from its index, so runs repeat exactly.
    SimNode& add(double x, double y, const std::string& name, const mesh::Config& cfg = mesh::Config()) {
        std::unique_ptr<SimNode> n(new SimNode(cfg));
        int index = (int)nodes.size();
        n->x = x; n->y = y;
        n->radio.world = this;    n->radio.index = index;
        n->platform.world = this; n->platform.rng.seed(1000 + index);
        uint8_t secret[32];
        std::mt19937 keygen(5000 + index);
        for (auto& b : secret) b = (uint8_t)keygen();
        nodes.push_back(std::move(n));
        nodes.back()->node.begin(secret, name.c_str());
        return *nodes.back();
    }

    void transmit(int from, const uint8_t* data, size_t len) {
        Transmission t{now, from, std::vector<uint8_t>(data, data + len)};
        log.push_back(t);
        if (on_transmit) on_transmit(t);
        uint32_t airtime = 50 + 2 * (uint32_t)len;   // rough stand-in for slow LoRa
        std::uniform_real_distribution<double> chance(0.0, 1.0);
        for (int to = 0; to < (int)nodes.size(); to++) {
            if (to == from) continue;
            double dx = nodes[to]->x - nodes[from]->x, dy = nodes[to]->y - nodes[from]->y;
            if (std::sqrt(dx * dx + dy * dy) > range) continue;
            if (chance(rng) < loss) continue;
            std::vector<uint8_t> copy = t.bytes;
            if (tamper) tamper(from, to, copy);
            in_flight.push_back({now + airtime, to, copy});
        }
    }

    // Let `ms` milliseconds of pretend time pass.
    void run(uint32_t ms) {
        uint32_t end = now + ms;
        while (now < end) {
            now += 10;
            for (size_t i = 0; i < in_flight.size();) {
                if (in_flight[i].at <= now) {
                    nodes[in_flight[i].to]->radio.inbox.push_back(in_flight[i].bytes);
                    in_flight.erase(in_flight.begin() + i);
                } else {
                    i++;
                }
            }
            for (auto& n : nodes) n->node.tick();
        }
    }

private:
    struct Arrival { uint32_t at; int to; std::vector<uint8_t> bytes; };
    std::vector<Arrival> in_flight;
};

inline bool SimRadio::send(const uint8_t* data, size_t len) { world->transmit(index, data, len); return true; }
inline uint32_t SimPlatform::now_ms() { return world->now; }

}  // namespace sim
