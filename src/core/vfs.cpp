#include "vfs.h"
#include "vos/log.h"
#include <algorithm>

namespace vos {

static const char* TAG = "VFS";

VirtualFS::VirtualFS() = default;

Result<void> VirtualFS::init() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Create default directories
    auto now = std::time(nullptr);
    auto make_dir = [&](const std::string& p) {
        VFSEntry e;
        e.name     = p;
        e.is_dir   = true;
        e.created  = now;
        e.modified = now;
        m_entries[p] = std::move(e);
    };

    make_dir("/");
    make_dir("/home");
    make_dir("/tmp");
    make_dir("/apps");
    make_dir("/system");

    log::info(TAG, "Virtual filesystem initialized with default dirs");
    return Result<void>::success();
}

std::string VirtualFS::normalize_path(const std::string& path) const {
    std::string p = path;
    // Ensure leading /
    if (p.empty() || p[0] != '/') p = "/" + p;
    // Remove trailing / (unless root)
    while (p.size() > 1 && p.back() == '/') p.pop_back();
    return p;
}

Result<void> VirtualFS::write_file(const std::string& path, const ByteBuffer& data) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string p = normalize_path(path);

    auto now = std::time(nullptr);
    auto it = m_entries.find(p);
    if (it != m_entries.end()) {
        if (it->second.is_dir) {
            return Result<void>::error(StatusCode::ERR_INVALID_ARG);
        }
        it->second.data     = data;
        it->second.modified = now;
    } else {
        VFSEntry e;
        e.name     = p;
        e.is_dir   = false;
        e.data     = data;
        e.created  = now;
        e.modified = now;
        m_entries[p] = std::move(e);
    }

    log::debug(TAG, "Write %zu bytes -> %s", data.size(), p.c_str());
    return Result<void>::success();
}

Result<ByteBuffer> VirtualFS::read_file(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string p = normalize_path(path);

    auto it = m_entries.find(p);
    if (it == m_entries.end()) {
        return Result<ByteBuffer>::error(StatusCode::ERR_NOT_FOUND);
    }
    if (it->second.is_dir) {
        return Result<ByteBuffer>::error(StatusCode::ERR_INVALID_ARG);
    }
    return Result<ByteBuffer>::success(it->second.data);
}

Result<void> VirtualFS::delete_file(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string p = normalize_path(path);

    auto it = m_entries.find(p);
    if (it == m_entries.end()) {
        return Result<void>::error(StatusCode::ERR_NOT_FOUND);
    }
    log::debug(TAG, "Delete %s", p.c_str());
    m_entries.erase(it);
    return Result<void>::success();
}

bool VirtualFS::exists(const std::string& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries.count(normalize_path(path)) > 0;
}

Result<void> VirtualFS::mkdir(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string p = normalize_path(path);

    if (m_entries.count(p)) {
        return Result<void>::error(StatusCode::ERR_ALREADY_EXISTS);
    }

    auto now = std::time(nullptr);
    VFSEntry e;
    e.name     = p;
    e.is_dir   = true;
    e.created  = now;
    e.modified = now;
    m_entries[p] = std::move(e);

    log::debug(TAG, "mkdir %s", p.c_str());
    return Result<void>::success();
}

Result<std::vector<std::string>> VirtualFS::list_dir(const std::string& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string p = normalize_path(path);

    auto it = m_entries.find(p);
    if (it == m_entries.end() || !it->second.is_dir) {
        return Result<std::vector<std::string>>::error(StatusCode::ERR_NOT_FOUND);
    }

    std::string prefix = (p == "/") ? "/" : p + "/";
    std::vector<std::string> children;
    for (const auto& [k, v] : m_entries) {
        if (k == p) continue;
        if (k.rfind(prefix, 0) == 0) {
            // Direct child only (no deeper nesting)
            auto remainder = k.substr(prefix.size());
            if (remainder.find('/') == std::string::npos) {
                children.push_back(k);
            }
        }
    }
    std::sort(children.begin(), children.end());
    return Result<std::vector<std::string>>::success(std::move(children));
}

size_t VirtualFS::total_files() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t count = 0;
    for (const auto& [k, v] : m_entries) {
        if (!v.is_dir) count++;
    }
    return count;
}

size_t VirtualFS::total_size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t sz = 0;
    for (const auto& [k, v] : m_entries) {
        sz += v.data.size();
    }
    return sz;
}

} // namespace vos
