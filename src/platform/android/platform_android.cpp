/*
 * VOS — Android Platform Abstraction
 * Platform-specific utilities for Android NDK.
 */

#ifdef __ANDROID__

#include "vos/types.h"
#include <android/log.h>
#include <ctime>
#include <cstring>
#include <unistd.h>
#include <sys/system_properties.h>

#define LOG_TAG "VOS_Platform"

namespace vos {
namespace platform {

uint64_t get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

std::string get_device_id() {
    char model[PROP_VALUE_MAX] = {0};
    __system_property_get("ro.product.model", model);
    if (model[0]) {
        return std::string("ANDROID_") + model;
    }
    return "ANDROID_UNKNOWN";
}

void sleep_ms(int ms) {
    usleep(ms * 1000);
}

bool is_elevated() {
    return getuid() == 0; // root
}

void init_platform() {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                        "Android platform initialized");
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                        "Device: %s", get_device_id().c_str());
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                        "Root: %s", is_elevated() ? "YES" : "NO");
}

} // namespace platform
} // namespace vos

#endif // __ANDROID__
