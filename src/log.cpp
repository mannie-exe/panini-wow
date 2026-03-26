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

void LogInfo(const char* fmt, ...) {
    if (!g_logFile) return;
    va_list args;
    va_start(args, fmt);
    fprintf(g_logFile, "[INFO] ");
    vfprintf(g_logFile, fmt, args);
    fprintf(g_logFile, "\n");
    fflush(g_logFile);
    va_end(args);
}

void LogWarn(const char* fmt, ...) {
    if (!g_logFile) return;
    va_list args;
    va_start(args, fmt);
    fprintf(g_logFile, "[WARN] ");
    vfprintf(g_logFile, fmt, args);
    fprintf(g_logFile, "\n");
    fflush(g_logFile);
    va_end(args);
}

void LogError(const char* fmt, ...) {
    if (!g_logFile) return;
    va_list args;
    va_start(args, fmt);
    fprintf(g_logFile, "[ERROR] ");
    vfprintf(g_logFile, fmt, args);
    fprintf(g_logFile, "\n");
    fflush(g_logFile);
    va_end(args);
}
