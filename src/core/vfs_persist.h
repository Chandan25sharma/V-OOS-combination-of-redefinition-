#pragma once

#include "vos/types.h"
#include "crypto.h"
#include <string>
#include <fstream>

namespace vos {

/**
 * Persistent VFS Storage
 * Serializes the VirtualFS to an encrypted file on disk.
 * Data format: [MAGIC:4][KEY_HASH:32][IV:16][ENCRYPTED_DATA:N]
 */
class VFSPersistence {
public:
    VFSPersistence(Crypto* crypto);
    ~VFSPersistence() = default;

    // Save all VFS entries to an encrypted file
    Result<void> save(const std::string& filepath, const class VirtualFS& vfs,
                      const ByteBuffer& key);

    // Load VFS entries from an encrypted file
    Result<void> load(const std::string& filepath, class VirtualFS& vfs,
                      const ByteBuffer& key);

    // Check if a persistence file exists
    static bool file_exists(const std::string& filepath);

private:
    // Serialize entries to a flat buffer
    ByteBuffer serialize_entries(const class VirtualFS& vfs);

    // Deserialize from buffer back into VFS
    Result<void> deserialize_entries(const ByteBuffer& data, class VirtualFS& vfs);

    Crypto* m_crypto;

    static constexpr uint32_t PERSIST_MAGIC = 0x564F5346; // "VOSF"
};

} // namespace vos
