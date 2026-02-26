#include "event_logger.h"
#include "vos/log.h"

namespace vos {

static const char* TAG = "EventLog";

static const char* severity_str(EventSeverity s) {
    switch (s) {
        case EventSeverity::DEBUG:    return "DEBUG";
        case EventSeverity::INFO:     return "INFO";
        case EventSeverity::WARNING:  return "WARN";
        case EventSeverity::SECURITY: return "SECURITY";
        case EventSeverity::CRITICAL: return "CRITICAL";
    }
    return "UNKNOWN";
}

EventLogger::EventLogger() = default;

Result<void> EventLogger::init(size_t max_events) {
    m_max_events = max_events;
    log::info(TAG, "Event logger initialized (max %zu events)", max_events);
    
    // Log the init event itself
    log_event(EventSeverity::INFO, "System", "VOS Event Logger started");
    return Result<void>::success();
}

void EventLogger::log_event(EventSeverity severity, const std::string& source,
                             const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);

    SystemEvent ev;
    ev.id        = m_next_id++;
    ev.severity  = severity;
    ev.source    = source;
    ev.message   = message;
    ev.timestamp = Clock::now();

    m_events.push_back(ev);

    // Trim to max
    while (m_events.size() > m_max_events) {
        m_events.pop_front();
    }

    // Also log to console for SECURITY and CRITICAL
    if (severity >= EventSeverity::SECURITY) {
        log::warn(TAG, "[%s] %s: %s", severity_str(severity),
                  source.c_str(), message.c_str());
    }

    // Notify callbacks
    for (auto& cb : m_callbacks) {
        if (cb) cb(ev);
    }
}

std::deque<SystemEvent> EventLogger::get_recent(size_t count) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (count >= m_events.size()) return m_events;

    std::deque<SystemEvent> result;
    auto it = m_events.end() - (int)count;
    result.assign(it, m_events.end());
    return result;
}

std::deque<SystemEvent> EventLogger::get_by_severity(EventSeverity min_severity) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::deque<SystemEvent> result;
    for (const auto& ev : m_events) {
        if (ev.severity >= min_severity) {
            result.push_back(ev);
        }
    }
    return result;
}

size_t EventLogger::total_events() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_events.size();
}

void EventLogger::on_event(EventCallback fn) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks.push_back(std::move(fn));
}

void EventLogger::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_events.clear();
    log::info(TAG, "Event log cleared");
}

} // namespace vos
