#pragma once
#include <cstdint>

// Known WoW client versions supported by the DLL.
enum class WowVersion { Classic112, WotLK335 };

// Memory offsets and function addresses for a specific WoW client version.
// All addresses are absolute VA in the main executable (no relocation adjustment needed
// because the exe is loaded at its preferred base by the WoW launcher).
struct WowOffsets {
    WowVersion version;

    uintptr_t CVarLookup;
    uintptr_t CVarRegister;
    uintptr_t CVar_FloatValue;
    uintptr_t CVar_IntValue;

    uintptr_t WorldFrame_Ptr;
    uintptr_t ActiveCamera_Off;
    uintptr_t Camera_FOV_Off;

    uintptr_t CGxDeviceD3d_Ptr;
    uintptr_t D3DDevice_Off;

    uintptr_t RenderWorld_Addr;

    uintptr_t GetActiveCamera_Addr;
    uintptr_t CVarGetString_Addr;
    uintptr_t CVarSet_Addr;
};

// WoW Classic 1.12.1 (build 5875)
static constexpr WowOffsets kClassic = {
    .version            = WowVersion::Classic112,
    .CVarLookup         = 0x0063DEC0,
    .CVarRegister       = 0x0063DB90,
    .CVar_FloatValue    = 0x24,
    .CVar_IntValue      = 0x28,
    .WorldFrame_Ptr     = 0x00B4B2BC,
    .ActiveCamera_Off   = 0x65B8,
    .Camera_FOV_Off     = 0x40,
    .CGxDeviceD3d_Ptr   = 0x00C0ED38,
    .D3DDevice_Off      = 0x38A8,
    .RenderWorld_Addr   = 0x00482D70,
    .GetActiveCamera_Addr = 0,
    .CVarGetString_Addr = 0,
    .CVarSet_Addr = 0,
};

// WoW WotLK 3.3.5a (build 12340)
// WotLK uses GetActiveCamera() function instead of static WorldFrame pointer chain.
// RenderWorld at 0x780F50 is not __thiscall; it loads ECX internally (CGxDeviceD3d).
// CVarGetString at 0x00767460 returns CVar* (not const char*); dereference CVar+0x28 for string value.
static constexpr WowOffsets kWotLK = {
    .version            = WowVersion::WotLK335,
    .CVarLookup         = 0x00767440,
    .CVarRegister       = 0x00767FC0,
    .CVar_FloatValue    = 0,
    .CVar_IntValue      = 0,
    .WorldFrame_Ptr     = 0x00B7436C,
    .ActiveCamera_Off   = 0,
    .Camera_FOV_Off     = 0x40,
    .CGxDeviceD3d_Ptr   = 0x00C5DF88,
    .D3DDevice_Off      = 0x397C,
    // CGWorldFrame::RenderWorld at 0x4FAF90: per-frame __cdecl callback
    // scheduled by OnFrameRender (vtable[32]), takes WorldFrame* as stack arg.
    // 0x780F50 is an unrelated one-shot init function; do not hook it.
    .RenderWorld_Addr   = 0x004FAF90,
    .GetActiveCamera_Addr = 0x004F5960,
    .CVarGetString_Addr = 0x00767460,
    .CVarSet_Addr = 0x007668C0,
};

// Points to the active offset table (kClassic or kWotLK), set by DetectWowVersion.
extern const WowOffsets* g_offsets;
