#include "panini.h"

HMODULE g_hModule = NULL;

constexpr int VTABLE_ENDSCENE = 42;
constexpr int VTABLE_RESET    = 16;

static bool InstallHooks() {
    IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!pD3D) {
        LogInfo("init", "Direct3DCreate9 failed");
        return false;
    }

    // Wait for WoW's window. DLLs loaded via dlls.txt run early; the game
    // window may not exist for several seconds after DLL_PROCESS_ATTACH.
    HWND hwnd = NULL;
    for (int i = 0; i < 120 && !hwnd; i++) {
        hwnd = FindWindowA("GxWindowClass", NULL);
        if (!hwnd) {
            // Wine may register a different class; try by window title.
            hwnd = FindWindowA(NULL, "World of Warcraft");
        }
        if (!hwnd) Sleep(500);
    }
    if (!hwnd) {
        LogInfo("init", "WoW window not found after 60s");
        pD3D->Release();
        return false;
    }
    LogInfo("init", "found WoW window HWND=%p", hwnd);

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed         = TRUE;
    pp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow    = hwnd;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.BackBufferCount  = 1;

    IDirect3DDevice9* pDummy = NULL;
    HRESULT hr = pD3D->CreateDevice(
        D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &pDummy
    );
    if (FAILED(hr)) {
        LogInfo("init", "CreateDevice failed hr=0x%08X", (unsigned)hr);
        pD3D->Release();
        return false;
    }

    void** vtable = *reinterpret_cast<void***>(pDummy);
    g_pOriginalEndScene = reinterpret_cast<EndScene_t>(vtable[VTABLE_ENDSCENE]);
    g_pOriginalReset    = reinterpret_cast<Reset_t>(vtable[VTABLE_RESET]);

    LogInfo("init", "vtable=%p, origEndScene=%p, origReset=%p",
            vtable, g_pOriginalEndScene, g_pOriginalReset);

    DWORD oldProt;
    VirtualProtect(&vtable[VTABLE_ENDSCENE], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt);
    vtable[VTABLE_ENDSCENE] = reinterpret_cast<void*>(&Hooked_EndScene);
    VirtualProtect(&vtable[VTABLE_ENDSCENE], sizeof(void*), oldProt, &oldProt);

    VirtualProtect(&vtable[VTABLE_RESET], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt);
    vtable[VTABLE_RESET] = reinterpret_cast<void*>(&Hooked_Reset);
    VirtualProtect(&vtable[VTABLE_RESET], sizeof(void*), oldProt, &oldProt);

    LogInfo("init", "hooks installed: EndScene->%p, Reset->%p",
            &Hooked_EndScene, &Hooked_Reset);

    pDummy->Release();
    pD3D->Release();
    return true;
}

static DWORD WINAPI InitThread(LPVOID) {
    LogInit();
    LogInfo("init", "PaniniWoW v0.1.0 initializing");

    CVar_RegisterAll();

    // Give the game time to create its D3D9 device and window.
    // PaniniWoW should be last in dlls.txt so other mods load first.
    Sleep(8000);

    if (!InstallHooks()) {
        LogInfo("init", "hook installation failed; panini pass will not run");
        return 1;
    }

    LogInfo("init", "ready, waiting for first EndScene");
    return 0;
}

static void Cleanup() {
    LogInfo("init", "shutting down");
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
