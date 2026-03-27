#include "panini.h"
#include "panini_shader.h"
#include "debug_shader.h"
#include <cmath>

EndScene_t    g_pOriginalEndScene    = nullptr;
Reset_t       g_pOriginalReset       = nullptr;

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
        LOG_INFO("hook", "CreateTexture failed hr=0x%08X", (unsigned)hr);
        return false;
    }
    g_pSceneTexture->GetSurfaceLevel(0, &g_pSceneSurface);

    hr = dev->CreatePixelShader(
        reinterpret_cast<const DWORD*>(g_paniniShaderBytecode), &g_pPaniniShader);
    if (FAILED(hr)) {
        LOG_INFO("hook", "CreatePixelShader(panini) failed hr=0x%08X", (unsigned)hr);
        ReleaseResources();
        return false;
    }

    hr = dev->CreatePixelShader(
        reinterpret_cast<const DWORD*>(g_debugShaderBytecode), &g_pDebugShader);
    if (FAILED(hr)) {
        LOG_INFO("hook", "CreatePixelShader(debug) failed hr=0x%08X", (unsigned)hr);
        ReleaseResources();
        return false;
    }

    LOG_INFO("hook", "resources ready: %ux%u fmt=%u", desc.Width, desc.Height, desc.Format);
    g_resourcesReady = true;
    return true;
}

static void ApplyPaniniPass(IDirect3DDevice9* dev) {
    PaniniConfig cfg;
    PaniniConfig_ReadFromCVars(&cfg);

    if (cfg.strength != cfg.strength || cfg.strength < 0.0f) cfg.strength = 0.5f;
    if (cfg.strength > 1.0f) cfg.strength = 1.0f;
    if (cfg.verticalComp != cfg.verticalComp) cfg.verticalComp = 0.0f;
    if (cfg.verticalComp < -1.0f) cfg.verticalComp = -1.0f;
    if (cfg.verticalComp > 1.0f) cfg.verticalComp = 1.0f;
    if (cfg.fill != cfg.fill || cfg.fill < 0.0f) cfg.fill = 0.8f;
    if (cfg.fill > 1.0f) cfg.fill = 1.0f;

    if (!cfg.enabled && !cfg.debugTint && !cfg.debugUV)
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

    IDirect3DPixelShader9* activeShader = (cfg.debugUV || cfg.debugTint) ? g_pDebugShader : g_pPaniniShader;

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
            float swFov = CVar_GetFloat("FoV", 0.0f);
            LOG_DEBUG("diag", "swFov=%08X fov=%08X htan=%08X zoom=%08X D=%08X fill=%08X asp=%08X",
                FloatBits(swFov), FloatBits(fov), FloatBits(halfTan),
                FloatBits(zoom), FloatBits(cfg.strength), FloatBits(cfg.fill),
                FloatBits(aspect));
        }
    }
#endif

    float c0[4] = { cfg.strength, halfTan, zoom, 1.0f };
    float c1[4] = { cfg.verticalComp, aspect, 0.0f, 0.0f };

    D3D9State saved;
    SaveD3D9State(dev, &saved);

    dev->StretchRect(pBB, NULL, g_pSceneSurface, NULL, D3DTEXF_NONE);

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
}

static uintptr_t g_originalRenderWorldTarget = 0;

// Called via JMP from 0x00482D70. ECX = WorldFrame* (__thiscall).
// The function prologue (push ebp; mov ebp,esp; ...) does NOT clobber ECX,
// so reading ECX in the first statement captures the caller's value.
void Hooked_RenderWorld() {
    void* thisPtr;
    __asm__ __volatile__("" : "=c"(thisPtr));

    UpdateCameraFov();

    __asm__ __volatile__ (
        "mov %0, %%ecx\n\t"
        "call *%1\n\t"
        :
        : "r"(thisPtr), "r"(g_originalRenderWorldTarget)
        : "eax", "edx", "ecx", "memory"
    );

    if (!g_resourcesReady || !IsWorldActive())
        return;

    IDirect3DDevice9* dev = GetWoWDevice();
    if (!dev) return;

    ApplyPaniniPass(dev);
}

static IDirect3DDevice9* g_pDevice = nullptr;

HRESULT __stdcall Hooked_EndScene(IDirect3DDevice9* dev) {
    if (!g_resourcesReady) {
        if (IsWorldActive() && CreateResources(dev)) {
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
            if (IsWorldActive()) CreateResources(dev);
        }
    }

    return g_pOriginalEndScene(dev);
}

HRESULT __stdcall Hooked_Reset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pParams) {
    LOG_INFO("hook", "Reset called, releasing resources");
    ReleaseResources();
    return g_pOriginalReset(dev, pParams);
}

void InstallRenderWorldHook() {
    auto target = reinterpret_cast<uint8_t*>(wow::RenderWorld_Addr);

    if (target[0] != 0xE9) {
        LOG_INFO("hook", "RenderWorld: expected existing JMP (0xE9), got 0x%02X", target[0]);
        return;
    }

    // Read the existing JMP displacement (another mod already hooked this function).
    int32_t existingDisp;
    memcpy(&existingDisp, target + 1, 4);
    g_originalRenderWorldTarget = reinterpret_cast<uintptr_t>(target + 5) + existingDisp;

    LOG_INFO("hook", "RenderWorld: existing JMP to %p, chaining on top",
            reinterpret_cast<void*>(g_originalRenderWorldTarget));

    // Replace the JMP displacement to point to our function.
    // No instruction boundary issues — we're just rewriting 4 bytes of an existing JMP.
    DWORD oldProt;
    VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &oldProt);

    int32_t newDisp = static_cast<int32_t>(
        reinterpret_cast<intptr_t>(&Hooked_RenderWorld) - reinterpret_cast<intptr_t>(target + 5));
    memcpy(target + 1, &newDisp, 4);

    VirtualProtect(target, 5, oldProt, &oldProt);

    LOG_INFO("hook", "RenderWorld: JMP redirected to %p", &Hooked_RenderWorld);
}
