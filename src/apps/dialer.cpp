#include "dialer.h"
#include "vos/log.h"

namespace vos {

static const char* TAG = "Dialer";

Dialer::Dialer() = default;

Result<void> Dialer::init() {
    log::info(TAG, "Dialer app initialized");
    return Result<void>::success();
}

Result<void> Dialer::dial(const std::string& number) {
    if (m_current_state != CallState::IDLE) {
        return Result<void>::error(StatusCode::ERR_ALREADY_EXISTS);
    }
    if (number.empty()) {
        return Result<void>::error(StatusCode::ERR_INVALID_ARG);
    }

    m_current_number = number;
    m_current_state  = CallState::DIALING;
    m_dial_start     = Clock::now();

    log::info(TAG, "Dialing %s...", number.c_str());
    return Result<void>::success();
}

Result<void> Dialer::hang_up() {
    if (m_current_state == CallState::IDLE || m_current_state == CallState::ENDED) {
        return Result<void>::error(StatusCode::ERR_INVALID_ARG);
    }

    // Record the call
    CallRecord rec;
    rec.number     = m_current_number;
    rec.state      = CallState::ENDED;
    rec.start_time = m_call_start;
    rec.end_time   = Clock::now();
    rec.outgoing   = true;
    m_history.push_back(rec);

    log::info(TAG, "Call ended with %s (duration: %ds)",
              m_current_number.c_str(), get_call_duration());

    m_current_state  = CallState::IDLE;
    m_current_number = "";
    return Result<void>::success();
}

int Dialer::get_call_duration() const {
    if (m_current_state == CallState::IN_CALL) {
        return (int)std::chrono::duration_cast<Seconds>(Clock::now() - m_call_start).count();
    }
    return 0;
}

void Dialer::tick() {
    if (m_current_state == CallState::DIALING) {
        // Simulate 2-second dialing phase
        auto elapsed = std::chrono::duration_cast<Seconds>(Clock::now() - m_dial_start);
        if (elapsed.count() >= 1) {
            m_current_state = CallState::RINGING;
            log::info(TAG, "Ringing %s...", m_current_number.c_str());
        }
    }
    else if (m_current_state == CallState::RINGING) {
        // Simulate 2-second ring phase then auto-answer
        auto elapsed = std::chrono::duration_cast<Seconds>(Clock::now() - m_dial_start);
        if (elapsed.count() >= 3) {
            m_current_state = CallState::IN_CALL;
            m_call_start    = Clock::now();
            log::info(TAG, "Connected to %s", m_current_number.c_str());
        }
    }
}

} // namespace vos
