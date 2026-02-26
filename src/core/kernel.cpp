#include "kernel.h"
#include "vos/log.h"

namespace vos {

static const char* TAG = "Kernel";

Kernel::Kernel() = default;

Kernel::~Kernel() {
    shutdown();
}

Result<void> Kernel::init() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_running.load()) {
        return Result<void>::error(StatusCode::ERR_ALREADY_EXISTS);
    }
    m_running.store(true);
    log::info(TAG, "Kernel initialized");
    return Result<void>::success();
}

void Kernel::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running.load()) return;

    m_running.store(false);
    // Terminate all processes
    for (auto& [pid, info] : m_processes) {
        info.state = ProcessState::TERMINATED;
    }
    log::info(TAG, "Kernel shutdown — %zu processes terminated",
              m_processes.size());
    m_processes.clear();
    m_tick_fns.clear();
}

Result<ProcessId> Kernel::spawn(AppId app_id, const std::string& name, ProcessTickFn tick_fn) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running.load()) {
        return Result<ProcessId>::error(StatusCode::ERR_NOT_INITIALIZED);
    }

    ProcessId pid = m_next_pid++;
    ProcessInfo info;
    info.pid        = pid;
    info.app_id     = app_id;
    info.name       = name;
    info.state      = ProcessState::READY;
    info.start_time = Clock::now();

    m_processes[pid] = info;
    m_tick_fns[pid]  = std::move(tick_fn);

    log::info(TAG, "Spawned process [%u] '%s' (app=%u)", pid, name.c_str(), app_id);
    return Result<ProcessId>::success(pid);
}

Result<void> Kernel::kill(ProcessId pid) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_processes.find(pid);
    if (it == m_processes.end()) {
        return Result<void>::error(StatusCode::ERR_NOT_FOUND);
    }
    log::info(TAG, "Killed process [%u] '%s'", pid, it->second.name.c_str());
    it->second.state = ProcessState::TERMINATED;
    m_tick_fns.erase(pid);
    m_processes.erase(it);
    return Result<void>::success();
}

Result<void> Kernel::suspend(ProcessId pid) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_processes.find(pid);
    if (it == m_processes.end()) {
        return Result<void>::error(StatusCode::ERR_NOT_FOUND);
    }
    it->second.state = ProcessState::SUSPENDED;
    log::info(TAG, "Suspended process [%u]", pid);
    return Result<void>::success();
}

Result<void> Kernel::resume(ProcessId pid) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_processes.find(pid);
    if (it == m_processes.end()) {
        return Result<void>::error(StatusCode::ERR_NOT_FOUND);
    }
    it->second.state = ProcessState::READY;
    log::info(TAG, "Resumed process [%u]", pid);
    return Result<void>::success();
}

Result<ProcessInfo> Kernel::get_process(ProcessId pid) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_processes.find(pid);
    if (it == m_processes.end()) {
        return Result<ProcessInfo>::error(StatusCode::ERR_NOT_FOUND);
    }
    return Result<ProcessInfo>::success(it->second);
}

std::vector<ProcessInfo> Kernel::list_processes() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ProcessInfo> result;
    result.reserve(m_processes.size());
    for (const auto& [pid, info] : m_processes) {
        result.push_back(info);
    }
    return result;
}

void Kernel::tick() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running.load()) return;

    for (auto& [pid, info] : m_processes) {
        if (info.state == ProcessState::READY || info.state == ProcessState::RUNNING) {
            info.state = ProcessState::RUNNING;
            auto fn_it = m_tick_fns.find(pid);
            if (fn_it != m_tick_fns.end() && fn_it->second) {
                fn_it->second(pid);
            }
        }
    }
}

} // namespace vos
