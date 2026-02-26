#pragma once

#include "vos/types.h"
#include <string>
#include <deque>
#include <mutex>
#include <functional>

namespace vos {

/**
 * In-app Notification System
 * Provides toast-style alerts within the VOS shell.
 */

enum class NotificationType {
    INFO,
    SUCCESS,
    WARNING,
    ERROR,
    SECURITY
};

struct Notification {
    uint32_t         id;
    NotificationType type;
    std::string      title;
    std::string      message;
    TimePoint        created;
    float            duration_sec;   // How long to show (0 = until dismissed)
    bool             dismissed;
};

using NotificationFn = std::function<void(const Notification&)>;

class NotificationManager {
public:
    NotificationManager();
    ~NotificationManager() = default;

    Result<void> init();

    // Push a notification
    uint32_t push(NotificationType type, const std::string& title,
                  const std::string& message, float duration_sec = 5.0f);

    // Convenience
    uint32_t info(const std::string& msg, float dur = 4.0f)
        { return push(NotificationType::INFO, "Info", msg, dur); }
    uint32_t success(const std::string& msg, float dur = 3.0f)
        { return push(NotificationType::SUCCESS, "Success", msg, dur); }
    uint32_t warning(const std::string& msg, float dur = 6.0f)
        { return push(NotificationType::WARNING, "Warning", msg, dur); }
    uint32_t error(const std::string& msg, float dur = 8.0f)
        { return push(NotificationType::ERROR, "Error", msg, dur); }
    uint32_t security_alert(const std::string& msg)
        { return push(NotificationType::SECURITY, "Security", msg, 0); }

    // Dismiss
    void dismiss(uint32_t id);
    void dismiss_all();

    // Get active (non-dismissed, non-expired) notifications
    std::vector<Notification> get_active() const;

    // Tick — auto-dismiss expired notifications
    void tick();

    // Callback
    void on_notification(NotificationFn fn);

private:
    mutable std::mutex            m_mutex;
    std::deque<Notification>      m_notifications;
    std::vector<NotificationFn>   m_callbacks;
    uint32_t                      m_next_id{1};
};

} // namespace vos
