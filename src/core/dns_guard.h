#pragma once

#include "vos/types.h"
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>

namespace vos {

/**
 * DNS Leak Guard
 * Prevents DNS queries from leaking outside the VOS privacy layer.
 * On desktop: intercepts DNS at the application level.
 * Provides encrypted DNS resolution via DNS-over-HTTPS (DoH).
 */
class DNSGuard {
public:
    DNSGuard();
    ~DNSGuard();

    Result<void> init();
    void shutdown();

    // Resolve a hostname via secure DNS
    Result<std::string> resolve(const std::string& hostname);

    // Check if guard is active
    bool is_active() const { return m_active.load(); }

    // Statistics
    struct Stats {
        uint64_t queries_total;
        uint64_t queries_blocked;
        uint64_t queries_resolved;
    };
    Stats get_stats() const;

    // Block list — domains that should always be blocked
    void add_blocked_domain(const std::string& domain);
    bool is_blocked(const std::string& domain) const;

private:
    mutable std::mutex    m_mutex;
    std::atomic<bool>     m_active{false};
    Stats                 m_stats{0, 0, 0};
    std::vector<std::string> m_block_list;

    // Simulated secure DNS cache
    std::vector<std::pair<std::string, std::string>> m_cache;
};

} // namespace vos
