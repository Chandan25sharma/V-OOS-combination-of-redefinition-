#include "lockdown.h"
#include "vos/log.h"
#include <algorithm>

namespace vos {

static const char* TAG = "Lockdown";

LockdownManager::LockdownManager() {
    m_whitelist = { APP_DIALER, APP_SMS, APP_CAMERA, APP_SYSTEM };
}

Result<void> LockdownManager::init() {
    log::info(TAG, "Lockdown manager initialized");
    return Result<void>::success();
}

Result<void> LockdownManager::start(Seconds duration) {
    if (m_active) {
        return Result<void>::error(StatusCode::ERR_ALREADY_EXISTS);
    }

    m_active = true;
    m_end_time = Clock::now() + duration;
    
    log::warn(TAG, "LOCKDOWN ACTIVATED for %lld seconds", (long long)duration.count());
    return Result<void>::success();
}

bool LockdownManager::is_active() const {
    if (!m_active) return false;

    if (Clock::now() >= m_end_time) {
        const_cast<LockdownManager*>(this)->m_active = false;
        log::info(TAG, "Lockdown period expired. System unlocked.");
        return false;
    }

    return true;
}

Seconds LockdownManager::get_remaining_time() const {
    if (!m_active) return Seconds(0);
    
    auto remaining = std::chrono::duration_cast<Seconds>(m_end_time - Clock::now());
    return remaining.count() > 0 ? remaining : Seconds(0);
}

bool LockdownManager::is_app_allowed(AppId app_id) const {
    if (!is_active()) return true;

    return std::find(m_whitelist.begin(), m_whitelist.end(), app_id) != m_whitelist.end();
}

void LockdownManager::force_unlock() {
    m_active = false;
    log::warn(TAG, "System FORCE UNLOCKED");
}

} // namespace vos
