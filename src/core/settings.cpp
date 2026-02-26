#include "settings.h"
#include "vos/log.h"
#include <fstream>
#include <sstream>

namespace vos {

static const char* TAG = "Settings";

Settings::Settings() = default;

Result<void> Settings::init() {
    set_defaults();
    log::info(TAG, "Settings initialized with %zu entries", m_store.size());
    return Result<void>::success();
}

void Settings::set_defaults() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_store[KEY_IP_ROTATION_INTERVAL] = "10";
    m_store[KEY_MESH_PORT]            = "5055";
    m_store[KEY_DNS_GUARD_ENABLED]    = "true";
    m_store[KEY_AUTO_DISCOVER]        = "true";
    m_store[KEY_LOCKDOWN_DEFAULT_MIN] = "5";
    m_store[KEY_VFS_PERSIST_PATH]     = "vos_data.enc";
    m_store[KEY_LOG_LEVEL]            = "info";
    m_store[KEY_THEME]                = "dark";
}

// ─── Getters ─────────────────────────────────────────────────

std::string Settings::get_string(const std::string& key, const std::string& def) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_store.find(key);
    return (it != m_store.end()) ? it->second : def;
}

int Settings::get_int(const std::string& key, int def) const {
    auto s = get_string(key);
    if (s.empty()) return def;
    try { return std::stoi(s); } catch (...) { return def; }
}

bool Settings::get_bool(const std::string& key, bool def) const {
    auto s = get_string(key);
    if (s.empty()) return def;
    return (s == "true" || s == "1" || s == "yes");
}

float Settings::get_float(const std::string& key, float def) const {
    auto s = get_string(key);
    if (s.empty()) return def;
    try { return std::stof(s); } catch (...) { return def; }
}

// ─── Setters ─────────────────────────────────────────────────

void Settings::set_string(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_store[key] = value;
}

void Settings::set_int(const std::string& key, int value) {
    set_string(key, std::to_string(value));
}

void Settings::set_bool(const std::string& key, bool value) {
    set_string(key, value ? "true" : "false");
}

void Settings::set_float(const std::string& key, float value) {
    set_string(key, std::to_string(value));
}

// ─── Persistence ─────────────────────────────────────────────

Result<void> Settings::save(const std::string& filepath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ofstream out(filepath);
    if (!out.is_open()) return Result<void>::error(StatusCode::ERR_IO);

    out << "# VOS Configuration\n";
    out << "# Auto-generated — do not edit manually\n\n";

    for (const auto& [key, val] : m_store) {
        out << key << "=" << val << "\n";
    }
    out.close();

    log::info(TAG, "Settings saved to %s (%zu entries)", filepath.c_str(), m_store.size());
    return Result<void>::success();
}

Result<void> Settings::load(const std::string& filepath) {
    std::ifstream in(filepath);
    if (!in.is_open()) return Result<void>::error(StatusCode::ERR_NOT_FOUND);

    std::lock_guard<std::mutex> lock(m_mutex);
    std::string line;
    int count = 0;

    while (std::getline(in, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        // Trim whitespace
        while (!key.empty() && key.back() == ' ') key.pop_back();
        while (!val.empty() && val.front() == ' ') val.erase(0, 1);

        m_store[key] = val;
        count++;
    }

    log::info(TAG, "Loaded %d settings from %s", count, filepath.c_str());
    return Result<void>::success();
}

} // namespace vos
