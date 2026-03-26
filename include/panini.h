#pragma once

// PaniniWoW — Panini projection post-process for WoW 1.12.1
// This header provides shared types and declarations across DLL modules.

#include <windows.h>
#include <d3d9.h>

//------------------------------------------------------------------------------
// Configuration
//------------------------------------------------------------------------------

struct PaniniConfig {
    bool  enabled;       // default: false
    float strength;      // D parameter, [0.0, 1.0], default: 0.5
    float verticalComp;  // S parameter, [-1.0, 1.0], default: 0.0
    float fill;          // fill zoom blend, [0.0, 1.0], default: 0.8
    bool  debug;         // debug overlay, default: false
};

void LoadConfig(const char* path, PaniniConfig* cfg);
void ConfigSetDefaults(PaniniConfig* cfg);

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
//------------------------------------------------------------------------------

void LogInit();
void LogShutdown();
void LogInfo(const char* fmt, ...);
void LogWarn(const char* fmt, ...);
void LogError(const char* fmt, ...);
