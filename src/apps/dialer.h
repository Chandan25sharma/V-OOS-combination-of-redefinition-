#pragma once

#include "vos/types.h"
#include <string>
#include <vector>
#include <chrono>

namespace vos {

// ─── Call Record ─────────────────────────────────────────────
enum class CallState {
    IDLE,
    DIALING,
    RINGING,
    IN_CALL,
    ENDED,
    MISSED
};

struct CallRecord {
    std::string number;
    CallState   state;
    TimePoint   start_time;
    TimePoint   end_time;
    bool        outgoing;
};

// ─── Dialer App ──────────────────────────────────────────────
class Dialer {
public:
    Dialer();
    ~Dialer() = default;

    Result<void> init();

    // Make a call (simulated on desktop)
    Result<void> dial(const std::string& number);

    // End the current call
    Result<void> hang_up();

    // Get current call state
    CallState get_state() const { return m_current_state; }
    std::string get_current_number() const { return m_current_number; }

    // Get call duration in seconds
    int get_call_duration() const;

    // Call history
    const std::vector<CallRecord>& get_history() const { return m_history; }
    void clear_history() { m_history.clear(); }

    // Tick — called from main loop for state transitions
    void tick();

private:
    CallState   m_current_state = CallState::IDLE;
    std::string m_current_number;
    TimePoint   m_call_start;
    TimePoint   m_dial_start;
    std::vector<CallRecord> m_history;
};

} // namespace vos
