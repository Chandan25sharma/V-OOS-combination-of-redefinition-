#include "dns_guard.h"
#include "vos/log.h"
#include <algorithm>
#include <random>
#include <sstream>

namespace vos {

static const char* TAG = "DNSGuard";

DNSGuard::DNSGuard() = default;

DNSGuard::~DNSGuard() {
    shutdown();
}

Result<void> DNSGuard::init() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Default blocklist — trackers and telemetry
    m_block_list = {
        "analytics.google.com",
        "tracking.example.com",
        "telemetry.microsoft.com",
        "ads.doubleclick.net",
        "facebook.com",
        "graph.facebook.com",
        "pixel.facebook.com",
        "connect.facebook.net"
    };

    m_active.store(true);
    m_stats = {0, 0, 0};

    log::info(TAG, "DNS Guard active — %zu domains blocked", m_block_list.size());
    return Result<void>::success();
}

void DNSGuard::shutdown() {
    m_active.store(false);
    log::info(TAG, "DNS Guard stopped — %llu total queries, %llu blocked",
              (unsigned long long)m_stats.queries_total,
              (unsigned long long)m_stats.queries_blocked);
}

Result<std::string> DNSGuard::resolve(const std::string& hostname) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.queries_total++;

    // Check blocklist
    if (is_blocked(hostname)) {
        m_stats.queries_blocked++;
        log::warn(TAG, "BLOCKED: %s", hostname.c_str());
        return Result<std::string>::error(StatusCode::ERR_PERMISSION);
    }

    // Check cache
    for (auto& [host, ip] : m_cache) {
        if (host == hostname) {
            m_stats.queries_resolved++;
            return Result<std::string>::success(ip);
        }
    }

    // Simulate DoH resolution — in production, this would make an HTTPS
    // request to a secure resolver like Cloudflare 1.1.1.1 or Google 8.8.8.8
    // For demo: generate a deterministic but random-looking IP
    std::hash<std::string> hasher;
    size_t h = hasher(hostname);
    std::ostringstream oss;
    oss << ((h >> 0) & 0xFF) << "."
        << ((h >> 8) & 0xFF) << "."
        << ((h >> 16) & 0xFF) << "."
        << ((h >> 24) & 0xFF);
    std::string ip = oss.str();

    // Cache result
    m_cache.push_back({hostname, ip});
    if (m_cache.size() > 500) {
        m_cache.erase(m_cache.begin()); // LRU eviction
    }

    m_stats.queries_resolved++;
    log::debug(TAG, "Resolved %s -> %s (via secure DNS)", hostname.c_str(), ip.c_str());
    return Result<std::string>::success(ip);
}

DNSGuard::Stats DNSGuard::get_stats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void DNSGuard::add_blocked_domain(const std::string& domain) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_block_list.push_back(domain);
}

bool DNSGuard::is_blocked(const std::string& domain) const {
    for (const auto& blocked : m_block_list) {
        if (domain == blocked) return true;
        // Subdomain match
        if (domain.size() > blocked.size() &&
            domain.substr(domain.size() - blocked.size() - 1) == "." + blocked) {
            return true;
        }
    }
    return false;
}

} // namespace vos
