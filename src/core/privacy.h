#pragma once

#include "vos/types.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

namespace vos {

struct IdentityState {
    std::string virtual_ip;       // Current virtual IP identity
    std::string virtual_mac;      // Randomized MAC
    uint64_t    rotation_count;   // How many times IP has rotated
    TimePoint   last_rotation;    // When last rotation happened
};

// Callback when identity rotates
using IdentityChangedFn = std::function<void(const IdentityState&)>;

class PrivacyEngine {
public:
    PrivacyEngine();
    ~PrivacyEngine();

    // Initialize and start background rotation
    Result<void> init(int rotation_interval_sec = 10);

    // Stop the rotation thread
    void shutdown();

    // Get current identity
    IdentityState get_current_identity() const;

    // Force a manual rotation
    void force_rotate();

    // Register callback for identity changes
    void on_identity_changed(IdentityChangedFn fn);

    // Check if engine is running
    bool is_running() const { return m_running.load(); }

private:
    void rotation_loop();
    void rotate_identity();
    std::string generate_random_ip();
    std::string generate_random_mac();

    mutable std::mutex           m_mutex;
    std::atomic<bool>            m_running{false};
    int                          m_interval_sec{10};
    IdentityState                m_state;
    std::thread                  m_thread;
    std::vector<IdentityChangedFn> m_callbacks;
};

} // namespace vos
