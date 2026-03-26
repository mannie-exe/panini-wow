#pragma once

#include <cstdio>
#include <cstdint>
#include <cstring>

extern FILE* g_logFile;

static inline uint32_t FloatBits(float f) {
    uint32_t u;
    memcpy(&u, &f, sizeof(u));
    return u;
}

#define LOG_INFO(mod, fmt, ...) \
    do { if (g_logFile) { fprintf(g_logFile, "[INFO] [%s] " fmt "\n", mod, ##__VA_ARGS__); fflush(g_logFile); } } while(0)

#ifdef PANINI_DEBUG_LOG
#define LOG_DEBUG(mod, fmt, ...) \
    do { if (g_logFile) { fprintf(g_logFile, "[DEBUG] [%s] " fmt "\n", mod, ##__VA_ARGS__); fflush(g_logFile); } } while(0)
#else
#define LOG_DEBUG(mod, fmt, ...) ((void)0)
#endif
