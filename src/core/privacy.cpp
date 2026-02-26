#include "privacy.h"
#include "vos/log.h"
#include <random>
#include <sstream>
#include <iomanip>

namespace vos {

static const char* TAG = "Privacy";

PrivacyEngine::PrivacyEngine() = default;

PrivacyEngine::~PrivacyEngine() {
    shutdown();
}

Result<void> PrivacyEngine::init(int rotation_interval_sec) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_running.load()) {
        return Result<void>::error(StatusCode::ERR_ALREADY_EXISTS);
    }

    m_interval_sec = rotation_interval_sec;
    m_state.rotation_count = 0;

    // Generate initial identity
    rotate_identity();

    // Start background rotation thread
    m_running.store(true);
    m_thread = std::thread(&PrivacyEngine::rotation_loop, this);

    log::info(TAG, "Privacy engine started — rotating every %d seconds", m_interval_sec);
    return Result<void>::success();
}

void PrivacyEngine::shutdown() {
    if (!m_running.load()) return;
    m_running.store(false);
    if (m_thread.joinable()) {
        m_thread.join();
    }
    log::info(TAG, "Privacy engine stopped after %llu rotations",
              (unsigned long long)m_state.rotation_count);
}

IdentityState PrivacyEngine::get_current_identity() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

void PrivacyEngine::force_rotate() {
    std::lock_guard<std::mutex> lock(m_mutex);
    rotate_identity();
    log::info(TAG, "Forced identity rotation #%llu",
              (unsigned long long)m_state.rotation_count);
}

void PrivacyEngine::on_identity_changed(IdentityChangedFn fn) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks.push_back(std::move(fn));
}

void PrivacyEngine::rotation_loop() {
    while (m_running.load()) {
        // Sleep in small increments so we can exit quickly on shutdown
        for (int i = 0; i < m_interval_sec * 10 && m_running.load(); i++) {
            std::this_thread::sleep_for(Millis(100));
        }

        if (!m_running.load()) break;

        std::lock_guard<std::mutex> lock(m_mutex);
        rotate_identity();

        // Notify callbacks
        for (auto& cb : m_callbacks) {
            if (cb) cb(m_state);
        }
    }
}

void PrivacyEngine::rotate_identity() {
    m_state.virtual_ip     = generate_random_ip();
    m_state.virtual_mac    = generate_random_mac();
    m_state.last_rotation  = Clock::now();
    m_state.rotation_count++;

    log::info(TAG, "Identity #%llu — IP: %s  MAC: %s",
              (unsigned long long)m_state.rotation_count,
              m_state.virtual_ip.c_str(),
              m_state.virtual_mac.c_str());
}

std::string PrivacyEngine::generate_random_ip() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(1, 254);

    // Generate a random private IP (10.x.x.x range)
    std::ostringstream oss;
    oss << "10." << dist(gen) << "." << dist(gen) << "." << dist(gen);
    return oss.str();
}

std::string PrivacyEngine::generate_random_mac() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);

    std::ostringstream oss;
    for (int i = 0; i < 6; i++) {
        if (i > 0) oss << ":";
        int byte = dist(gen);
        // Set locally administered bit on first byte
        if (i == 0) byte = (byte | 0x02) & 0xFE;
        oss << std::hex << std::setw(2) << std::setfill('0') << byte;
    }
    return oss.str();
}

} // namespace vos
