#include "panini.h"
#include "version.g.h"

HMODULE g_hModule = NULL;

constexpr int VTABLE_ENDSCENE = 42;
constexpr int VTABLE_RESET    = 16;

static bool InstallHooks() {
    IDirect3DDevice9* dev = nullptr;
    for (int i = 0; i < 600 && !dev; i++) {
        dev = GetWoWDevice();
        if (!dev) Sleep(100);
    }
    if (!dev) {
        LOG_INFO("init", "WoW D3D9 device not found after 60s");
        return false;
    }
    LOG_INFO("init", "found WoW device at %p", dev);

    void** vtable = *reinterpret_cast<void***>(dev);
    g_pOriginalEndScene = reinterpret_cast<EndScene_t>(vtable[VTABLE_ENDSCENE]);
    g_pOriginalReset    = reinterpret_cast<Reset_t>(vtable[VTABLE_RESET]);

    LOG_INFO("init", "vtable=%p origEndScene=%p origReset=%p",
            vtable, g_pOriginalEndScene, g_pOriginalReset);

    DWORD oldProt;
    VirtualProtect(&vtable[VTABLE_ENDSCENE], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt);
    vtable[VTABLE_ENDSCENE] = reinterpret_cast<void*>(&Hooked_EndScene);
    VirtualProtect(&vtable[VTABLE_ENDSCENE], sizeof(void*), oldProt, &oldProt);

    VirtualProtect(&vtable[VTABLE_RESET], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt);
    vtable[VTABLE_RESET] = reinterpret_cast<void*>(&Hooked_Reset);
    VirtualProtect(&vtable[VTABLE_RESET], sizeof(void*), oldProt, &oldProt);

    LOG_INFO("init", "hooks installed: EndScene->%p Reset->%p",
            &Hooked_EndScene, &Hooked_Reset);

    return true;
}

static DWORD WINAPI InitThread(LPVOID) {
    LogInit();
    LOG_INFO("init", "PaniniClassicWoW " PANINI_VERSION " initializing (SSE2 math)");

    if (!InstallHooks()) {
        LOG_INFO("init", "D3D hook installation failed");
        return 1;
    }

    CVar_RegisterAll();
    InstallRenderWorldHook();

    LOG_INFO("init", "ready");
    return 0;
}

static void Cleanup() {
    LOG_INFO("init", "shutting down");
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
