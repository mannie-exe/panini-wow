#include "panini.h"
#include "trampoline_pool.h"
#include "version.g.h"

HMODULE g_hModule = NULL;

const WowOffsets* g_offsets = nullptr;

bool DetectWowVersion() {
    HMODULE hExe = GetModuleHandleA(NULL);
    if (!hExe) {
        g_offsets = &kClassic;
        return true;
    }

    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(hExe);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(
        reinterpret_cast<uint8_t*>(hExe) + dos->e_lfanew);

    DWORD timestamp = nt->FileHeader.TimeDateStamp;

    // Classic 1.12.1 (2006): ~0x45000000, WotLK 3.3.5a (2010): ~0x4C200000.
    // Threshold 0x48000000 (~March 2008) separates the two eras.
    if (timestamp >= 0x48000000) {
        g_offsets = &kWotLK;
    } else {
        g_offsets = &kClassic;
    }
    return true;
}

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
    DetectWowVersion();
    InitVersionOps();

    LOG_INFO("init", "PaniniWoW " PANINI_VERSION " initializing (SSE2 math)");
    LOG_INFO("init", "PE timestamp: 0x%08X, version: %s",
        []() -> DWORD {
            HMODULE h = GetModuleHandleA(NULL);
            if (!h) return 0;
            auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(h);
            auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(
                reinterpret_cast<uint8_t*>(h) + dos->e_lfanew);
            return nt->FileHeader.TimeDateStamp;
        }(),
        g_offsets->version == WowVersion::WotLK335 ? "WotLK 3.3.5a" : "Classic 1.12.1");

    if (!InstallHooks()) {
        LOG_INFO("init", "D3D hook installation failed");
        return 1;
    }

    CVar_RegisterAll();

    if (!TrampolinePool_Init()) {
        LOG_INFO("init", "trampoline pool allocation failed");
        return 1;
    }
    LOG_INFO("init", "trampoline pool at %p", g_trampolinePool);

    InstallRenderWorldHook();

    LOG_INFO("init", "hook strategy: RenderWorld trampoline + EndScene");
    LOG_INFO("init", "ready");
    return 0;
}

static void Cleanup() {
    LOG_INFO("init", "shutting down");
    TrampolinePool_Shutdown();
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
