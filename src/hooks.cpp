#include "panini.h"

EndScene_t g_pOriginalEndScene = nullptr;
Reset_t    g_pOriginalReset    = nullptr;

HRESULT __stdcall Hooked_EndScene(IDirect3DDevice9* pDevice) {
    // TODO: Panini post-process pass
    // For now, passthrough.
    return g_pOriginalEndScene(pDevice);
}

HRESULT __stdcall Hooked_Reset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pParams) {
    // TODO: Release panini resources before Reset, recreate after.
    return g_pOriginalReset(pDevice, pParams);
}
