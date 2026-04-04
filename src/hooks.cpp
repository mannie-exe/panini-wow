#include "panini.h"
#include "panini_shader.h"
#include "tint_shader.h"
#include "cas_shader.h"
#include "uv_vis_shader.h"
#include "fxaa_shader.h"
#include "trampoline_pool.h"
#include <cmath>

EndScene_t    g_pOriginalEndScene    = nullptr;
Reset_t       g_pOriginalReset       = nullptr;

static IDirect3DTexture9*     g_pSceneTexture      = nullptr;
static IDirect3DSurface9*     g_pSceneSurface      = nullptr;
static IDirect3DTexture9*     g_pPaniniOutputTexture = nullptr;
static IDirect3DSurface9*     g_pPaniniOutputSurface = nullptr;
static IDirect3DPixelShader9* g_pPaniniShader  = nullptr;
static IDirect3DPixelShader9* g_pTintShader    = nullptr;
static IDirect3DPixelShader9* g_pUvVisShader   = nullptr;
static IDirect3DPixelShader9* g_pCasShader     = nullptr;
static IDirect3DPixelShader9* g_pFxaaShader    = nullptr;
static bool                   g_resourcesReady = false;
static UINT                   g_bbW = 0;
static UINT                   g_bbH = 0;

struct ScreenVertex { float x, y, z, rhw, u, v; };

static void ReleaseResources() {
    if (g_pPaniniOutputSurface) { g_pPaniniOutputSurface->Release(); g_pPaniniOutputSurface = nullptr; }
    if (g_pPaniniOutputTexture) { g_pPaniniOutputTexture->Release(); g_pPaniniOutputTexture = nullptr; }
    if (g_pSceneSurface) { g_pSceneSurface->Release(); g_pSceneSurface = nullptr; }
    if (g_pSceneTexture) { g_pSceneTexture->Release(); g_pSceneTexture = nullptr; }
    if (g_pPaniniShader) { g_pPaniniShader->Release(); g_pPaniniShader = nullptr; }
    if (g_pTintShader)   { g_pTintShader->Release();   g_pTintShader   = nullptr; }
    if (g_pUvVisShader)  { g_pUvVisShader->Release();  g_pUvVisShader  = nullptr; }
    if (g_pCasShader)    { g_pCasShader->Release();    g_pCasShader    = nullptr; }
    if (g_pFxaaShader)   { g_pFxaaShader->Release();   g_pFxaaShader   = nullptr; }
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
        LOG_INFO("hook", "CreateTexture failed hr=0x%08X", (unsigned)hr);
        return false;
    }
    g_pSceneTexture->GetSurfaceLevel(0, &g_pSceneSurface);

    hr = dev->CreateTexture(
        desc.Width, desc.Height, 1,
        D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8,
        D3DPOOL_DEFAULT, &g_pPaniniOutputTexture, NULL);
    if (FAILED(hr)) {
        LOG_INFO("hook", "CreateTexture(paniniOutput) failed hr=0x%08X", (unsigned)hr);
        ReleaseResources();
        return false;
    }
    g_pPaniniOutputTexture->GetSurfaceLevel(0, &g_pPaniniOutputSurface);

    hr = dev->CreatePixelShader(
        reinterpret_cast<const DWORD*>(g_paniniShaderBytecode), &g_pPaniniShader);
    if (FAILED(hr)) {
        LOG_INFO("hook", "CreatePixelShader(panini) failed hr=0x%08X", (unsigned)hr);
        ReleaseResources();
        return false;
    }

    hr = dev->CreatePixelShader(
        reinterpret_cast<const DWORD*>(g_tintShaderBytecode), &g_pTintShader);
    if (FAILED(hr)) {
        LOG_INFO("hook", "CreatePixelShader(tint) failed hr=0x%08X", (unsigned)hr);
        ReleaseResources();
        return false;
    }

    hr = dev->CreatePixelShader(
        reinterpret_cast<const DWORD*>(g_uvVisShaderBytecode), &g_pUvVisShader);
    if (FAILED(hr)) {
        LOG_INFO("hook", "CreatePixelShader(uvVis) failed hr=0x%08X", (unsigned)hr);
        ReleaseResources();
        return false;
    }

    hr = dev->CreatePixelShader(
        reinterpret_cast<const DWORD*>(g_casShaderBytecode), &g_pCasShader);
    if (FAILED(hr)) {
        LOG_INFO("hook", "CreatePixelShader(cas) failed hr=0x%08X", (unsigned)hr);
        ReleaseResources();
        return false;
    }

    hr = dev->CreatePixelShader(
        reinterpret_cast<const DWORD*>(g_fxaaShaderBytecode), &g_pFxaaShader);
    if (FAILED(hr)) {
        LOG_INFO("hook", "CreatePixelShader(fxaa) failed hr=0x%08X", (unsigned)hr);
        ReleaseResources();
        return false;
    }

    LOG_INFO("hook", "resources ready: %ux%u fmt=%u", desc.Width, desc.Height, desc.Format);
    g_resourcesReady = true;
    return true;
}

static void ApplyPostProcess(IDirect3DDevice9* dev) {
    PostProcessConfig cfg;
    PostProcessConfig_ReadFromCVars(&cfg);

    if (!cfg.paniniEnabled) cfg.strength = 0.0f;
    if (cfg.strength != cfg.strength || cfg.strength < 0.0f) cfg.strength = 0.01f;
    if (cfg.strength > 1.0f) cfg.strength = 1.0f;
    if (cfg.verticalComp != cfg.verticalComp) cfg.verticalComp = 0.0f;
    if (cfg.verticalComp < -1.0f) cfg.verticalComp = -1.0f;
    if (cfg.verticalComp > 1.0f) cfg.verticalComp = 1.0f;
    if (cfg.fill != cfg.fill || cfg.fill < 0.0f) cfg.fill = 0.8f;
    if (cfg.fill > 1.0f) cfg.fill = 1.0f;

    if (!cfg.paniniEnabled && !cfg.fxaaEnabled && cfg.sharpen <= 0.0f && !cfg.debugTint && !cfg.debugUV)
        return;

    IDirect3DSurface9* pBB = nullptr;
    dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBB);
    if (!pBB) return;

    D3DSURFACE_DESC bbDesc;
    pBB->GetDesc(&bbDesc);
    if (bbDesc.Width != g_bbW || bbDesc.Height != g_bbH) {
        pBB->Release();
        ReleaseResources();
        if (!CreateResources(dev))
            return;
        dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBB);
        if (!pBB) return;
    }

    float fov     = ReadCameraFov();
    float aspect  = (g_bbH > 0) ? static_cast<float>(g_bbW) / static_cast<float>(g_bbH) : 1.7778f;
    float halfTan = tanf(fov * 0.5f);
    float zoom    = ComputeFillZoom(cfg.strength, halfTan, aspect, cfg.fill);
    if (zoom != zoom || zoom < 0.01f || zoom > 10.0f)
        zoom = 1.273f;

#ifdef PANINI_DEBUG_LOG
    {
        static int s_diag = 0;
        if (s_diag++ < 1) {
            float swFov = CVar_GetFloat(CVar::WowFov, 0.0f);
            LOG_DEBUG("diag", "swFov=%08X fov=%08X htan=%08X zoom=%08X D=%08X fill=%08X asp=%08X",
                FloatBits(swFov), FloatBits(fov), FloatBits(halfTan),
                FloatBits(zoom), FloatBits(cfg.strength), FloatBits(cfg.fill),
                FloatBits(aspect));
        }
    }
#endif

    float c0[4] = { cfg.strength, halfTan, zoom, 1.0f };
    float c1[4] = { cfg.verticalComp, aspect, 0.0f, 0.0f };
    float invW = (g_bbW > 0) ? 1.0f / static_cast<float>(g_bbW) : 0.0f;
    float invH = (g_bbH > 0) ? 1.0f / static_cast<float>(g_bbH) : 0.0f;

    D3D9State saved;
    SaveD3D9State(dev, &saved);

    dev->StretchRect(pBB, NULL, g_pSceneSurface, NULL, D3DTEXF_NONE);

    dev->SetPixelShader(g_pPaniniShader);
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

    dev->SetTexture(0, g_pSceneTexture);
    dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);

    dev->SetRenderTarget(0, g_pPaniniOutputSurface);

    float w = static_cast<float>(g_bbW);
    float h = static_cast<float>(g_bbH);
    ScreenVertex quad[4] = {
        { -0.5f,     -0.5f,     0.0f, 1.0f, 0.0f, 0.0f },
        {  w - 0.5f, -0.5f,     0.0f, 1.0f, 1.0f, 0.0f },
        { -0.5f,      h - 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
        {  w - 0.5f,  h - 0.5f, 0.0f, 1.0f, 1.0f, 1.0f },
    };
    dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(ScreenVertex));

    float fxaaC0[4] = { invW, invH, 0.0f, 0.0f };

    if (cfg.debugTint) {
        dev->StretchRect(g_pPaniniOutputSurface, NULL, g_pSceneSurface, NULL, D3DTEXF_POINT);
        dev->SetRenderTarget(0, pBB);
        dev->SetTexture(0, g_pSceneTexture);
        dev->SetPixelShader(g_pTintShader);
        dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(ScreenVertex));
    } else if (cfg.debugUV) {
        dev->StretchRect(g_pPaniniOutputSurface, NULL, g_pSceneSurface, NULL, D3DTEXF_POINT);
        dev->SetRenderTarget(0, pBB);
        dev->SetTexture(0, g_pSceneTexture);
        dev->SetPixelShader(g_pUvVisShader);
        dev->SetPixelShaderConstantF(0, c0, 1);
        dev->SetPixelShaderConstantF(1, c1, 1);
        dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(ScreenVertex));
    } else if (cfg.fxaaEnabled && cfg.sharpen > 0.0f) {
        float sh = cfg.sharpen;
        if (sh > 1.0f) sh = 1.0f;

        dev->StretchRect(g_pPaniniOutputSurface, NULL, g_pSceneSurface, NULL, D3DTEXF_POINT);
        dev->SetRenderTarget(0, g_pPaniniOutputSurface);
        dev->SetTexture(0, g_pSceneTexture);
        dev->SetPixelShader(g_pFxaaShader);
        dev->SetPixelShaderConstantF(0, fxaaC0, 1);
        dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(ScreenVertex));

        dev->StretchRect(g_pPaniniOutputSurface, NULL, g_pSceneSurface, NULL, D3DTEXF_POINT);

        float casC1[4] = { invW, invH, 0.0f, 0.0f };
        float casC2[4] = { sh, 0.0f, 0.0f, 0.0f };

        dev->SetRenderTarget(0, pBB);
        dev->SetTexture(0, g_pSceneTexture);
        dev->SetPixelShader(g_pCasShader);
        dev->SetPixelShaderConstantF(1, casC1, 1);
        dev->SetPixelShaderConstantF(2, casC2, 1);
        dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(ScreenVertex));
    } else if (cfg.fxaaEnabled) {
        dev->StretchRect(g_pPaniniOutputSurface, NULL, g_pSceneSurface, NULL, D3DTEXF_POINT);
        dev->SetRenderTarget(0, pBB);
        dev->SetTexture(0, g_pSceneTexture);
        dev->SetPixelShader(g_pFxaaShader);
        dev->SetPixelShaderConstantF(0, fxaaC0, 1);
        dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(ScreenVertex));
    } else if (cfg.sharpen > 0.0f) {
        float sh = cfg.sharpen;
        if (sh > 1.0f) sh = 1.0f;
        dev->StretchRect(g_pPaniniOutputSurface, NULL, g_pSceneSurface, NULL, D3DTEXF_POINT);

        float casC1[4] = { invW, invH, 0.0f, 0.0f };
        float casC2[4] = { sh, 0.0f, 0.0f, 0.0f };

        dev->SetRenderTarget(0, pBB);
        dev->SetTexture(0, g_pSceneTexture);
        dev->SetPixelShader(g_pCasShader);
        dev->SetPixelShaderConstantF(1, casC1, 1);
        dev->SetPixelShaderConstantF(2, casC2, 1);
        dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(ScreenVertex));
    } else {
        dev->StretchRect(g_pPaniniOutputSurface, NULL, pBB, NULL, D3DTEXF_LINEAR);
    }

    RestoreD3D9State(dev, &saved);
    pBB->Release();
}

static uintptr_t g_originalRenderWorldTarget = 0;

// Classic 0x482D70: __thiscall, ECX = WorldFrame* on entry, no stack args.
// WotLK 0x4FAF90: __cdecl, WorldFrame* is first stack arg.
// Declared with a parameter so the compiler reads the stack arg correctly
// regardless of frame pointer omission (-fomit-frame-pointer in -O2).
// On Classic, the parameter slot holds whatever was on the stack (ignored).
void __cdecl Hooked_RenderWorld(void* stackArg) {
    void* thisPtr;
    if (g_offsets->version == WowVersion::WotLK335) {
        thisPtr = stackArg;
    } else {
        __asm__ __volatile__("" : "=c"(thisPtr));
    }

    UpdateCameraFov();

    g_ops.callOriginalRenderWorld(g_originalRenderWorldTarget, thisPtr);

    if (!g_resourcesReady || !g_ops.isWorldActive())
        return;

    IDirect3DDevice9* dev = GetWoWDevice();
    if (!dev) return;

    ApplyPostProcess(dev);
}

HRESULT __stdcall Hooked_EndScene(IDirect3DDevice9* dev) {
    if (!g_resourcesReady) {
        if (g_ops.isWorldActive() && CreateResources(dev)) {
            LOG_INFO("hook", "EndScene: resources created after world active");
        }
    }

    IDirect3DSurface9* pBB = nullptr;
    dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBB);
    if (pBB) {
        D3DSURFACE_DESC bbDesc;
        pBB->GetDesc(&bbDesc);
        pBB->Release();
        if (bbDesc.Width != g_bbW || bbDesc.Height != g_bbH) {
            ReleaseResources();
            if (g_ops.isWorldActive()) CreateResources(dev);
        }
    }

    return g_pOriginalEndScene(dev);
}

HRESULT __stdcall Hooked_Reset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pParams) {
    LOG_INFO("hook", "Reset called, releasing resources");
    ReleaseResources();
    return g_pOriginalReset(dev, pParams);
}

// Trampoline: saves the original prologue bytes and jumps back.
// Classic (0x00482D70): push ebp(1) + mov ebp,esp(2) + sub esp,0x80(6) = 9 bytes
// WotLK (0x004FAF90): push ebp(1) + mov ebp,esp(2) + sub esp,0x80(6) = 9 bytes
static uint8_t* g_trampoline = nullptr;

void InstallRenderWorldHook() {
    auto target = reinterpret_cast<uint8_t*>(g_offsets->RenderWorld_Addr);

    int patchSize = g_ops.renderWorldPatchSize;

    // If another mod already hooked (0xE9 = JMP), chain on top instead
    if (target[0] == 0xE9) {
        int32_t existingDisp;
        memcpy(&existingDisp, target + 1, 4);
        g_originalRenderWorldTarget = reinterpret_cast<uintptr_t>(target + 5) + existingDisp;
        LOG_INFO("hook", "RenderWorld: chaining on existing JMP to %p",
                reinterpret_cast<void*>(g_originalRenderWorldTarget));

        DWORD oldProt;
        VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &oldProt);
        int32_t newDisp = static_cast<int32_t>(
            reinterpret_cast<intptr_t>(&Hooked_RenderWorld) - reinterpret_cast<intptr_t>(target + 5));
        memcpy(target + 1, &newDisp, 4);
        VirtualProtect(target, 5, oldProt, &oldProt);

        LOG_INFO("hook", "RenderWorld: JMP redirected to %p", &Hooked_RenderWorld);
        return;
    }

    int trampOffset = TrampolineAlloc(32);
    if (trampOffset < 0) {
        LOG_INFO("hook", "RenderWorld: trampoline pool exhausted");
        return;
    }
    g_trampoline = &g_trampolinePool[trampOffset];

    memcpy(g_trampoline, target, patchSize);

    g_trampoline[patchSize] = 0xE9;
    int32_t trampolineDisp = static_cast<int32_t>(
        reinterpret_cast<intptr_t>(target + patchSize) -
        reinterpret_cast<intptr_t>(g_trampoline + patchSize + 5));
    memcpy(g_trampoline + patchSize + 1, &trampolineDisp, 4);

    g_originalRenderWorldTarget = reinterpret_cast<uintptr_t>(g_trampoline);

    DWORD oldProt;
    VirtualProtect(target, patchSize, PAGE_EXECUTE_READWRITE, &oldProt);

    target[0] = 0xE9;
    int32_t hookDisp = static_cast<int32_t>(
        reinterpret_cast<intptr_t>(&Hooked_RenderWorld) - reinterpret_cast<intptr_t>(target + 5));
    memcpy(target + 1, &hookDisp, 4);
    memset(target + 5, 0x90, patchSize - 5);

    VirtualProtect(target, patchSize, oldProt, &oldProt);

    LOG_INFO("hook", "RenderWorld: trampoline at %p patchSize=%d hook at %p",
            g_trampoline, patchSize, &Hooked_RenderWorld);
}
