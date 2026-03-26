#include "panini.h"
#include <cstdio>
#include <cstdarg>

static FILE* g_logFile = nullptr;

void LogInit() {
    g_logFile = fopen("mods\\PaniniWoW.log", "w");
}

void LogShutdown() {
    if (g_logFile) {
        fclose(g_logFile);
        g_logFile = nullptr;
    }
}

void LogInfo(const char* mod, const char* fmt, ...) {
    if (!g_logFile) return;
    fprintf(g_logFile, "[INFO] [%s] ", mod);
    va_list args;
    va_start(args, fmt);
    vfprintf(g_logFile, fmt, args);
    va_end(args);
    fprintf(g_logFile, "\n");
    fflush(g_logFile);
}

void LogTrace(const char* mod, const char* fmt, ...) {
#ifndef NDEBUG
    if (!g_logFile) return;
    fprintf(g_logFile, "[TRACE] [%s] ", mod);
    va_list args;
    va_start(args, fmt);
    vfprintf(g_logFile, fmt, args);
    va_end(args);
    fprintf(g_logFile, "\n");
    fflush(g_logFile);
#else
    (void)mod; (void)fmt;
#endif
}
