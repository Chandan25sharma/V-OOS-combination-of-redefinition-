/*
 * VOS — Linux Platform Abstraction
 * Platform-specific utilities for Linux.
 */

#ifndef _WIN32
#ifndef __ANDROID__

#include "vos/types.h"
#include "vos/log.h"
#include <unistd.h>
#include <ctime>
#include <cstring>

namespace vos {
namespace platform {

static const char* TAG = "Linux";

uint64_t get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

std::string get_device_id() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        return std::string("LNX_") + hostname;
    }
    return "LNX_UNKNOWN";
}

void sleep_ms(int ms) {
    usleep(ms * 1000);
}

bool is_elevated() {
    return getuid() == 0;
}

void init_platform() {
    log::info(TAG, "Linux platform initialized");
    log::info(TAG, "Device ID: %s", get_device_id().c_str());
    log::info(TAG, "Root: %s", is_elevated() ? "YES" : "NO");
}

} // namespace platform
} // namespace vos

#endif // __ANDROID__
#endif // _WIN32
