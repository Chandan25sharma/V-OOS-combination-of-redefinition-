/*
 * VOS — Win32 Platform Abstraction
 * Platform-specific utilities for Windows.
 */

#ifdef _WIN32

#include "vos/types.h"
#include "vos/log.h"
#include <windows.h>
#include <cstdlib>

namespace vos {
namespace platform {

static const char* TAG = "Win32";

// Get a high-resolution timestamp in milliseconds
uint64_t get_time_ms() {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (uint64_t)(counter.QuadPart * 1000 / freq.QuadPart);
}

// Generate a unique device ID based on machine info
std::string get_device_id() {
    char buf[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(buf);
    if (GetComputerNameA(buf, &size)) {
        return std::string("WIN_") + buf;
    }
    return "WIN_UNKNOWN";
}

// Set console title
void set_console_title(const std::string& title) {
    SetConsoleTitleA(title.c_str());
}

// Sleep in milliseconds (platform-accurate)
void sleep_ms(int ms) {
    Sleep(ms);
}

// Check if running as admin
bool is_elevated() {
    BOOL fRet = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
            fRet = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    return fRet != 0;
}

void init_platform() {
    log::info(TAG, "Win32 platform initialized");
    log::info(TAG, "Device ID: %s", get_device_id().c_str());
    log::info(TAG, "Elevated: %s", is_elevated() ? "YES" : "NO");
}

} // namespace platform
} // namespace vos

#endif // _WIN32
