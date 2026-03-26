#include "panini.h"

FILE* g_logFile = nullptr;

void LogInit() {
    g_logFile = fopen("mods\\PaniniWoW.log", "w");
}

void LogShutdown() {
    if (g_logFile) {
        fclose(g_logFile);
        g_logFile = nullptr;
    }
}
