#include "node.h"

#include <string.h>

namespace mesh {

// "Has time t arrived?" - written this way so it still works when the
// millisecond counter wraps round after about 49 days.
static bool due(uint32_t now, uint32_t t) { return (int32_t)(now - t) >= 0; }

Node::Node(Radio& radio, Platform& platform, Events& events, const Config& config)
    : radio_(radio), platform_(platform), events_(events), cfg_(config),
      started_(false), next_announce_(0), seen_pos_(0) {
    memset(&me_, 0, sizeof me_);
    memset(name_, 0, sizeof name_);
    memset(&stats_, 0, sizeof stats_);
    memset(peers_, 0, sizeof peers_);
    memset(seen_, 0, sizeof seen_);
    memset(tx_, 0, sizeof tx_);
    memset(pending_, 0, sizeof pending_);
}

void Node::begin(const uint8_t secret[KEY_SIZE], const char* name) {
    identity_from_secret(me_, secret);
    strncpy(name_, name, MAX_NAME);
    name_[MAX_NAME] = 0;
    started_ = true;
    // First announce after a short random wait, so devices switched on
    // together do not all transmit in the same instant.
    next_announce_ = platform_.now_ms() + random32() % 2000;
}

uint32_t Node::random32() {
    uint8_t b[4];
    platform_.random_bytes(b, sizeof b);
    return rd32(b);
}

// ---------------------------------------------------------------- tables

bool Node::seen(uint32_t src, uint32_t id) const {
    for (size_t i = 0; i < SEEN_SIZE; i++)
        if (seen_[i].src == src && seen_[i].id == id) return true;
    return false;
}

void Node::remember(uint32_t src, uint32_t id) {
    seen_[seen_pos_].src = src;
    seen_[seen_pos_].id  = id;
    seen_pos_ = (seen_pos_ + 1) % SEEN_SIZE;   // oldest entry gets overwritten
}

Peer* Node::peer(uint32_t id) {
    for (size_t i = 0; i < MAX_PEERS; i++)
        if (peers_[i].used && peers_[i].id == id) return &peers_[i];
    return 0;
}

const Peer* Node::find_peer(uint32_t id) const {
    for (size_t i = 0; i < MAX_PEERS; i++)
        if (peers_[i].used && peers_[i].id == id) return &peers_[i];
    return 0;
}

const Peer* Node::peer_at(size_t index) const {
    return (index < MAX_PEERS && peers_[index].used) ? &peers_[index] : 0;
}

// ---------------------------------------------------------------- sending

// Queue a packet for transmission after delay_ms.
void Node::send_packet(Packet& p, uint32_t delay_ms) {
    remember(p.src, p.id);   // so we ignore it when a neighbour relays it back
    for (size_t i = 0; i < TX_QUEUE; i++) {
        if (!tx_[i].used) {
            tx_[i].used    = true;
            tx_[i].send_at = platform_.now_ms() + delay_ms;
            tx_[i].len     = (uint8_t)encode(p, tx_[i].data);
            return;
        }
    }
    stats_.dropped++;
}

void Node::announce() {
    Packet p;
    p.type      = PKT_ANNOUNCE;
    p.hop_limit = cfg_.hop_limit;
    p.src       = me_.id;
    p.dst       = BROADCAST;
    p.id        = random32();
    size_t n = strlen(name_);
    memcpy(p.payload, me_.pub, KEY_SIZE);
    p.payload[KEY_SIZE] = (uint8_t)n;
    memcpy(p.payload + KEY_SIZE + 1, name_, n);
    p.payload_len = KEY_SIZE + 1 + n;
    send_packet(p, 0);
}

SendResult Node::send_text(uint32_t to, const char* text, size_t len, uint32_t* handle) {
    if (len > MAX_TEXT) return SEND_TOO_LONG;
    Peer* pr = peer(to);
    if (!pr) return SEND_UNKNOWN_PEER;

    Pending* m = 0;
    for (size_t i = 0; i < MAX_PENDING; i++)
        if (!pending_[i].used) { m = &pending_[i]; break; }
    if (!m) return SEND_BUSY;

    // payload = [8-byte message number][locked text][16-byte tamper tag]
    uint8_t* nonce = m->payload;
    platform_.random_bytes(nonce, MSG_NONCE_SIZE);
    seal(m->payload + MSG_NONCE_SIZE, m->payload + MSG_NONCE_SIZE + len, pr->key,
         PKT_TEXT, me_.id, to, nonce, (const uint8_t*)text, len);

    m->used        = true;
    m->dst         = to;
    m->handle      = rd32(nonce);
    m->payload_len = (uint8_t)(MSG_NONCE_SIZE + len + MAC_SIZE);
    m->tries_left  = cfg_.max_tries;
    if (handle) *handle = m->handle;
    send_pending(*m);
    return SEND_OK;
}

// Transmit (or re-transmit) a message that is waiting for its ACK.
void Node::send_pending(Pending& m) {
    Packet p;
    p.type      = PKT_TEXT;
    p.hop_limit = cfg_.hop_limit;
    p.src       = me_.id;
    p.dst       = m.dst;
    p.id        = random32();   // fresh packet id, or relays would ignore the resend
    memcpy(p.payload, m.payload, m.payload_len);
    p.payload_len = m.payload_len;
    send_packet(p, 0);
    m.tries_left--;
    m.next_try = platform_.now_ms() + cfg_.ack_timeout_ms;
}

// ---------------------------------------------------------------- receiving

void Node::handle_packet(const uint8_t* data, size_t len) {
    Packet p;
    if (!decode(data, len, p)) { stats_.bad_packets++; return; }
    if (p.src == me_.id) return;                           // my own packet, echoed back
    if (seen(p.src, p.id)) { stats_.duplicates++; return; }
    remember(p.src, p.id);
    stats_.received++;

    bool for_me = (p.dst == me_.id);
    if (for_me || p.dst == BROADCAST) {
        if (p.type == PKT_ANNOUNCE)  handle_announce(p);
        else if (p.type == PKT_TEXT) handle_text(p);
        else                         handle_ack(p);
    }

    // RELAY: pass on anything that is not only for me, if it has hops left.
    if (!for_me && p.hop_limit > 0) {
        p.hop_limit--;
        uint32_t span = cfg_.relay_delay_max_ms - cfg_.relay_delay_min_ms + 1;
        send_packet(p, cfg_.relay_delay_min_ms + random32() % span);
        stats_.relayed++;
    }
}

void Node::handle_announce(const Packet& p) {
    if (p.payload_len < KEY_SIZE + 1) { stats_.bad_packets++; return; }
    const uint8_t* pub = p.payload;
    size_t n = p.payload[KEY_SIZE];
    if (n > MAX_NAME || p.payload_len != KEY_SIZE + 1 + n) { stats_.bad_packets++; return; }

    // The address must be the hash of the key it comes with.
    if (id_from_public(pub) != p.src) { stats_.bad_announce++; return; }

    Peer* pr = peer(p.src);
    if (pr) {
        // Trust on first use: once we hold a key for an address we never
        // silently replace it. A different key is a red flag (see SECURITY.md).
        if (memcmp(pr->pub, pub, KEY_SIZE) != 0) stats_.key_conflicts++;
        return;
    }
    for (size_t i = 0; i < MAX_PEERS; i++) {
        if (!peers_[i].used) {
            pr = &peers_[i];
            memset(pr, 0, sizeof *pr);
            pr->used = true;
            pr->id   = p.src;
            memcpy(pr->pub, pub, KEY_SIZE);
            memcpy(pr->name, p.payload + KEY_SIZE + 1, n);
            derive_shared_key(pr->key, me_, pub);
            events_.on_peer(pr->id, pr->name);
            return;
        }
    }
    stats_.dropped++;
}

void Node::handle_text(const Packet& p) {
    if (p.dst != me_.id) return;
    if (p.payload_len < MSG_NONCE_SIZE + MAC_SIZE) { stats_.bad_packets++; return; }
    Peer* pr = peer(p.src);
    if (!pr) { stats_.unknown_sender++; return; }

    const uint8_t* nonce  = p.payload;
    const uint8_t* cipher = p.payload + MSG_NONCE_SIZE;
    size_t         len    = p.payload_len - MSG_NONCE_SIZE - MAC_SIZE;
    const uint8_t* mac    = cipher + len;

    char text[MAX_TEXT + 1];
    if (!open((uint8_t*)text, mac, pr->key, PKT_TEXT, p.src, me_.id, nonce, cipher, len)) {
        stats_.bad_auth++;
        return;
    }
    text[len] = 0;

    // A resend (our ACK got lost) carries the same message number: ACK it
    // again but do not show the text twice.
    bool repeat = false;
    for (size_t i = 0; i < pr->recent_count; i++)
        if (memcmp(pr->recent[i], nonce, MSG_NONCE_SIZE) == 0) repeat = true;
    if (!repeat) {
        memcpy(pr->recent[pr->recent_pos], nonce, MSG_NONCE_SIZE);
        pr->recent_pos = (uint8_t)((pr->recent_pos + 1) % RECENT_MSGS);
        if (pr->recent_count < RECENT_MSGS) pr->recent_count++;
        events_.on_text(p.src, text, len);
    }

    // ACK = the message number plus a tag only the real receiver can make.
    Packet a;
    a.type      = PKT_ACK;
    a.hop_limit = cfg_.hop_limit;
    a.src       = me_.id;
    a.dst       = p.src;
    a.id        = random32();
    memcpy(a.payload, nonce, MSG_NONCE_SIZE);
    uint8_t nothing = 0;
    seal(&nothing, a.payload + MSG_NONCE_SIZE, pr->key, PKT_ACK, me_.id, p.src, nonce, &nothing, 0);
    a.payload_len = MSG_NONCE_SIZE + MAC_SIZE;
    send_packet(a, 0);
}

void Node::handle_ack(const Packet& p) {
    if (p.dst != me_.id) return;
    if (p.payload_len != MSG_NONCE_SIZE + MAC_SIZE) { stats_.bad_packets++; return; }
    Peer* pr = peer(p.src);
    if (!pr) { stats_.unknown_sender++; return; }

    const uint8_t* nonce = p.payload;
    uint8_t nothing = 0;
    if (!open(&nothing, p.payload + MSG_NONCE_SIZE, pr->key, PKT_ACK, p.src, me_.id, nonce, &nothing, 0)) {
        stats_.bad_auth++;
        return;
    }
    for (size_t i = 0; i < MAX_PENDING; i++) {
        Pending& m = pending_[i];
        if (m.used && m.dst == p.src && memcmp(m.payload, nonce, MSG_NONCE_SIZE) == 0) {
            m.used = false;
            events_.on_delivered(m.dst, m.handle);
            return;
        }
    }
}

// ---------------------------------------------------------------- main loop

void Node::tick() {
    if (!started_) return;

    // 1. Take in everything the radio has heard.
    uint8_t buf[MAX_PACKET];
    size_t  n;
    while ((n = radio_.receive(buf, sizeof buf)) > 0) handle_packet(buf, n);

    uint32_t now = platform_.now_ms();

    // 2. Resend messages whose ACK never came; give up when out of tries.
    for (size_t i = 0; i < MAX_PENDING; i++) {
        Pending& m = pending_[i];
        if (!m.used || !due(now, m.next_try)) continue;
        if (m.tries_left == 0) {
            m.used = false;
            events_.on_failed(m.dst, m.handle);
        } else {
            send_pending(m);
        }
    }

    // 3. Time to announce again?
    if (due(now, next_announce_)) {
        announce();
        next_announce_ = now + cfg_.announce_interval_ms;
    }

    // 4. Transmit queued packets whose wait is over.
    for (size_t i = 0; i < TX_QUEUE; i++) {
        if (tx_[i].used && due(now, tx_[i].send_at)) {
            if (radio_.send(tx_[i].data, tx_[i].len)) {
                tx_[i].used = false;
                stats_.sent++;
            }
        }
    }
}

}  // namespace mesh
