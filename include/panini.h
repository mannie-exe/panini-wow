#pragma once

// PaniniWoW; panini projection post-process for WoW 1.12.1.
// Shared types and declarations across DLL modules.

#include <windows.h>
#include <d3d9.h>
#include <cstdint>

//------------------------------------------------------------------------------
// WoW 1.12.1 (build 5875) memory offsets
//------------------------------------------------------------------------------

namespace wow {

// CVar system
constexpr uintptr_t CVarLookup_Addr    = 0x0063DEC0;
constexpr uintptr_t CVarRegister_Addr  = 0x0063DB90;

// CVar struct field offsets (verified via runtime hex dump)
constexpr uintptr_t CVar_StringValue   = 0x20; // char* to value string
constexpr uintptr_t CVar_FloatValue    = 0x24; // cached float
constexpr uintptr_t CVar_IntValue      = 0x28; // cached int

// Camera pointer chain: [WorldFrame] -> +ActiveCameraOff -> CGCamera
constexpr uintptr_t WorldFrame_Ptr     = 0x00B4B2BC;
constexpr uintptr_t ActiveCamera_Off   = 0x65B8;
constexpr uintptr_t Camera_FOV_Off     = 0x40;
constexpr uintptr_t Camera_Aspect_Off  = 0x44;

// D3D9 device pointer chain: [CGxDeviceD3d] -> +D3DDevice_Off -> IDirect3DDevice9*
constexpr uintptr_t CGxDeviceD3d_Ptr   = 0x00C0ED38;
constexpr uintptr_t D3DDevice_Off      = 0x38A8;

} // namespace wow

//------------------------------------------------------------------------------
// CVar access (src/cvar.cpp)
//------------------------------------------------------------------------------

void  CVar_RegisterAll();
float CVar_GetFloat(const char* name, float fallback);
int   CVar_GetInt(const char* name, int fallback);

//------------------------------------------------------------------------------
// Configuration (read from CVars each frame)
//------------------------------------------------------------------------------

struct PaniniConfig {
    bool  enabled;
    float strength;      // D parameter, [0.0, 1.0]
    float verticalComp;  // S parameter, [-1.0, 1.0]
    float fill;          // fill zoom blend, [0.0, 1.0]
    bool  debug;
};

void PaniniConfig_ReadFromCVars(PaniniConfig* cfg);

//------------------------------------------------------------------------------
// Hooks
//------------------------------------------------------------------------------

typedef HRESULT(__stdcall* EndScene_t)(IDirect3DDevice9*);
typedef HRESULT(__stdcall* Reset_t)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);

extern EndScene_t g_pOriginalEndScene;
extern Reset_t    g_pOriginalReset;

HRESULT __stdcall Hooked_EndScene(IDirect3DDevice9* pDevice);
HRESULT __stdcall Hooked_Reset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pParams);

//------------------------------------------------------------------------------
// Panini math
//------------------------------------------------------------------------------

float ComputeFillZoom(float strength, float halfTanFov, float aspect, float fill);
float ReadCameraFov();
float ReadCameraAspect();
void  WriteCameraFov(float fov);
IDirect3DDevice9* GetWoWDevice();

//------------------------------------------------------------------------------
// D3D9 state management
//------------------------------------------------------------------------------

struct D3D9State {
    IDirect3DPixelShader9*  pPixelShader;
    IDirect3DVertexShader9* pVertexShader;
    IDirect3DSurface9*      pRenderTarget0;
    IDirect3DBaseTexture9*  pTexture0;
    DWORD                   fvf;
    DWORD                   zenable;
    DWORD                   alphablend;
    DWORD                   cullmode;
    DWORD                   stencilenable;
    DWORD                   scissortest;
    DWORD                   colorwrite;
    DWORD                   tss_colorop;
    DWORD                   tss_alphaop;
    DWORD                   samp_minfilter;
    DWORD                   samp_magfilter;
    DWORD                   samp_addressu;
    DWORD                   samp_addressv;
    D3DVIEWPORT9            viewport;
};

void SaveD3D9State(IDirect3DDevice9* dev, D3D9State* state);
void RestoreD3D9State(IDirect3DDevice9* dev, const D3D9State* state);

//------------------------------------------------------------------------------
// Logging
//
// Two levels: INFO (always) and TRACE (debug builds only).
// Wide/fat format: each line packs all relevant context.
// Output: mods/PaniniWoW.log, flushed after every write.
// No external libs. No runtime configuration.
// TRACE gated by NDEBUG (absent in Debug builds, present in Release).
//------------------------------------------------------------------------------

void LogInit();
void LogShutdown();

void LogInfo(const char* mod, const char* fmt, ...);
void LogTrace(const char* mod, const char* fmt, ...);
