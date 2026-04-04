#pragma once

#include <cstdio>
#include <cstdint>
#include <cstring>

// Global log file handle. Set by LogInit, cleared by LogShutdown.
extern FILE* g_logFile;

// Reinterprets a float's bit pattern as uint32_t for safe printf under MinGW/Wine,
// where varargs float promotion produces garbage with %f.
static inline uint32_t FloatBits(float f) {
    uint32_t u;
    memcpy(&u, &f, sizeof(u));
    return u;
}

// Writes a timestamped INFO line to the log file. No-op if g_logFile is null.
#define LOG_INFO(mod, fmt, ...) \
    do { if (g_logFile) { fprintf(g_logFile, "[INFO] [%s] " fmt "\n", mod, ##__VA_ARGS__); fflush(g_logFile); } } while(0)

// Writes a timestamped DEBUG line. Compiled out unless PANINI_DEBUG_LOG is defined.
#ifdef PANINI_DEBUG_LOG
#define LOG_DEBUG(mod, fmt, ...) \
    do { if (g_logFile) { fprintf(g_logFile, "[DEBUG] [%s] " fmt "\n", mod, ##__VA_ARGS__); fflush(g_logFile); } } while(0)
#else
#define LOG_DEBUG(mod, fmt, ...) ((void)0)
#endif
