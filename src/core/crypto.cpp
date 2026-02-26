#include "crypto.h"
#include "vos/log.h"
#include <random>
#include <algorithm>
#include <cstring>

namespace vos {

static const char* TAG = "Crypto";

Crypto::Crypto() = default;

Result<void> Crypto::init() {
    m_initialized = true;
    log::info(TAG, "Crypto engine initialized (demo mode — use libsodium for production)");
    return Result<void>::success();
}

ByteBuffer Crypto::random_bytes(size_t count) {
    ByteBuffer buf(count);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint16_t> dist(0, 255);
    for (size_t i = 0; i < count; i++) {
        buf[i] = static_cast<uint8_t>(dist(gen));
    }
    return buf;
}

ByteBuffer Crypto::generate_key() {
    return random_bytes(32); // 256-bit
}

ByteBuffer Crypto::encrypt(const ByteBuffer& plaintext, const ByteBuffer& key) {
    // XOR cipher for demo — replace with AES-256-GCM in production
    ByteBuffer out(plaintext.size());
    for (size_t i = 0; i < plaintext.size(); i++) {
        out[i] = plaintext[i] ^ key[i % key.size()];
    }
    return out;
}

ByteBuffer Crypto::decrypt(const ByteBuffer& ciphertext, const ByteBuffer& key) {
    // XOR is symmetric
    return encrypt(ciphertext, key);
}

ByteBuffer Crypto::hmac(const ByteBuffer& data, const ByteBuffer& key) {
    // Simple hash-based MAC for demo
    // In production: HMAC-SHA256 via libsodium
    ByteBuffer mac(32, 0);
    for (size_t i = 0; i < data.size(); i++) {
        mac[i % 32] ^= data[i] ^ key[i % key.size()];
    }
    // Second pass for mixing
    for (size_t i = 0; i < 32; i++) {
        mac[i] = static_cast<uint8_t>((mac[i] * 31 + key[i % key.size()]) & 0xFF);
    }
    return mac;
}

bool Crypto::hmac_verify(const ByteBuffer& data, const ByteBuffer& key,
                         const ByteBuffer& expected) {
    auto computed = hmac(data, key);
    if (computed.size() != expected.size()) return false;
    // Constant-time comparison
    uint8_t diff = 0;
    for (size_t i = 0; i < computed.size(); i++) {
        diff |= computed[i] ^ expected[i];
    }
    return diff == 0;
}

} // namespace vos
