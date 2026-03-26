#include "panini.h"
#include "panini_shader.h"
#include "debug_shader.h"
#include <cmath>

EndScene_t g_pOriginalEndScene = nullptr;
Reset_t    g_pOriginalReset    = nullptr;

// Render resources (lazy-created, released on Reset).
static IDirect3DTexture9*     g_pSceneTexture  = nullptr;
static IDirect3DSurface9*     g_pSceneSurface  = nullptr;
static IDirect3DPixelShader9* g_pPaniniShader  = nullptr;
static IDirect3DPixelShader9* g_pDebugShader   = nullptr;
static bool                   g_resourcesReady = false;
static UINT                   g_bbW = 0;
static UINT                   g_bbH = 0;

struct ScreenVertex { float x, y, z, rhw, u, v; };

static void ReleaseResources() {
    if (g_pSceneSurface) { g_pSceneSurface->Release(); g_pSceneSurface = nullptr; }
    if (g_pSceneTexture) { g_pSceneTexture->Release(); g_pSceneTexture = nullptr; }
    if (g_pPaniniShader) { g_pPaniniShader->Release(); g_pPaniniShader = nullptr; }
    if (g_pDebugShader)  { g_pDebugShader->Release();  g_pDebugShader  = nullptr; }
    g_resourcesReady = false;
    g_bbW = g_bbH = 0;
}

static bool CreateResources(IDirect3DDevice9* dev) {
    IDirect3DSurface9* pBB = nullptr;
    dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBB);
    if (!pBB) return false;

    D3DSURFACE_DESC desc;
    pBB->GetDesc(&desc);
    pBB->Release();

    g_bbW = desc.Width;
    g_bbH = desc.Height;

    HRESULT hr = dev->CreateTexture(
        desc.Width, desc.Height, 1,
        D3DUSAGE_RENDERTARGET, desc.Format,
        D3DPOOL_DEFAULT, &g_pSceneTexture, NULL);
    if (FAILED(hr)) {
        LogInfo("hook", "CreateTexture failed hr=0x%08X", (unsigned)hr);
        return false;
    }
    g_pSceneTexture->GetSurfaceLevel(0, &g_pSceneSurface);

    hr = dev->CreatePixelShader(
        reinterpret_cast<const DWORD*>(g_paniniShaderBytecode), &g_pPaniniShader);
    if (FAILED(hr)) {
        LogInfo("hook", "CreatePixelShader(panini) failed hr=0x%08X", (unsigned)hr);
        ReleaseResources();
        return false;
    }

    hr = dev->CreatePixelShader(
        reinterpret_cast<const DWORD*>(g_debugShaderBytecode), &g_pDebugShader);
    if (FAILED(hr)) {
        LogInfo("hook", "CreatePixelShader(debug) failed hr=0x%08X", (unsigned)hr);
        ReleaseResources();
        return false;
    }

    LogInfo("hook", "resources ready: %ux%u fmt=%u panini=%p debug=%p",
            desc.Width, desc.Height, desc.Format, g_pPaniniShader, g_pDebugShader);
    g_resourcesReady = true;
    return true;
}

// One-shot diagnostics; logged once then silenced.
static bool g_firstFrame = true;

HRESULT __stdcall Hooked_EndScene(IDirect3DDevice9* dev) {
    if (g_firstFrame) {
        float fov = ReadCameraFov();
        float aspect = ReadCameraAspect();
        LogInfo("hook", "EndScene first call: device=%p fov=%.4f aspect=%.4f",
                dev, (double)fov, (double)aspect);
        g_firstFrame = false;
    }

    PaniniConfig cfg;
    PaniniConfig_ReadFromCVars(&cfg);

    // Clamp to sane ranges; never feed inf/NaN into shaders or camera writes.
    if (cfg.strength != cfg.strength || cfg.strength < 0.0f) cfg.strength = 0.5f;
    if (cfg.strength > 1.0f) cfg.strength = 1.0f;
    if (cfg.verticalComp != cfg.verticalComp) cfg.verticalComp = 0.0f;
    if (cfg.verticalComp < -1.0f) cfg.verticalComp = -1.0f;
    if (cfg.verticalComp > 1.0f) cfg.verticalComp = 1.0f;
    if (cfg.fill != cfg.fill || cfg.fill < 0.0f) cfg.fill = 0.8f;
    if (cfg.fill > 1.0f) cfg.fill = 1.0f;

    if (cfg.enabled) {
        float targetFov = CVar_GetFloat("paniniFov", 2.0f);
        if (targetFov > 0.5f && targetFov < 3.5f)
            WriteCameraFov(targetFov);
    }

    if (!cfg.enabled && !cfg.debug)
        return g_pOriginalEndScene(dev);

    if (!g_resourcesReady) {
        if (!CreateResources(dev))
            return g_pOriginalEndScene(dev);
    }

    // Resolution change check.
    IDirect3DSurface9* pBB = nullptr;
    dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBB);
    if (!pBB) return g_pOriginalEndScene(dev);

    D3DSURFACE_DESC bbDesc;
    pBB->GetDesc(&bbDesc);
    if (bbDesc.Width != g_bbW || bbDesc.Height != g_bbH) {
        pBB->Release();
        ReleaseResources();
        if (!CreateResources(dev))
            return g_pOriginalEndScene(dev);
        dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBB);
        if (!pBB) return g_pOriginalEndScene(dev);
    }

    // Pick shader: debug mode uses the red-reduction test shader,
    // normal mode uses the panini projection shader.
    IDirect3DPixelShader9* activeShader = cfg.debug ? g_pDebugShader : g_pPaniniShader;

    // Compute panini shader constants (harmless if debug shader ignores them).
    float fov     = ReadCameraFov();
    float aspect  = ReadCameraAspect();
    float halfTan = tanf(fov * 0.5f);
    float zoom    = ComputeFillZoom(cfg.strength, halfTan, aspect, cfg.fill);

    float c0[4] = { cfg.strength, halfTan, zoom, 1.0f };
    float c1[4] = { cfg.verticalComp, aspect, 0.0f, 0.0f };

    // Save state.
    D3D9State saved;
    SaveD3D9State(dev, &saved);

    // Copy backbuffer to scene texture.
    dev->StretchRect(pBB, NULL, g_pSceneSurface, NULL, D3DTEXF_NONE);

    // Fullscreen pass.
    dev->SetRenderTarget(0, pBB);
    dev->SetTexture(0, g_pSceneTexture);
    dev->SetPixelShader(activeShader);
    dev->SetVertexShader(NULL);
    dev->SetPixelShaderConstantF(0, c0, 1);
    dev->SetPixelShaderConstantF(1, c1, 1);

    dev->SetRenderState(D3DRS_ZENABLE, FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    dev->SetRenderState(D3DRS_COLORWRITEENABLE,
        D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
        D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);

    dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

    dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

    float w = static_cast<float>(g_bbW);
    float h = static_cast<float>(g_bbH);
    ScreenVertex quad[4] = {
        { -0.5f,     -0.5f,     0.0f, 1.0f, 0.0f, 0.0f },
        {  w - 0.5f, -0.5f,     0.0f, 1.0f, 1.0f, 0.0f },
        { -0.5f,      h - 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
        {  w - 0.5f,  h - 0.5f, 0.0f, 1.0f, 1.0f, 1.0f },
    };

    dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
    dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(ScreenVertex));

    RestoreD3D9State(dev, &saved);
    pBB->Release();

    return g_pOriginalEndScene(dev);
}

HRESULT __stdcall Hooked_Reset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pParams) {
    LogInfo("hook", "Reset called, releasing resources");
    ReleaseResources();
    g_firstFrame = true;
    return g_pOriginalReset(dev, pParams);
}
