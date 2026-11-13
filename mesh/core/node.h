// ============================================================================
// node.h - one mesh node: the brain that runs on every device
//
// PLAIN LANGUAGE:
//   Each device runs one Node. Its whole job fits in four rules:
//
//   1. ANNOUNCE  Every so often, broadcast "my name is X, my public key is Y".
//                Nodes that hear it remember X and can now encrypt to X.
//
//   2. RELAY     When a packet arrives that I have not seen before and that
//                is not only for me, wait a short random time and broadcast
//                it again with the hop limit lowered by one. Every node
//                doing this is called FLOODING: the packet spreads outward
//                like a ripple until it reaches its destination. Remembering
//                packet ids stops the ripple from bouncing around forever.
//
//   3. DELIVER   When an encrypted message addressed to me arrives, unlock
//                it, hand the text to the app, and send back an ACK.
//
//   4. RETRY     After sending a message, wait for the ACK. If none comes,
//                send again, a few times, then report failure.
//
//   Nothing here knows what kind of radio is underneath. No dynamic memory
//   is used (all tables are fixed size), so it behaves the same on a PC and
//   on a small microcontroller.
// ============================================================================
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "crypto.h"
#include "packet.h"
#include "platform.h"

namespace mesh {

const size_t MAX_NAME     = 16;
const size_t MAX_TEXT     = MAX_PAYLOAD - MSG_NONCE_SIZE - MAC_SIZE;   // 217 bytes
const size_t MAX_PEERS    = 16;
const size_t SEEN_SIZE    = 64;   // how many recent packet ids are remembered
const size_t TX_QUEUE     = 8;    // packets waiting for their turn to transmit
const size_t MAX_PENDING  = 4;    // own messages still waiting for an ACK
const size_t RECENT_MSGS  = 8;    // per peer: recent message numbers (stops repeats)

struct Config {
    uint8_t  hop_limit            = 4;                // max relays a packet may use
    uint32_t announce_interval_ms = 15u * 60u * 1000u;
    uint32_t ack_timeout_ms       = 10000;            // wait this long before resending
    uint8_t  max_tries            = 3;                // total sends per message
    uint32_t relay_delay_min_ms   = 50;               // random wait before relaying, so
    uint32_t relay_delay_max_ms   = 400;              // neighbours do not all talk at once
};

// The app (chat page, test, ...) overrides the ones it cares about.
class Events {
public:
    virtual ~Events() {}
    virtual void on_text(uint32_t from, const char* text, size_t len) { (void)from; (void)text; (void)len; }
    virtual void on_delivered(uint32_t to, uint32_t handle) { (void)to; (void)handle; }
    virtual void on_failed(uint32_t to, uint32_t handle) { (void)to; (void)handle; }
    virtual void on_peer(uint32_t id, const char* name) { (void)id; (void)name; }
};

struct Peer {
    bool     used;
    uint32_t id;
    uint8_t  pub[KEY_SIZE];
    uint8_t  key[KEY_SIZE];                        // shared key with this peer
    char     name[MAX_NAME + 1];
    uint8_t  recent[RECENT_MSGS][MSG_NONCE_SIZE];  // message numbers already delivered
    uint8_t  recent_count;
    uint8_t  recent_pos;
};

struct Stats {
    uint32_t sent;            // packets handed to the radio
    uint32_t received;        // new, valid packets heard
    uint32_t relayed;         // packets passed on for others
    uint32_t duplicates;      // packets ignored because already seen
    uint32_t bad_packets;     // not a valid packet at all
    uint32_t bad_auth;        // failed to unlock: wrong key or tampered
    uint32_t bad_announce;    // announce whose address does not match its key
    uint32_t key_conflicts;   // known address announced with a DIFFERENT key
    uint32_t unknown_sender;  // message from a node we have no key for
    uint32_t dropped;         // a table was full
};

enum SendResult { SEND_OK, SEND_UNKNOWN_PEER, SEND_TOO_LONG, SEND_BUSY };

class Node {
public:
    Node(Radio& radio, Platform& platform, Events& events, const Config& config);

    // Call once. secret = 32 random bytes that the device keeps (and stores).
    void begin(const uint8_t secret[KEY_SIZE], const char* name);

    // Call often (every few milliseconds). Does all receiving and sending.
    void tick();

    // Encrypt and send text to a known peer. *handle identifies this message
    // in the later on_delivered / on_failed callback.
    SendResult send_text(uint32_t to, const char* text, size_t len, uint32_t* handle = 0);

    // Broadcast our name and public key now.
    void announce();

    const Identity& identity() const { return me_; }
    const char*     name() const { return name_; }
    const Stats&    stats() const { return stats_; }
    const Peer*     find_peer(uint32_t id) const;
    const Peer*     peer_at(size_t index) const;   // 0..MAX_PEERS-1, null if empty

private:
    struct SeenEntry { uint32_t src, id; };
    struct TxEntry   { bool used; uint32_t send_at; uint8_t len; uint8_t data[MAX_PACKET]; };
    struct Pending {
        bool     used;
        uint32_t dst;
        uint32_t handle;
        uint32_t next_try;
        uint8_t  tries_left;
        uint8_t  payload_len;
        uint8_t  payload[MAX_PAYLOAD];   // message number + ciphertext + mac
    };

    void     handle_packet(const uint8_t* data, size_t len);
    void     handle_announce(const Packet& p);
    void     handle_text(const Packet& p);
    void     handle_ack(const Packet& p);
    void     send_packet(Packet& p, uint32_t delay_ms);
    void     send_pending(Pending& m);
    bool     seen(uint32_t src, uint32_t id) const;
    void     remember(uint32_t src, uint32_t id);
    Peer*    peer(uint32_t id);
    uint32_t random32();

    Radio&    radio_;
    Platform& platform_;
    Events&   events_;
    Config    cfg_;
    Identity  me_;
    char      name_[MAX_NAME + 1];
    bool      started_;
    uint32_t  next_announce_;
    Stats     stats_;
    Peer      peers_[MAX_PEERS];
    SeenEntry seen_[SEEN_SIZE];
    size_t    seen_pos_;
    TxEntry   tx_[TX_QUEUE];
    Pending   pending_[MAX_PENDING];
};

}  // namespace mesh
