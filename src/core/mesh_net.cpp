#include "mesh_net.h"
#include "vos/log.h"
#include <iostream>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

namespace vos {

static const char* TAG = "MeshNet";

ByteBuffer MeshPacket::serialize() const {
    size_t total_size = 4 + 1 + 1 + 4 + payload.size() + hmac.size();
    ByteBuffer buf(total_size);
    uint8_t* p = buf.data();

    auto write32 = [&](uint32_t val) {
        std::memcpy(p, &val, 4);
        p += 4;
    };
    auto write8 = [&](uint8_t val) {
        *p++ = val;
    };

    write32(magic);
    write8(version);
    write8(static_cast<uint8_t>(type));
    write32(payload_len);
    std::memcpy(p, payload.data(), payload.size());
    p += payload.size();
    if (!hmac.empty()) {
        std::memcpy(p, hmac.data(), hmac.size());
    }

    return buf;
}

Result<MeshPacket> MeshPacket::deserialize(const ByteBuffer& data) {
    if (data.size() < 10) return Result<MeshPacket>::error(StatusCode::ERR_INVALID_ARG);
    
    const uint8_t* p = data.data();
    MeshPacket pkt;
    std::memcpy(&pkt.magic, p, 4); p += 4;
    if (pkt.magic != MESH_MAGIC) return Result<MeshPacket>::error(StatusCode::ERR_INVALID_ARG);
    
    pkt.version = *p++;
    pkt.type = static_cast<MeshMsgType>(*p++);
    std::memcpy(&pkt.payload_len, p, 4); p += 4;
    
    if (data.size() < 10 + pkt.payload_len) return Result<MeshPacket>::error(StatusCode::ERR_INVALID_ARG);
    
    pkt.payload.assign(p, p + pkt.payload_len);
    p += pkt.payload_len;
    
    size_t remaining = data.size() - (10 + pkt.payload_len);
    if (remaining > 0) {
        pkt.hmac.assign(p, p + remaining);
    }
    
    return Result<MeshPacket>::success(std::move(pkt));
}

MeshNet::MeshNet() {
    m_own_id = "PEER_" + std::to_string(std::rand() % 10000);
}

MeshNet::~MeshNet() {
    shutdown();
}

Result<void> MeshNet::init(Crypto* crypto, uint16_t port) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_crypto = crypto;
    m_port = port;
    m_session_key = m_crypto->generate_key();

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return Result<void>::error(StatusCode::ERR_INTERNAL);
    }
#endif

    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET) {
        return Result<void>::error(StatusCode::ERR_NETWORK);
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(m_port);

    if (bind(m_socket, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(m_socket);
        return Result<void>::error(StatusCode::ERR_NETWORK);
    }

    m_running.store(true);
    m_listener_thread = std::thread(&MeshNet::listener_loop, this);

    log::info(TAG, "Mesh network initialized on port %u. PeerID: %s", m_port, m_own_id.c_str());
    return Result<void>::success();
}

void MeshNet::shutdown() {
    if (!m_running.load()) return;
    m_running.store(false);
    m_discovering.store(false);

    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }

    if (m_listener_thread.joinable()) m_listener_thread.join();
    if (m_discovery_thread.joinable()) m_discovery_thread.join();

#ifdef _WIN32
    WSACleanup();
#endif
    log::info(TAG, "Mesh network shutdown");
}

void MeshNet::start_discovery() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_discovering.load()) return;
    m_discovering.store(true);
    m_discovery_thread = std::thread(&MeshNet::discovery_loop, this);
    log::info(TAG, "Starting peer discovery...");
}

void MeshNet::stop_discovery() {
    m_discovering.store(false);
}

std::vector<MeshPeer> MeshNet::get_peers() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<MeshPeer> peers;
    for (const auto& [id, peer] : m_peers) {
        peers.push_back(peer);
    }
    return peers;
}

Result<void> MeshNet::send_text(const std::string& peer_id, const std::string& message) {
    ByteBuffer payload(message.begin(), message.end());
    MeshPacket pkt = create_packet(MeshMsgType::TEXT_MSG, payload);
    
    std::string addr_str;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_peers.find(peer_id);
        if (it == m_peers.end()) return Result<void>::error(StatusCode::ERR_NOT_FOUND);
        addr_str = it->second.address;
    }

    ByteBuffer buf = pkt.serialize();
    
    sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(m_port);
    inet_ptr(addr_str.c_str(), &dest.sin_addr.s_addr);

    sendto(m_socket, (const char*)buf.data(), (int)buf.size(), 0, (struct sockaddr*)&dest, sizeof(dest));
    
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

void MeshNet::listener_loop() {
    ByteBuffer buf(65535);
    sockaddr_in from;
    int from_len = sizeof(from);

    while (m_running.load()) {
        int received = recvfrom(m_socket, (char*)buf.data(), (int)buf.size(), 0, (struct sockaddr*)&from, &from_len);
        if (received <= 0) continue;

        ByteBuffer data(buf.begin(), buf.begin() + received);
        auto res = MeshPacket::deserialize(data);
        if (res.ok()) {
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &from.sin_addr, ip, INET_ADDRSTRLEN);
            handle_packet(res.value, std::string(ip));
        }
    }
}

void MeshNet::discovery_loop() {
    while (m_discovering.load()) {
        ByteBuffer payload(m_own_id.begin(), m_own_id.end());
        MeshPacket pkt = create_packet(MeshMsgType::DISCOVER, payload);
        ByteBuffer buf = pkt.serialize();

        sockaddr_in dest;
        dest.sin_family = AF_INET;
        dest.sin_port = htons(m_port);
        dest.sin_addr.s_addr = INADDR_BROADCAST;

        #ifdef _WIN32
        BOOL opt = TRUE;
        setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, (char*)&opt, sizeof(opt));
        #else
        int opt = 1;
        setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
        #endif

        sendto(m_socket, (const char*)buf.data(), (int)buf.size(), 0, (struct sockaddr*)&dest, sizeof(dest));

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

void MeshNet::handle_packet(const MeshPacket& pkt, const std::string& from_addr) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (pkt.type == MeshMsgType::DISCOVER || pkt.type == MeshMsgType::DISCOVER_ACK) {
        std::string peer_id(pkt.payload.begin(), pkt.payload.end());
        if (peer_id == m_own_id) return;

        bool is_new = m_peers.find(peer_id) == m_peers.end();
        MeshPeer& peer = m_peers[peer_id];
        peer.peer_id = peer_id;
        peer.address = from_addr;
        peer.last_seen = Clock::now();
        peer.connected = true;

        if (is_new) {
            log::info(TAG, "Found peer: %s at %s", peer_id.c_str(), from_addr.c_str());
            for (auto& cb : m_peer_callbacks) cb(peer);
            
            // Send ACK if it was a discovery
            if (pkt.type == MeshMsgType::DISCOVER) {
                ByteBuffer payload(m_own_id.begin(), m_own_id.end());
                MeshPacket ack = create_packet(MeshMsgType::DISCOVER_ACK, payload);
                ByteBuffer buf = ack.serialize();
                
                sockaddr_in dest;
                dest.sin_family = AF_INET;
                dest.sin_port = htons(m_port);
                inet_ptr(from_addr.c_str(), &dest.sin_addr.s_addr);
                sendto(m_socket, (const char*)buf.data(), (int)buf.size(), 0, (struct sockaddr*)&dest, sizeof(dest));
            }
        }
    } else if (pkt.type == MeshMsgType::TEXT_MSG) {
        // Find peer ID from address
        std::string peer_id = "unknown";
        for (auto const& [id, p] : m_peers) {
            if (p.address == from_addr) {
                peer_id = id;
                break;
            }
        }
        
        log::info(TAG, "Message from %s: %s", peer_id.c_str(), std::string(pkt.payload.begin(), pkt.payload.end()).c_str());
        for (auto& cb : m_msg_callbacks) cb(peer_id, pkt.payload);
    }
}

MeshPacket MeshNet::create_packet(MeshMsgType type, const ByteBuffer& payload) {
    MeshPacket pkt;
    pkt.magic = MESH_MAGIC;
    pkt.version = MESH_VERSION;
    pkt.type = type;
    pkt.payload_len = (uint32_t)payload.size();
    pkt.payload = payload;
    // In production, sign with HMAC using m_crypto
    return pkt;
}

// Helper to avoid inet_addr warnings
void inet_ptr(const char* cp, void* addr) {
    #ifdef _WIN32
    InetPtonA(AF_INET, cp, addr);
    #else
    inet_pton(AF_INET, cp, addr);
    #endif
}

} // namespace vos
