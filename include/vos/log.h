#pragma once

/*
 * VOS — Lightweight logging
 */

#include <cstdio>
#include <cstdarg>

namespace vos {
namespace log {

enum class Level { DEBUG, INFO, WARN, ERR };

inline Level g_min_level = Level::DEBUG;

inline const char* level_str(Level l) {
    switch (l) {
        case Level::DEBUG: return "DBG";
        case Level::INFO:  return "INF";
        case Level::WARN:  return "WRN";
        case Level::ERR:   return "ERR";
    }
    return "???";
}

inline void vlog(Level level, const char* tag, const char* fmt, va_list args) {
    if (level < g_min_level) return;
    fprintf(stderr, "[VOS][%s][%s] ", level_str(level), tag);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    fflush(stderr);
}

inline void debug(const char* tag, const char* fmt, ...) {
    va_list a; va_start(a, fmt); vlog(Level::DEBUG, tag, fmt, a); va_end(a);
}
inline void info(const char* tag, const char* fmt, ...) {
    va_list a; va_start(a, fmt); vlog(Level::INFO, tag, fmt, a); va_end(a);
}
inline void warn(const char* tag, const char* fmt, ...) {
    va_list a; va_start(a, fmt); vlog(Level::WARN, tag, fmt, a); va_end(a);
}
inline void error(const char* tag, const char* fmt, ...) {
    va_list a; va_start(a, fmt); vlog(Level::ERR, tag, fmt, a); va_end(a);
}

} // namespace log
} // namespace vos
