#include "notifications.h"
#include "vos/log.h"
#include <chrono>
#include <algorithm>

namespace vos {

static const char* TAG = "Notify";

NotificationManager::NotificationManager() = default;

Result<void> NotificationManager::init() {
    log::info(TAG, "Notification manager initialized");
    return Result<void>::success();
}

uint32_t NotificationManager::push(NotificationType type, const std::string& title,
                                    const std::string& message, float duration_sec) {
    std::lock_guard<std::mutex> lock(m_mutex);

    Notification n;
    n.id           = m_next_id++;
    n.type         = type;
    n.title        = title;
    n.message      = message;
    n.created      = Clock::now();
    n.duration_sec = duration_sec;
    n.dismissed    = false;

    m_notifications.push_back(n);

    // Keep max 50
    while (m_notifications.size() > 50) {
        m_notifications.pop_front();
    }

    log::debug(TAG, "[%s] %s: %s", title.c_str(), message.c_str(),
               duration_sec > 0 ? "auto-dismiss" : "manual-dismiss");

    for (auto& cb : m_callbacks) {
        if (cb) cb(n);
    }

    return n.id;
}

void NotificationManager::dismiss(uint32_t id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& n : m_notifications) {
        if (n.id == id) { n.dismissed = true; break; }
    }
}

void NotificationManager::dismiss_all() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& n : m_notifications) {
        n.dismissed = true;
    }
}

std::vector<Notification> NotificationManager::get_active() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Notification> active;
    auto now = Clock::now();

    for (const auto& n : m_notifications) {
        if (n.dismissed) continue;

        if (n.duration_sec > 0) {
            auto elapsed = std::chrono::duration_cast<Millis>(now - n.created).count();
            if (elapsed > (long long)(n.duration_sec * 1000)) continue; // expired
        }

        active.push_back(n);
    }
    return active;
}

void NotificationManager::tick() {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto now = Clock::now();

    for (auto& n : m_notifications) {
        if (n.dismissed) continue;
        if (n.duration_sec > 0) {
            auto elapsed = std::chrono::duration_cast<Millis>(now - n.created).count();
            if (elapsed > (long long)(n.duration_sec * 1000)) {
                n.dismissed = true;
            }
        }
    }
}

void NotificationManager::on_notification(NotificationFn fn) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks.push_back(std::move(fn));
}

} // namespace vos
