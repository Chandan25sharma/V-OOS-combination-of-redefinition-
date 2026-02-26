#pragma once

#include "vos/types.h"
#include <string>
#include <deque>
#include <mutex>
#include <functional>
#include <chrono>
#include <ctime>

namespace vos {

/**
 * Event Logger — Audit Trail
 * Records all significant system events for security auditing.
 */

enum class EventSeverity {
    DEBUG,
    INFO,
    WARNING,
    SECURITY,
    CRITICAL
};

struct SystemEvent {
    uint64_t       id;
    EventSeverity  severity;
    std::string    source;     // Module that generated the event
    std::string    message;
    TimePoint      timestamp;
};

using EventCallback = std::function<void(const SystemEvent&)>;

class EventLogger {
public:
    EventLogger();
    ~EventLogger() = default;

    Result<void> init(size_t max_events = 1000);

    // Log events
    void log_event(EventSeverity severity, const std::string& source,
                   const std::string& message);

    // Convenience methods
    void info(const std::string& src, const std::string& msg)     { log_event(EventSeverity::INFO, src, msg); }
    void warn(const std::string& src, const std::string& msg)     { log_event(EventSeverity::WARNING, src, msg); }
    void security(const std::string& src, const std::string& msg) { log_event(EventSeverity::SECURITY, src, msg); }
    void critical(const std::string& src, const std::string& msg) { log_event(EventSeverity::CRITICAL, src, msg); }

    // Query
    std::deque<SystemEvent> get_recent(size_t count = 50) const;
    std::deque<SystemEvent> get_by_severity(EventSeverity min_severity) const;
    size_t                  total_events() const;

    // Register callback for real-time events
    void on_event(EventCallback fn);

    // Clear log
    void clear();

private:
    mutable std::mutex          m_mutex;
    std::deque<SystemEvent>     m_events;
    std::vector<EventCallback>  m_callbacks;
    size_t                      m_max_events{1000};
    uint64_t                    m_next_id{1};
};

} // namespace vos
