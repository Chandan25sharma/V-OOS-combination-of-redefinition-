/*
 * VOS Unit Test — Mesh Network Packet Serialization
 */
#include <cassert>
#include <cstdio>
#include "core/mesh_net.h"
#include "core/crypto.h"

using namespace vos;

void test_packet_roundtrip() {
    MeshPacket pkt;
    pkt.magic       = MESH_MAGIC;
    pkt.version     = MESH_VERSION;
    pkt.type        = MeshMsgType::TEXT_MSG;
    pkt.payload     = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"
    pkt.payload_len = (uint32_t)pkt.payload.size();
    pkt.hmac        = {0xAA, 0xBB, 0xCC};

    ByteBuffer wire = pkt.serialize();
    auto res = MeshPacket::deserialize(wire);

    assert(res.ok());
    assert(res.value.magic == MESH_MAGIC);
    assert(res.value.version == MESH_VERSION);
    assert(res.value.type == MeshMsgType::TEXT_MSG);
    assert(res.value.payload == pkt.payload);
    assert(res.value.hmac == pkt.hmac);
    printf("[PASS] test_packet_roundtrip\n");
}

void test_bad_magic() {
    ByteBuffer bad = {0x00, 0x00, 0x00, 0x00, 0x01, 0x10, 0x00, 0x00, 0x00, 0x00};
    auto res = MeshPacket::deserialize(bad);
    assert(!res.ok());
    printf("[PASS] test_bad_magic\n");
}

void test_truncated_packet() {
    // Too short
    ByteBuffer tiny = {0x01, 0x02};
    auto r1 = MeshPacket::deserialize(tiny);
    assert(!r1.ok());

    // Header claims more payload than available
    MeshPacket pkt;
    pkt.magic       = MESH_MAGIC;
    pkt.version     = MESH_VERSION;
    pkt.type        = MeshMsgType::PING;
    pkt.payload     = {0x01};
    pkt.payload_len = 999; // Lie about length
    ByteBuffer wire = pkt.serialize();
    auto r2 = MeshPacket::deserialize(wire);
    assert(!r2.ok());
    printf("[PASS] test_truncated_packet\n");
}

void test_empty_payload() {
    MeshPacket pkt;
    pkt.magic       = MESH_MAGIC;
    pkt.version     = MESH_VERSION;
    pkt.type        = MeshMsgType::PING;
    pkt.payload     = {};
    pkt.payload_len = 0;

    ByteBuffer wire = pkt.serialize();
    auto res = MeshPacket::deserialize(wire);
    assert(res.ok());
    assert(res.value.payload.empty());
    printf("[PASS] test_empty_payload\n");
}

void test_discover_packet() {
    std::string peer_id = "TEST_PEER_42";
    MeshPacket pkt;
    pkt.magic       = MESH_MAGIC;
    pkt.version     = MESH_VERSION;
    pkt.type        = MeshMsgType::DISCOVER;
    pkt.payload.assign(peer_id.begin(), peer_id.end());
    pkt.payload_len = (uint32_t)pkt.payload.size();

    ByteBuffer wire = pkt.serialize();
    auto res = MeshPacket::deserialize(wire);
    assert(res.ok());
    assert(res.value.type == MeshMsgType::DISCOVER);

    std::string decoded(res.value.payload.begin(), res.value.payload.end());
    assert(decoded == peer_id);
    printf("[PASS] test_discover_packet\n");
}

void test_crypto_encrypt_decrypt() {
    Crypto crypto;
    crypto.init();

    ByteBuffer key = crypto.generate_key();
    assert(key.size() == 32);

    std::string msg = "Hello VOS Mesh!";
    ByteBuffer plain(msg.begin(), msg.end());
    ByteBuffer cipher = crypto.encrypt(plain, key);
    ByteBuffer decrypted = crypto.decrypt(cipher, key);

    assert(decrypted == plain);
    printf("[PASS] test_crypto_encrypt_decrypt\n");
}

void test_crypto_hmac() {
    Crypto crypto;
    crypto.init();

    ByteBuffer key = crypto.generate_key();
    ByteBuffer data = {1, 2, 3, 4, 5};

    ByteBuffer mac = crypto.hmac(data, key);
    assert(mac.size() == 32);
    assert(crypto.hmac_verify(data, key, mac));

    // Tamper with data
    ByteBuffer tampered = data;
    tampered[0] = 99;
    assert(!crypto.hmac_verify(tampered, key, mac));
    printf("[PASS] test_crypto_hmac\n");
}

int main() {
    printf("=== Mesh Network & Crypto Tests ===\n");
    test_packet_roundtrip();
    test_bad_magic();
    test_truncated_packet();
    test_empty_payload();
    test_discover_packet();
    test_crypto_encrypt_decrypt();
    test_crypto_hmac();
    printf("All Mesh Network & Crypto tests passed!\n\n");
    return 0;
}
