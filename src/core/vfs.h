#pragma once

#include "vos/types.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include <ctime>

namespace vos {

struct VFSEntry {
    std::string name;
    bool        is_dir;
    ByteBuffer  data;
    time_t      created;
    time_t      modified;
};

class VirtualFS {
public:
    VirtualFS();
    ~VirtualFS() = default;

    Result<void> init();

    // File operations
    Result<void>       write_file(const std::string& path, const ByteBuffer& data);
    Result<ByteBuffer> read_file(const std::string& path);
    Result<void>       delete_file(const std::string& path);
    bool               exists(const std::string& path) const;

    // Directory operations
    Result<void> mkdir(const std::string& path);
    Result<std::vector<std::string>> list_dir(const std::string& path) const;

    // Stats
    size_t total_files() const;
    size_t total_size() const;

private:
    std::string normalize_path(const std::string& path) const;

    mutable std::mutex                          m_mutex;
    std::unordered_map<std::string, VFSEntry>   m_entries;
};

} // namespace vos
