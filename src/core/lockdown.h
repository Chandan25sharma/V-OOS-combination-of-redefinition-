#pragma once

#include "vos/types.h"
#include <string>
#include <vector>
#include <chrono>

namespace vos {

/*
 * Lockdown Manager
 * Enforces restricted access to apps based on a timer.
 */
class LockdownManager {
public:
    LockdownManager();
    ~LockdownManager() = default;

    Result<void> init();

    // Start a lockdown period
    Result<void> start(Seconds duration);
    
    // Check if lockdown is currently active
    bool is_active() const;

    // Get remaining time
    Seconds get_remaining_time() const;

    // Verify if an app is allowed
    bool is_app_allowed(AppId app_id) const;

    // Force unlock (for emergency/debug - usually disabled)
    void force_unlock();

private:
    bool m_active = false;
    TimePoint m_end_time;
    std::vector<AppId> m_whitelist;
};

} // namespace vos
