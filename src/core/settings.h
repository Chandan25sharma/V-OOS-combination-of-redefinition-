#pragma once

#include "vos/types.h"
#include <string>
#include <unordered_map>
#include <mutex>

namespace vos {

/**
 * VOS Settings / Configuration System
 * Key-value config store with typed getters, defaults, and disk persistence.
 */
class Settings {
public:
    Settings();
    ~Settings() = default;

    Result<void> init();

    // ─── Typed Getters (with defaults) ───────────────────────
    std::string get_string(const std::string& key, const std::string& default_val = "") const;
    int         get_int(const std::string& key, int default_val = 0) const;
    bool        get_bool(const std::string& key, bool default_val = false) const;
    float       get_float(const std::string& key, float default_val = 0.0f) const;

    // ─── Setters ─────────────────────────────────────────────
    void set_string(const std::string& key, const std::string& value);
    void set_int(const std::string& key, int value);
    void set_bool(const std::string& key, bool value);
    void set_float(const std::string& key, float value);

    // ─── Persistence ─────────────────────────────────────────
    Result<void> save(const std::string& filepath) const;
    Result<void> load(const std::string& filepath);

    // ─── Predefined Keys ─────────────────────────────────────
    static constexpr const char* KEY_IP_ROTATION_INTERVAL = "privacy.ip_rotation_sec";
    static constexpr const char* KEY_MESH_PORT            = "mesh.port";
    static constexpr const char* KEY_DNS_GUARD_ENABLED    = "privacy.dns_guard";
    static constexpr const char* KEY_AUTO_DISCOVER         = "mesh.auto_discover";
    static constexpr const char* KEY_LOCKDOWN_DEFAULT_MIN  = "lockdown.default_minutes";
    static constexpr const char* KEY_VFS_PERSIST_PATH      = "vfs.persist_path";
    static constexpr const char* KEY_LOG_LEVEL             = "system.log_level";
    static constexpr const char* KEY_THEME                 = "ui.theme";

private:
    void set_defaults();

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::string> m_store;
};

} // namespace vos
