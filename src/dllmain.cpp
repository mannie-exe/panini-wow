#include "panini.h"
#include "version.g.h"

HMODULE g_hModule = NULL;

constexpr int VTABLE_ENDSCENE = 42;
constexpr int VTABLE_RESET    = 16;

static bool InstallHooks() {
    IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!pD3D) {
        LOG_INFO("init", "Direct3DCreate9 failed");
        return false;
    }

    HWND hwnd = NULL;
    for (int i = 0; i < 120 && !hwnd; i++) {
        hwnd = FindWindowA("GxWindowClass", NULL);
        if (!hwnd)
            hwnd = FindWindowA(NULL, "World of Warcraft");
        if (!hwnd) Sleep(500);
    }
    if (!hwnd) {
        LOG_INFO("init", "WoW window not found after 60s");
        pD3D->Release();
        return false;
    }
    LOG_INFO("init", "found WoW window HWND=%p", hwnd);

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
        LOG_INFO("init", "CreateDevice failed hr=0x%08X", (unsigned)hr);
        pD3D->Release();
        return false;
    }

    void** vtable = *reinterpret_cast<void***>(pDummy);
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

    pDummy->Release();
    pD3D->Release();
    return true;
}

static DWORD WINAPI InitThread(LPVOID) {
    LogInit();
    LOG_INFO("init", "PaniniWoW " PANINI_VERSION " initializing (SSE2 math)");

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
