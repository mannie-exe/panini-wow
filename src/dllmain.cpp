#include "panini.h"

// Global state
PaniniConfig g_config = {};
HMODULE      g_hModule = NULL;

static DWORD WINAPI InitThread(LPVOID) {
    LogInit();
    LogInfo("PaniniWoW v%s initializing...", "0.1.0");

    ConfigSetDefaults(&g_config);
    LoadConfig("WTF\\panini.conf", &g_config);

    // TODO: Discover D3D9 device, hook EndScene/Reset
    LogInfo("PaniniWoW init complete (hooks not yet installed).");
    return 0;
}

static void Cleanup() {
    LogInfo("PaniniWoW shutting down.");
    LogShutdown();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        g_hModule = hModule;
        CreateThread(NULL, 0, InitThread, NULL, 0, NULL);
        return TRUE;

    case DLL_PROCESS_DETACH:
        Cleanup();
        return TRUE;
    }
    return TRUE;
}
