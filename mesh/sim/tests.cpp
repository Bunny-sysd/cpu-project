// tests.cpp - checks that the mesh does what docs say it does.
// Run with:  python mesh/build.py

#include <cstdio>
#include <cstring>
#include <string>

#include "world.h"

using namespace mesh;
using sim::World;

static int checks = 0, failures = 0;
#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
    printf("    FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

static Config fast_config() {
    Config c;
    c.announce_interval_ms = 60000;
    c.ack_timeout_ms       = 5000;
    return c;
}

// Count how many logged transmissions carry a given packet type.
static int count_type(const World& w, size_t from_index, uint8_t type) {
    int n = 0;
    for (size_t i = from_index; i < w.log.size(); i++)
        if ((w.log[i].bytes[0] & 0x0F) == type) n++;
    return n;
}

// ---------------------------------------------------------------------------

static void test_packet_format() {
    printf("packet: encode / decode\n");
    Packet p;
    p.type = PKT_TEXT; p.hop_limit = 3; p.src = 0x11223344; p.dst = 0xAABBCCDD; p.id = 0xDEADBEEF;
    p.payload_len = 5; memcpy(p.payload, "hello", 5);
    uint8_t buf[MAX_PACKET];
    size_t n = encode(p, buf);
    CHECK(n == HEADER_SIZE + 5);

    Packet q;
    CHECK(decode(buf, n, q));
    CHECK(q.type == PKT_TEXT && q.hop_limit == 3 && q.src == p.src && q.dst == p.dst && q.id == p.id);
    CHECK(q.payload_len == 5 && memcmp(q.payload, "hello", 5) == 0);

    CHECK(!decode(buf, HEADER_SIZE - 1, q));          // too short
    buf[0] = 0x22;  CHECK(!decode(buf, n, q));        // wrong version
    buf[0] = 0x1F;  CHECK(!decode(buf, n, q));        // unknown type
}

static void test_crypto() {
    printf("crypto: shared key, lock / unlock, tamper detection\n");
    uint8_t sa[32], sb[32], sc[32];
    for (int i = 0; i < 32; i++) { sa[i] = (uint8_t)(i + 1); sb[i] = (uint8_t)(i + 101); sc[i] = (uint8_t)(i + 201); }
    Identity a, b, c;
    identity_from_secret(a, sa); identity_from_secret(b, sb); identity_from_secret(c, sc);
    CHECK(a.id != b.id);
    CHECK(a.id == id_from_public(a.pub));

    uint8_t kab[32], kba[32], kcb[32];
    derive_shared_key(kab, a, b.pub);
    derive_shared_key(kba, b, a.pub);
    derive_shared_key(kcb, c, b.pub);
    CHECK(memcmp(kab, kba, 32) == 0);                 // both sides get the same key
    CHECK(memcmp(kab, kcb, 32) != 0);                 // a third node gets a different one

    const char* msg = "meet at noon";
    size_t len = strlen(msg);
    uint8_t nonce[8] = {1, 2, 3, 4, 5, 6, 7, 8}, cipher[64], mac[16], plain[64];
    seal(cipher, mac, kab, PKT_TEXT, a.id, b.id, nonce, (const uint8_t*)msg, len);
    CHECK(memcmp(cipher, msg, len) != 0);             // really scrambled
    CHECK(open(plain, mac, kba, PKT_TEXT, a.id, b.id, nonce, cipher, len));
    CHECK(memcmp(plain, msg, len) == 0);

    CHECK(!open(plain, mac, kcb, PKT_TEXT, a.id, b.id, nonce, cipher, len));   // wrong key
    CHECK(!open(plain, mac, kba, PKT_TEXT, c.id, b.id, nonce, cipher, len));   // forged sender
    CHECK(!open(plain, mac, kba, PKT_ACK,  a.id, b.id, nonce, cipher, len));   // wrong type
    cipher[3] ^= 0x01;
    CHECK(!open(plain, mac, kba, PKT_TEXT, a.id, b.id, nonce, cipher, len));   // one bit flipped
}

static void test_neighbours() {
    printf("mesh: two nodes in range\n");
    World w;
    auto& a = w.add(0, 0, "alice", fast_config());
    auto& b = w.add(1, 0, "bob", fast_config());
    w.run(5000);
    CHECK(a.node.find_peer(b.node.identity().id) != nullptr);
    CHECK(b.node.find_peer(a.node.identity().id) != nullptr);
    CHECK(strcmp(a.node.find_peer(b.node.identity().id)->name, "bob") == 0);

    uint32_t handle = 0;
    CHECK(a.node.send_text(b.node.identity().id, "hi bob", 6, &handle) == SEND_OK);
    w.run(3000);
    CHECK(b.events.texts.size() == 1);
    CHECK(b.events.texts.size() == 1 && b.events.texts[0].text == "hi bob");
    CHECK(b.events.texts.size() == 1 && b.events.texts[0].from == a.node.identity().id);
    CHECK(a.events.delivered.size() == 1 && a.events.delivered[0] == handle);
    CHECK(a.events.failed.empty());
}

static void test_multi_hop() {
    printf("mesh: 4 nodes in a line, message hops A -> B -> C -> D\n");
    World w;
    auto& a = w.add(0, 0, "A", fast_config());
    auto& b = w.add(1, 0, "B", fast_config());
    auto& c = w.add(2, 0, "C", fast_config());
    auto& d = w.add(3, 0, "D", fast_config());
    w.run(10000);
    CHECK(a.node.find_peer(d.node.identity().id) != nullptr);   // heard of D through relays

    size_t mark = w.log.size();
    std::string secret = "the password is swordfish";
    CHECK(a.node.send_text(d.node.identity().id, secret.c_str(), secret.size()) == SEND_OK);
    w.run(8000);

    CHECK(d.events.texts.size() == 1 && d.events.texts[0].text == secret);
    CHECK(a.events.delivered.size() == 1);
    CHECK(b.events.texts.empty() && c.events.texts.empty());    // relays were not shown it
    CHECK(b.node.stats().relayed > 0 && c.node.stats().relayed > 0);

    // Nothing that went over the air contains the readable message.
    bool leaked = false;
    for (size_t i = mark; i < w.log.size(); i++) {
        std::string raw(w.log[i].bytes.begin(), w.log[i].bytes.end());
        if (raw.find("swordfish") != std::string::npos) leaked = true;
    }
    CHECK(!leaked);

    // Flooding stops: each node transmits a given packet at most once.
    CHECK(count_type(w, mark, PKT_TEXT) <= 4);
    CHECK(count_type(w, mark, PKT_ACK) <= 4);
}

static void test_hop_limit() {
    printf("mesh: hop limit stops packets travelling forever\n");
    Config cfg = fast_config();
    cfg.hop_limit = 2;                                // at most 2 relays
    World w;
    auto& a = w.add(0, 0, "A", cfg);
    w.add(1, 0, "B", cfg); w.add(2, 0, "C", cfg);
    auto& d = w.add(3, 0, "D", cfg);
    w.add(4, 0, "E", cfg);
    auto& f = w.add(5, 0, "F", cfg);
    w.run(10000);
    CHECK(a.node.find_peer(d.node.identity().id) != nullptr);   // 2 relays away: reachable
    CHECK(a.node.find_peer(f.node.identity().id) == nullptr);   // 4 relays away: never heard
    CHECK(a.node.send_text(f.node.identity().id, "x", 1) == SEND_UNKNOWN_PEER);

    CHECK(a.node.send_text(d.node.identity().id, "edge", 4) == SEND_OK);
    w.run(8000);
    CHECK(d.events.texts.size() == 1);
    CHECK(a.events.delivered.size() == 1);
}

static void test_grid_flood_is_bounded() {
    printf("mesh: 3x3 grid, flood reaches the far corner and then stops\n");
    World w;
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 3; x++)
            w.add(x, y, "n" + std::to_string(y * 3 + x), fast_config());
    w.run(10000);
    auto& first = *w.nodes[0];
    auto& last  = *w.nodes[8];
    size_t mark = w.log.size();
    CHECK(first.node.send_text(last.node.identity().id, "corner to corner", 16) == SEND_OK);
    w.run(8000);
    CHECK(last.events.texts.size() == 1);
    CHECK(first.events.delivered.size() == 1);
    CHECK(count_type(w, mark, PKT_TEXT) <= 9);        // never more than one send per node
    CHECK(count_type(w, mark, PKT_ACK) <= 9);
}

static void test_lossy_links() {
    printf("mesh: 15%% of packets lost, retries still deliver exactly once\n");
    Config cfg = fast_config();
    cfg.max_tries = 6;
    World w;
    auto& a = w.add(0, 0, "A", cfg);
    w.add(1, 0, "B", cfg);
    auto& c = w.add(2, 0, "C", cfg);
    w.run(10000);                                     // discovery on clean links
    w.loss = 0.15;
    int sent = 0;
    for (int i = 0; i < 5; i++) {
        std::string m = "message " + std::to_string(i);
        if (a.node.send_text(c.node.identity().id, m.c_str(), m.size()) == SEND_OK) sent++;
        w.run(40000);
    }
    CHECK(sent == 5);
    CHECK(c.events.texts.size() == 5);                // all arrived, none shown twice
    CHECK(a.events.delivered.size() == 5);
    CHECK(a.events.failed.empty());
}

static void test_dead_link_reports_failure() {
    printf("mesh: unreachable peer -> sender is told it failed\n");
    World w;
    auto& a = w.add(0, 0, "A", fast_config());
    auto& b = w.add(1, 0, "B", fast_config());
    w.run(5000);
    w.loss = 1.0;                                     // B walks out of range
    size_t mark = w.log.size();
    uint32_t handle = 0;
    CHECK(a.node.send_text(b.node.identity().id, "anyone?", 7, &handle) == SEND_OK);
    w.run(30000);
    CHECK(b.events.texts.empty());
    CHECK(a.events.delivered.empty());
    CHECK(a.events.failed.size() == 1 && a.events.failed[0] == handle);
    CHECK(count_type(w, mark, PKT_TEXT) == 3);        // tried exactly max_tries times
}

static void test_tampering_relay() {
    printf("security: a relay that alters the message is detected\n");
    World w;
    auto& a = w.add(0, 0, "A", fast_config());
    w.add(1, 0, "B", fast_config());
    auto& c = w.add(2, 0, "C", fast_config());
    w.run(10000);
    // Node 1 (B) is malicious: it flips one bit in every TEXT it passes on.
    w.tamper = [](int from, int, std::vector<uint8_t>& bytes) {
        if (from == 1 && (bytes[0] & 0x0F) == PKT_TEXT) bytes[HEADER_SIZE + 10] ^= 0x01;
    };
    CHECK(a.node.send_text(c.node.identity().id, "pay bob 10", 10) == SEND_OK);
    w.run(30000);
    CHECK(c.events.texts.empty());                    // the altered text is never shown
    CHECK(c.node.stats().bad_auth > 0);
    CHECK(a.events.failed.size() == 1);
}

static void test_forged_announce() {
    printf("security: announce with an address that does not match its key is rejected\n");
    World w;
    auto& a = w.add(0, 0, "A", fast_config());
    auto& b = w.add(1, 0, "B", fast_config());
    w.run(5000);
    // An attacker announces B's address with the attacker's own key.
    uint8_t evil_secret[32];
    memset(evil_secret, 0x66, sizeof evil_secret);
    Identity evil;
    identity_from_secret(evil, evil_secret);
    Packet p;
    p.type = PKT_ANNOUNCE; p.hop_limit = 0; p.src = b.node.identity().id; p.dst = BROADCAST; p.id = 777;
    memcpy(p.payload, evil.pub, 32);
    p.payload[32] = 3; memcpy(p.payload + 33, "bob", 3);
    p.payload_len = 36;
    uint8_t buf[MAX_PACKET];
    size_t n = encode(p, buf);
    a.radio.inbox.push_back(std::vector<uint8_t>(buf, buf + n));
    w.run(100);
    CHECK(a.node.stats().bad_announce == 1);
    CHECK(memcmp(a.node.find_peer(b.node.identity().id)->pub, b.node.identity().pub, 32) == 0);
}

static void test_limits() {
    printf("limits: too-long text and garbage packets\n");
    World w;
    auto& a = w.add(0, 0, "A", fast_config());
    auto& b = w.add(1, 0, "B", fast_config());
    w.run(5000);
    std::string big(MAX_TEXT + 1, 'x'), max(MAX_TEXT, 'y');
    CHECK(a.node.send_text(b.node.identity().id, big.c_str(), big.size()) == SEND_TOO_LONG);
    CHECK(a.node.send_text(b.node.identity().id, max.c_str(), max.size()) == SEND_OK);
    w.run(5000);
    CHECK(b.events.texts.size() == 1 && b.events.texts[0].text == max);

    uint8_t junk[40];
    memset(junk, 0xEE, sizeof junk);
    b.radio.inbox.push_back(std::vector<uint8_t>(junk, junk + sizeof junk));
    b.radio.inbox.push_back(std::vector<uint8_t>(junk, junk + 3));
    w.run(100);
    CHECK(b.node.stats().bad_packets == 2);
}

int main() {
    test_packet_format();
    test_crypto();
    test_neighbours();
    test_multi_hop();
    test_hop_limit();
    test_grid_flood_is_bounded();
    test_lossy_links();
    test_dead_link_reports_failure();
    test_tampering_relay();
    test_forged_announce();
    test_limits();
    printf("\n%d checks, %d failed\n", checks, failures);
    printf(failures == 0 ? "ALL MESH TESTS PASSED\n" : "MESH TESTS FAILED\n");
    return failures == 0 ? 0 : 1;
}
