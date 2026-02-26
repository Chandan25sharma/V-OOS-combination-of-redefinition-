#pragma once

/*
 * VOS — Common types and definitions
 */

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <chrono>
#include <mutex>

namespace vos {

// ─── Result Type ─────────────────────────────────────────────
enum class StatusCode {
    OK = 0,
    ERR_NOT_FOUND,
    ERR_PERMISSION,
    ERR_TIMEOUT,
    ERR_IO,
    ERR_NETWORK,
    ERR_CRYPTO,
    ERR_INVALID_ARG,
    ERR_ALREADY_EXISTS,
    ERR_NOT_INITIALIZED,
    ERR_LOCKDOWN_ACTIVE,
    ERR_INTERNAL
};

inline const char* status_to_string(StatusCode s) {
    switch (s) {
        case StatusCode::OK:                 return "OK";
        case StatusCode::ERR_NOT_FOUND:      return "Not Found";
        case StatusCode::ERR_PERMISSION:     return "Permission Denied";
        case StatusCode::ERR_TIMEOUT:        return "Timeout";
        case StatusCode::ERR_IO:             return "I/O Error";
        case StatusCode::ERR_NETWORK:        return "Network Error";
        case StatusCode::ERR_CRYPTO:         return "Crypto Error";
        case StatusCode::ERR_INVALID_ARG:    return "Invalid Argument";
        case StatusCode::ERR_ALREADY_EXISTS: return "Already Exists";
        case StatusCode::ERR_NOT_INITIALIZED:return "Not Initialized";
        case StatusCode::ERR_LOCKDOWN_ACTIVE:return "Lockdown Active";
        case StatusCode::ERR_INTERNAL:       return "Internal Error";
        default:                             return "Unknown";
    }
}

template<typename T>
struct Result {
    StatusCode status;
    T          value;

    bool ok() const { return status == StatusCode::OK; }

    static Result success(T val) { return { StatusCode::OK, std::move(val) }; }
    static Result error(StatusCode s) { return { s, T{} }; }
};

template<>
struct Result<void> {
    StatusCode status;
    bool ok() const { return status == StatusCode::OK; }
    static Result success() { return { StatusCode::OK }; }
    static Result error(StatusCode s) { return { s }; }
};

// ─── Time Aliases ────────────────────────────────────────────
using Clock     = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using Duration  = Clock::duration;
using Seconds   = std::chrono::seconds;
using Millis    = std::chrono::milliseconds;

// ─── Byte Buffer ─────────────────────────────────────────────
using ByteBuffer = std::vector<uint8_t>;

// ─── Process / App IDs ───────────────────────────────────────
using ProcessId = uint32_t;
using AppId     = uint16_t;

constexpr AppId APP_DIALER  = 1;
constexpr AppId APP_SMS     = 2;
constexpr AppId APP_CAMERA  = 3;
constexpr AppId APP_SYSTEM  = 0;

} // namespace vos
