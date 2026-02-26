#pragma once

#include "vos/types.h"
#include <string>

namespace vos {

/*
 * Minimal crypto wrapper.
 * In production, this would wrap libsodium or OpenSSL.
 * For now, we use a simple XOR-based cipher for demonstration,
 * with the interface designed for drop-in replacement.
 */
class Crypto {
public:
    Crypto();
    ~Crypto() = default;

    Result<void> init();

    // Generate a random 256-bit key
    ByteBuffer generate_key();

    // Encrypt/Decrypt with a given key
    ByteBuffer encrypt(const ByteBuffer& plaintext, const ByteBuffer& key);
    ByteBuffer decrypt(const ByteBuffer& ciphertext, const ByteBuffer& key);

    // HMAC for integrity
    ByteBuffer hmac(const ByteBuffer& data, const ByteBuffer& key);
    bool       hmac_verify(const ByteBuffer& data, const ByteBuffer& key,
                           const ByteBuffer& expected);

    // Random bytes
    ByteBuffer random_bytes(size_t count);

private:
    bool m_initialized = false;
};

} // namespace vos
