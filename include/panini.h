#pragma once

#include <windows.h>
#include <d3d9.h>
#include <cstdint>
#include "log.h"
#include "wow_offsets.h"
#include "cvar_core.h"

// Function pointer table resolved once at init. All version-specific implementation
// is isolated here; hot-path code calls through these pointers with zero branches.
struct VersionOps {
    void  (*registerCVar)(const char* name, const char* defaultVal);
    float (*getCVarFloat)(const char* name, float fallback);
    int   (*getCVarInt)(const char* name, int fallback);
    void* (*getCameraPtr)();
    bool  (*isWorldActive)();
    void  (*callOriginalRenderWorld)(uintptr_t target, void* ecx);
    int   renderWorldPatchSize;
};

extern VersionOps g_ops;

// Populates g_ops from the active offset table. Must be called after DetectWowVersion
// and before CVar_RegisterAll or any CVar read.
void InitVersionOps();

// Detects WoW client version (Classic 1.12.1 or WotLK 3.3.5a) via PE header timestamp.
// Sets g_offsets to the matching offset table. Returns true on success.
bool DetectWowVersion();

// Registers all PaniniWoW CVars with the version-appropriate engine registration
// path resolved by InitVersionOps.
void  CVar_RegisterAll();

// Reads a CVar as float using the version-appropriate lookup path.
// Returns fallback if the CVar does not exist or the value is NaN.
float CVar_GetFloat(CVar id, float fallback);

// Reads a CVar as int using the version-appropriate lookup path.
// Returns fallback if the CVar does not exist.
int   CVar_GetInt(CVar id, int fallback);

// Post-process configuration snapshot read from CVars each frame.
// Fields are clamped to valid ranges in ApplyPostProcess, not here.
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

// Reads all post-process CVars and populates the config struct.
void PostProcessConfig_ReadFromCVars(PostProcessConfig* cfg);

// Opens the log file at mods\PaniniWoW.log.
void  LogInit();

// Flushes and closes the log file.
void  LogShutdown();

// IDirect3DDevice9::EndScene function pointer type.
typedef HRESULT(__stdcall* EndScene_t)(IDirect3DDevice9*);

// IDirect3DDevice9::Reset function pointer type.
typedef HRESULT(__stdcall* Reset_t)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);

// Saved original vtable pointers before hook installation.
extern EndScene_t    g_pOriginalEndScene;
extern Reset_t       g_pOriginalReset;

// VTable hook for EndScene. Creates resources on first frame, handles resize, chains original.
HRESULT __stdcall Hooked_EndScene(IDirect3DDevice9* pDevice);

// VTable hook for Reset. Releases all GPU resources, chains original.
HRESULT __stdcall Hooked_Reset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pParams);

// Trampoline hook for RenderWorld. Updates camera FoV, calls original, then runs post-process.
// Classic: __thiscall, WorldFrame* in ECX. WotLK: __cdecl, WorldFrame* as stack arg.
// Parameter declared so the compiler reads the WotLK stack arg correctly even with
// -fomit-frame-pointer. On Classic, the parameter value is ignored (ECX used instead).
void __cdecl Hooked_RenderWorld(void* stackArg);

// Installs a JMP trampoline at RenderWorld. Chains on existing JMP hooks if another mod hooked first.
void InstallRenderWorldHook();

#include "panini_math.h"

// Returns the current camera FoV in radians. Prefers paniniFov CVar when enabled, falls back
// to the FoV CVar, then to the in-memory camera value, then to pi/2.
float ReadCameraFov();

// Returns the IDirect3DDevice9* from WoW's CGxDeviceD3d singleton via offset chain.
// Returns nullptr if CGxDeviceD3d has not been initialized.
IDirect3DDevice9* GetWoWDevice();

// Returns true when the game world is loaded and rendering (WorldFrame pointer nonzero, or
// WotLK camera pointer nonzero).
bool  IsWorldActive();

// Writes paniniFov to the camera struct when panini is enabled. On disable transition,
// restores the FoV CVar value to avoid stale overridden FoV.
void  UpdateCameraFov();

// Captures the full D3D9 device state needed for post-process rendering.
// Acquires COM references via Get* calls; caller must ensure Release via RestoreD3D9State.
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

// Saves the full D3D9 device state needed for post-process rendering.
// Acquires COM references to pixel/vertex shaders, render target, and texture.
void SaveD3D9State(IDirect3DDevice9* dev, D3D9State* state);

// Restores the D3D9 device state and releases COM references acquired by SaveD3D9State.
void RestoreD3D9State(IDirect3DDevice9* dev, const D3D9State* state);
