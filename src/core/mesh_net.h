#pragma once

#include "vos/types.h"
#include "crypto.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <unordered_map>

namespace vos {

// ─── Packet Protocol ─────────────────────────────────────────
// [MAGIC:4][VER:1][TYPE:1][PAYLOAD_LEN:4][PAYLOAD:N][HMAC:32]

constexpr uint32_t MESH_MAGIC   = 0x564F534D; // "VOSM"
constexpr uint8_t  MESH_VERSION = 1;

enum class MeshMsgType : uint8_t {
    DISCOVER    = 0x01,  // Peer discovery broadcast
    DISCOVER_ACK= 0x02,  // Response to discovery
    TEXT_MSG    = 0x10,  // Text message
    FILE_CHUNK  = 0x20,  // File transfer chunk
    FILE_META   = 0x21,  // File transfer metadata
    PING        = 0xF0,
    PONG        = 0xF1,
};

struct MeshPacket {
    uint32_t    magic;
    uint8_t     version;
    MeshMsgType type;
    uint32_t    payload_len;
    ByteBuffer  payload;
    ByteBuffer  hmac;

    // Serialize to wire format
    ByteBuffer serialize() const;

    // Deserialize from wire format
    static Result<MeshPacket> deserialize(const ByteBuffer& data);
};

// ─── Peer Info ───────────────────────────────────────────────
struct MeshPeer {
    std::string peer_id;       // Unique identifier
    std::string address;       // IP:port or BT address
    TimePoint   last_seen;
    bool        connected;
};

// ─── Callbacks ───────────────────────────────────────────────
using MeshMessageFn = std::function<void(const std::string& peer_id, const ByteBuffer& payload)>;
using MeshPeerFn    = std::function<void(const MeshPeer& peer)>;

// ─── Mesh Network Manager ────────────────────────────────────
class MeshNet {
public:
    MeshNet();
    ~MeshNet();

    // Init with a crypto instance for encryption
    Result<void> init(Crypto* crypto, uint16_t port = 5055);
    void shutdown();

    // Discovery
    void start_discovery();
    void stop_discovery();
    std::vector<MeshPeer> get_peers() const;

    // Messaging
    Result<void> send_text(const std::string& peer_id, const std::string& message);
    Result<void> send_file(const std::string& peer_id, const std::string& filename,
                           const ByteBuffer& data);

    // Register callbacks
    void on_message(MeshMessageFn fn);
    void on_peer_found(MeshPeerFn fn);

    bool is_running() const { return m_running.load(); }

    // Get our own peer ID
    std::string get_own_id() const;

private:
    void listener_loop();
    void discovery_loop();
    void handle_packet(const MeshPacket& pkt, const std::string& from_addr);
    MeshPacket create_packet(MeshMsgType type, const ByteBuffer& payload);

    mutable std::mutex    m_mutex;
    std::atomic<bool>     m_running{false};
    std::atomic<bool>     m_discovering{false};
    uint16_t              m_port{5055};
    std::string           m_own_id;

    Crypto*               m_crypto{nullptr};
    ByteBuffer            m_session_key;

    std::thread           m_listener_thread;
    std::thread           m_discovery_thread;

    std::unordered_map<std::string, MeshPeer> m_peers;
    std::vector<MeshMessageFn>  m_msg_callbacks;
    std::vector<MeshPeerFn>     m_peer_callbacks;

#ifdef _WIN32
    uintptr_t m_socket{(uintptr_t)(~0)};
#else
    int m_socket{-1};
#endif
};

} // namespace vos
