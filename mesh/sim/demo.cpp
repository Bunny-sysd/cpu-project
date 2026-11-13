// demo.cpp - watch one encrypted message hop across four nodes.
// Run with:  python mesh/build.py --demo
//
//   alice ---- relay1 ---- relay2 ---- dana
//   Each node only reaches its direct neighbours.

#include <cstdio>
#include <string>

#include "world.h"

using namespace mesh;

int main() {
    sim::World w;
    Config cfg;
    const char* names[] = {"alice", "relay1", "relay2", "dana"};
    for (int i = 0; i < 4; i++) w.add(i, 0, names[i], cfg);
    auto& alice = *w.nodes[0];
    auto& dana  = *w.nodes[3];

    printf("Nodes switch on and announce themselves...\n");
    w.run(10000);
    for (int i = 0; i < 4; i++) {
        char fp[17];
        fingerprint(w.nodes[i]->node.identity().pub, fp);
        printf("  %-7s address %08x  key fingerprint %s\n", names[i],
               (unsigned)w.nodes[i]->node.identity().id, fp);
    }

    std::string text = "hello dana, no internet needed";
    printf("\nalice sends to dana: \"%s\"\n\n", text.c_str());

    w.on_transmit = [&](const sim::World::Transmission& t) {
        Packet p;
        if (!decode(t.bytes.data(), t.bytes.size(), p)) return;
        const char* type = p.type == PKT_TEXT ? "TEXT" : p.type == PKT_ACK ? "ACK " : "ANNC";
        bool origin = (p.src == w.nodes[t.from]->node.identity().id);
        printf("  t=%6.2fs  %-7s %s %s  hops left %d  %3d bytes  payload: ",
               t.time / 1000.0, names[t.from], origin ? "sends " : "relays", type,
               p.hop_limit, (int)t.bytes.size());
        for (size_t i = 0; i < p.payload_len && i < 12; i++) printf("%02x", p.payload[i]);
        printf("...\n");
    };

    alice.node.send_text(dana.node.identity().id, text.c_str(), text.size());
    w.run(6000);

    printf("\n");
    for (int i = 0; i < 4; i++) {
        auto& n = *w.nodes[i];
        if (n.events.texts.empty())
            printf("  %-7s read nothing\n", names[i]);
        else
            printf("  %-7s read: \"%s\"\n", names[i], n.events.texts[0].text.c_str());
    }
    printf("  alice was told: %s\n", alice.events.delivered.empty() ? "NOT delivered" : "delivered");
    return 0;
}
