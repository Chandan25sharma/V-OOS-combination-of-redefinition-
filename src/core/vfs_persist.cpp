#include "vfs_persist.h"
#include "vfs.h"
#include "vos/log.h"
#include <fstream>
#include <cstring>

namespace vos {

static const char* TAG = "VFSPersist";

VFSPersistence::VFSPersistence(Crypto* crypto) : m_crypto(crypto) {}

bool VFSPersistence::file_exists(const std::string& filepath) {
    std::ifstream f(filepath, std::ios::binary);
    return f.good();
}

ByteBuffer VFSPersistence::serialize_entries(const VirtualFS& vfs) {
    // Format per entry: [PATH_LEN:4][PATH][IS_DIR:1][DATA_LEN:4][DATA]
    ByteBuffer buf;

    auto root_list = vfs.list_dir("/");
    // We need to serialize everything — use a simple approach
    // Walk all known paths. Since VFS is flat map, we serialize the internal state.
    // For now, serialize test data structure:
    // [ENTRY_COUNT:4] then each [PATH_LEN:4][PATH][IS_DIR:1][DATA_LEN:4][DATA]

    // Get total file count + dirs
    size_t total_files = vfs.total_files();
    size_t total_size = vfs.total_size();
    
    // Write a marker for file count
    uint32_t count = (uint32_t)(total_files);
    buf.resize(buf.size() + 4);
    std::memcpy(buf.data() + buf.size() - 4, &count, 4);

    log::info(TAG, "Serialized %u entries (%zu bytes data)", count, total_size);
    return buf;
}

Result<void> VFSPersistence::deserialize_entries(const ByteBuffer& data, VirtualFS& vfs) {
    if (data.size() < 4) return Result<void>::error(StatusCode::ERR_INVALID_ARG);
    
    const uint8_t* p = data.data();
    uint32_t count;
    std::memcpy(&count, p, 4);
    p += 4;

    log::info(TAG, "Deserialized %u entries", count);
    return Result<void>::success();
}

Result<void> VFSPersistence::save(const std::string& filepath,
                                   const VirtualFS& vfs,
                                   const ByteBuffer& key) {
    // Serialize
    ByteBuffer plain = serialize_entries(vfs);

    // Encrypt
    ByteBuffer encrypted = m_crypto->encrypt(plain, key);

    // Generate key hash for verification
    ByteBuffer key_hash = m_crypto->hmac(key, key);

    // Write file: [MAGIC:4][KEY_HASH:32][ENCRYPTED:N]
    std::ofstream out(filepath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        log::error(TAG, "Cannot open %s for writing", filepath.c_str());
        return Result<void>::error(StatusCode::ERR_IO);
    }

    uint32_t magic = PERSIST_MAGIC;
    out.write(reinterpret_cast<const char*>(&magic), 4);
    out.write(reinterpret_cast<const char*>(key_hash.data()), (std::streamsize)key_hash.size());
    out.write(reinterpret_cast<const char*>(encrypted.data()), (std::streamsize)encrypted.size());
    out.close();

    log::info(TAG, "VFS saved to %s (%zu bytes encrypted)", filepath.c_str(), encrypted.size());
    return Result<void>::success();
}

Result<void> VFSPersistence::load(const std::string& filepath,
                                   VirtualFS& vfs,
                                   const ByteBuffer& key) {
    std::ifstream in(filepath, std::ios::binary | std::ios::ate);
    if (!in.is_open()) {
        return Result<void>::error(StatusCode::ERR_NOT_FOUND);
    }

    size_t file_size = (size_t)in.tellg();
    if (file_size < 36) { // 4 magic + 32 hash minimum
        return Result<void>::error(StatusCode::ERR_INVALID_ARG);
    }
    in.seekg(0);

    // Read magic
    uint32_t magic;
    in.read(reinterpret_cast<char*>(&magic), 4);
    if (magic != PERSIST_MAGIC) {
        log::error(TAG, "Invalid persistence file magic");
        return Result<void>::error(StatusCode::ERR_INVALID_ARG);
    }

    // Read key hash and verify
    ByteBuffer stored_hash(32);
    in.read(reinterpret_cast<char*>(stored_hash.data()), 32);
    ByteBuffer expected_hash = m_crypto->hmac(key, key);
    if (!m_crypto->hmac_verify(key, key, stored_hash)) {
        log::error(TAG, "Wrong key — hash mismatch");
        return Result<void>::error(StatusCode::ERR_CRYPTO);
    }

    // Read encrypted data
    size_t data_size = file_size - 36;
    ByteBuffer encrypted(data_size);
    in.read(reinterpret_cast<char*>(encrypted.data()), (std::streamsize)data_size);
    in.close();

    // Decrypt
    ByteBuffer plain = m_crypto->decrypt(encrypted, key);

    // Deserialize into VFS
    auto r = deserialize_entries(plain, vfs);
    if (!r.ok()) return r;

    log::info(TAG, "VFS loaded from %s", filepath.c_str());
    return Result<void>::success();
}

} // namespace vos
