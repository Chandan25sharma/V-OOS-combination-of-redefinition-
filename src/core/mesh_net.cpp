#include "mesh_net.h"
#include "vos/log.h"
#include <cstring>
#include <cstdlib>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#define VOS_INVALID_SOCKET INVALID_SOCKET
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#define closesocket close
#define VOS_INVALID_SOCKET -1
#endif

namespace vos {

static const char* TAG = "MeshNet";

// ─── Helper: portable inet_pton wrapper ──────────────────────
static void vos_inet_pton(const char* cp, void* addr) {
#ifdef _WIN32
    InetPtonA(AF_INET, cp, addr);
#else
    inet_pton(AF_INET, cp, addr);
#endif
}

// ─── MeshPacket ──────────────────────────────────────────────

ByteBuffer MeshPacket::serialize() const {
    size_t total = 4 + 1 + 1 + 4 + payload.size() + hmac.size();
    ByteBuffer buf(total);
    uint8_t* p = buf.data();

    std::memcpy(p, &magic, 4);       p += 4;
    *p++ = version;
    *p++ = static_cast<uint8_t>(type);
    std::memcpy(p, &payload_len, 4); p += 4;

    if (!payload.empty()) {
        std::memcpy(p, payload.data(), payload.size());
        p += payload.size();
    }
    if (!hmac.empty()) {
        std::memcpy(p, hmac.data(), hmac.size());
    }
    return buf;
}

Result<MeshPacket> MeshPacket::deserialize(const ByteBuffer& data) {
    if (data.size() < 10)
        return Result<MeshPacket>::error(StatusCode::ERR_INVALID_ARG);

    const uint8_t* p = data.data();
    MeshPacket pkt;

    std::memcpy(&pkt.magic, p, 4); p += 4;
    if (pkt.magic != MESH_MAGIC)
        return Result<MeshPacket>::error(StatusCode::ERR_INVALID_ARG);

    pkt.version = *p++;
    pkt.type    = static_cast<MeshMsgType>(*p++);
    std::memcpy(&pkt.payload_len, p, 4); p += 4;

    if (data.size() < 10 + pkt.payload_len)
        return Result<MeshPacket>::error(StatusCode::ERR_INVALID_ARG);

    pkt.payload.assign(p, p + pkt.payload_len);
    p += pkt.payload_len;

    size_t remaining = data.size() - (size_t)(10 + pkt.payload_len);
    if (remaining > 0) {
        pkt.hmac.assign(p, p + remaining);
    }
    return Result<MeshPacket>::success(std::move(pkt));
}

// ─── MeshNet ─────────────────────────────────────────────────

MeshNet::MeshNet() {
    // Generate a random peer ID
    std::srand((unsigned)std::time(nullptr));
    m_own_id = "PEER_" + std::to_string(std::rand() % 100000);
}

MeshNet::~MeshNet() {
    shutdown();
}

Result<void> MeshNet::init(Crypto* crypto, uint16_t port) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_running.load()) return Result<void>::error(StatusCode::ERR_ALREADY_EXISTS);

    m_crypto      = crypto;
    m_port        = port;
    m_session_key = m_crypto->generate_key();

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        log::error(TAG, "WSAStartup failed");
        return Result<void>::error(StatusCode::ERR_INTERNAL);
    }
#endif

    // Create UDP socket
    m_socket = (uintptr_t)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if ((intptr_t)m_socket < 0) {
        log::error(TAG, "Failed to create socket");
        return Result<void>::error(StatusCode::ERR_NETWORK);
    }

    // Allow address reuse
    int optval = 1;
    setsockopt((int)m_socket, SOL_SOCKET, SO_REUSEADDR,
               (const char*)&optval, sizeof(optval));

    // Enable broadcast
    int bcast = 1;
    setsockopt((int)m_socket, SOL_SOCKET, SO_BROADCAST,
               (const char*)&bcast, sizeof(bcast));

    // Set non-blocking with timeout for clean shutdown
#ifdef _WIN32
    DWORD timeout = 1000;
    setsockopt((int)m_socket, SOL_SOCKET, SO_RCVTIMEO,
               (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec  = 1;
    tv.tv_usec = 0;
    setsockopt((int)m_socket, SOL_SOCKET, SO_RCVTIMEO,
               (const char*)&tv, sizeof(tv));
#endif

    // Bind
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(m_port);

    if (bind((int)m_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        log::error(TAG, "Bind to port %u failed", m_port);
        closesocket((int)m_socket);
        return Result<void>::error(StatusCode::ERR_NETWORK);
    }

    m_running.store(true);
    m_listener_thread = std::thread(&MeshNet::listener_loop, this);

    log::info(TAG, "Mesh network started on port %u  |  PeerID: %s",
              m_port, m_own_id.c_str());
    return Result<void>::success();
}

void MeshNet::shutdown() {
    if (!m_running.load()) return;

    m_running.store(false);
    m_discovering.store(false);

    if ((intptr_t)m_socket >= 0) {
        closesocket((int)m_socket);
        m_socket = (uintptr_t)-1;
    }

    if (m_listener_thread.joinable())   m_listener_thread.join();
    if (m_discovery_thread.joinable())  m_discovery_thread.join();

#ifdef _WIN32
    WSACleanup();
#endif
    log::info(TAG, "Mesh network shutdown — discovered %zu peers", m_peers.size());
}

void MeshNet::start_discovery() {
    if (m_discovering.load()) return;
    m_discovering.store(true);
    m_discovery_thread = std::thread(&MeshNet::discovery_loop, this);
    log::info(TAG, "Peer discovery started");
}

void MeshNet::stop_discovery() {
    m_discovering.store(false);
    if (m_discovery_thread.joinable()) m_discovery_thread.join();
    log::info(TAG, "Peer discovery stopped");
}

std::vector<MeshPeer> MeshNet::get_peers() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<MeshPeer> out;
    out.reserve(m_peers.size());
    for (const auto& [id, peer] : m_peers) {
        out.push_back(peer);
    }
    return out;
}

Result<void> MeshNet::send_text(const std::string& peer_id, const std::string& message) {
    std::string addr_str;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_peers.find(peer_id);
        if (it == m_peers.end())
            return Result<void>::error(StatusCode::ERR_NOT_FOUND);
        addr_str = it->second.address;
    }

    // Encrypt payload
    ByteBuffer plain(message.begin(), message.end());
    ByteBuffer encrypted = m_crypto->encrypt(plain, m_session_key);

    MeshPacket pkt = create_packet(MeshMsgType::TEXT_MSG, encrypted);
    pkt.hmac = m_crypto->hmac(encrypted, m_session_key);
    ByteBuffer buf = pkt.serialize();

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port   = htons(m_port);
    vos_inet_pton(addr_str.c_str(), &dest.sin_addr);

    sendto((int)m_socket, (const char*)buf.data(), (int)buf.size(), 0,
           (struct sockaddr*)&dest, sizeof(dest));

    log::info(TAG, "Sent encrypted message to %s (%zu bytes)",
              peer_id.c_str(), buf.size());
    return Result<void>::success();
}

Result<void> MeshNet::send_file(const std::string& peer_id,
                                const std::string& filename,
                                const ByteBuffer& data) {
    // Send file metadata first, then chunks
    std::string addr_str;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_peers.find(peer_id);
        if (it == m_peers.end())
            return Result<void>::error(StatusCode::ERR_NOT_FOUND);
        addr_str = it->second.address;
    }

    // META packet: filename + size
    std::string meta = filename + "|" + std::to_string(data.size());
    ByteBuffer meta_buf(meta.begin(), meta.end());
    MeshPacket meta_pkt = create_packet(MeshMsgType::FILE_META, meta_buf);
    ByteBuffer meta_wire = meta_pkt.serialize();

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port   = htons(m_port);
    vos_inet_pton(addr_str.c_str(), &dest.sin_addr);

    sendto((int)m_socket, (const char*)meta_wire.data(), (int)meta_wire.size(), 0,
           (struct sockaddr*)&dest, sizeof(dest));

    // Chunk data in 8KB pieces
    const size_t CHUNK_SIZE = 8192;
    for (size_t offset = 0; offset < data.size(); offset += CHUNK_SIZE) {
        size_t len = std::min(CHUNK_SIZE, data.size() - offset);
        ByteBuffer chunk(data.begin() + offset, data.begin() + offset + len);
        ByteBuffer enc = m_crypto->encrypt(chunk, m_session_key);

        MeshPacket cpkt = create_packet(MeshMsgType::FILE_CHUNK, enc);
        ByteBuffer cwire = cpkt.serialize();
        sendto((int)m_socket, (const char*)cwire.data(), (int)cwire.size(), 0,
               (struct sockaddr*)&dest, sizeof(dest));
    }

    log::info(TAG, "Sent file '%s' (%zu bytes) to %s",
              filename.c_str(), data.size(), peer_id.c_str());
    return Result<void>::success();
}

void MeshNet::on_message(MeshMessageFn fn) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_msg_callbacks.push_back(std::move(fn));
}

void MeshNet::on_peer_found(MeshPeerFn fn) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_peer_callbacks.push_back(std::move(fn));
}

std::string MeshNet::get_own_id() const {
    return m_own_id;
}

// ─── Background Threads ─────────────────────────────────────

void MeshNet::listener_loop() {
    ByteBuffer buf(65535);

    while (m_running.load()) {
        sockaddr_in from{};
        socklen_t from_len = sizeof(from);

        int received = recvfrom((int)m_socket, (char*)buf.data(), (int)buf.size(), 0,
                                (struct sockaddr*)&from, &from_len);

        if (received <= 0) continue; // timeout or error

        ByteBuffer data(buf.begin(), buf.begin() + received);
        auto res = MeshPacket::deserialize(data);
        if (!res.ok()) continue;

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &from.sin_addr, ip, INET_ADDRSTRLEN);
        handle_packet(res.value, std::string(ip));
    }
}

void MeshNet::discovery_loop() {
    while (m_discovering.load() && m_running.load()) {
        ByteBuffer payload(m_own_id.begin(), m_own_id.end());
        MeshPacket pkt = create_packet(MeshMsgType::DISCOVER, payload);
        ByteBuffer buf = pkt.serialize();

        sockaddr_in dest{};
        dest.sin_family      = AF_INET;
        dest.sin_port        = htons(m_port);
        dest.sin_addr.s_addr = INADDR_BROADCAST;

        sendto((int)m_socket, (const char*)buf.data(), (int)buf.size(), 0,
               (struct sockaddr*)&dest, sizeof(dest));

        // Sleep 5 seconds between broadcasts
        for (int i = 0; i < 50 && m_discovering.load(); i++) {
            std::this_thread::sleep_for(Millis(100));
        }
    }
}

void MeshNet::handle_packet(const MeshPacket& pkt, const std::string& from_addr) {
    std::lock_guard<std::mutex> lock(m_mutex);

    switch (pkt.type) {
    case MeshMsgType::DISCOVER:
    case MeshMsgType::DISCOVER_ACK: {
        std::string peer_id(pkt.payload.begin(), pkt.payload.end());
        if (peer_id == m_own_id) return; // Ignore self

        bool is_new = (m_peers.find(peer_id) == m_peers.end());

        MeshPeer& peer = m_peers[peer_id];
        peer.peer_id   = peer_id;
        peer.address   = from_addr;
        peer.last_seen = Clock::now();
        peer.connected = true;

        if (is_new) {
            log::info(TAG, "Discovered peer: %s @ %s", peer_id.c_str(), from_addr.c_str());
            for (auto& cb : m_peer_callbacks) cb(peer);

            if (pkt.type == MeshMsgType::DISCOVER) {
                // ACK back
                ByteBuffer ack_data(m_own_id.begin(), m_own_id.end());
                MeshPacket ack = create_packet(MeshMsgType::DISCOVER_ACK, ack_data);
                ByteBuffer ack_buf = ack.serialize();

                sockaddr_in dest{};
                dest.sin_family = AF_INET;
                dest.sin_port   = htons(m_port);
                vos_inet_pton(from_addr.c_str(), &dest.sin_addr);

                sendto((int)m_socket, (const char*)ack_buf.data(), (int)ack_buf.size(), 0,
                       (struct sockaddr*)&dest, sizeof(dest));
            }
        }
        break;
    }

    case MeshMsgType::TEXT_MSG: {
        // Find peer by address
        std::string sender_id = "unknown";
        for (const auto& [id, p] : m_peers) {
            if (p.address == from_addr) { sender_id = id; break; }
        }

        // Decrypt payload
        ByteBuffer decrypted = m_crypto->decrypt(pkt.payload, m_session_key);

        log::info(TAG, "Message from %s: %.*s",
                  sender_id.c_str(), (int)decrypted.size(), decrypted.data());

        for (auto& cb : m_msg_callbacks) cb(sender_id, decrypted);
        break;
    }

    case MeshMsgType::PING: {
        ByteBuffer pong_data(m_own_id.begin(), m_own_id.end());
        MeshPacket pong = create_packet(MeshMsgType::PONG, pong_data);
        ByteBuffer pong_buf = pong.serialize();

        sockaddr_in dest{};
        dest.sin_family = AF_INET;
        dest.sin_port   = htons(m_port);
        vos_inet_pton(from_addr.c_str(), &dest.sin_addr);
        sendto((int)m_socket, (const char*)pong_buf.data(), (int)pong_buf.size(), 0,
               (struct sockaddr*)&dest, sizeof(dest));
        break;
    }

    default:
        break;
    }
}

MeshPacket MeshNet::create_packet(MeshMsgType type, const ByteBuffer& payload) {
    MeshPacket pkt;
    pkt.magic       = MESH_MAGIC;
    pkt.version     = MESH_VERSION;
    pkt.type        = type;
    pkt.payload_len = (uint32_t)payload.size();
    pkt.payload     = payload;
    return pkt;
}

} // namespace vos
