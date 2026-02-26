#pragma once

#include "vos/types.h"
#include <string>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>

namespace vos {

enum class ProcessState {
    READY,
    RUNNING,
    SUSPENDED,
    TERMINATED
};

struct ProcessInfo {
    ProcessId    pid;
    AppId        app_id;
    std::string  name;
    ProcessState state;
    TimePoint    start_time;
};

// Callback for process main loop tick
using ProcessTickFn = std::function<void(ProcessId)>;

class Kernel {
public:
    Kernel();
    ~Kernel();

    // Initialize the kernel
    Result<void> init();

    // Shutdown cleanly
    void shutdown();

    // Process management
    Result<ProcessId> spawn(AppId app_id, const std::string& name, ProcessTickFn tick_fn);
    Result<void>      kill(ProcessId pid);
    Result<void>      suspend(ProcessId pid);
    Result<void>      resume(ProcessId pid);

    // Query
    Result<ProcessInfo> get_process(ProcessId pid) const;
    std::vector<ProcessInfo> list_processes() const;

    // Run one scheduler tick (call from main loop)
    void tick();

    // Is the kernel running?
    bool is_running() const { return m_running.load(); }

private:
    mutable std::mutex                        m_mutex;
    std::unordered_map<ProcessId, ProcessInfo> m_processes;
    std::unordered_map<ProcessId, ProcessTickFn> m_tick_fns;
    std::atomic<bool>                         m_running{false};
    ProcessId                                 m_next_pid{1};
};

} // namespace vos
