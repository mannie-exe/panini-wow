#pragma once

#include <windows.h>
#include <d3d9.h>
#include <cstdint>
#include "log.h"

namespace wow {

constexpr uintptr_t CVarLookup_Addr    = 0x0063DEC0;
constexpr uintptr_t CVarRegister_Addr  = 0x0063DB90;

constexpr uintptr_t CVar_StringValue   = 0x20;
constexpr uintptr_t CVar_FloatValue    = 0x24;
constexpr uintptr_t CVar_IntValue      = 0x28;

constexpr uintptr_t WorldFrame_Ptr     = 0x00B4B2BC;
constexpr uintptr_t ActiveCamera_Off   = 0x65B8;
constexpr uintptr_t Camera_FOV_Off     = 0x40;
constexpr uintptr_t Camera_Aspect_Off  = 0x44;

constexpr uintptr_t CGxDeviceD3d_Ptr   = 0x00C0ED38;
constexpr uintptr_t D3DDevice_Off      = 0x38A8;

constexpr uintptr_t RenderWorld_Addr   = 0x00482D70;

} // namespace wow

void  CVar_RegisterAll();
float CVar_GetFloat(const char* name, float fallback);
int   CVar_GetInt(const char* name, int fallback);

struct PostProcessConfig {
    bool  paniniEnabled;
    float strength;
    float verticalComp;
    float fill;
    bool  fxaaEnabled;
    float sharpen;
    bool  debugTint;
    bool  debugUV;
};

void PostProcessConfig_ReadFromCVars(PostProcessConfig* cfg);

void  LogInit();
void  LogShutdown();

typedef HRESULT(__stdcall* EndScene_t)(IDirect3DDevice9*);
typedef HRESULT(__stdcall* Reset_t)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
extern EndScene_t    g_pOriginalEndScene;
extern Reset_t       g_pOriginalReset;

HRESULT __stdcall Hooked_EndScene(IDirect3DDevice9* pDevice);
HRESULT __stdcall Hooked_Reset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pParams);
void   Hooked_RenderWorld();

void InstallRenderWorldHook();

#include "panini_math.h"

float ReadCameraFov();
IDirect3DDevice9* GetWoWDevice();
bool  IsWorldActive();
void  UpdateCameraFov();

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
