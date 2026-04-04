#pragma once
#include <cstddef>

enum class CVar : int {
    Enabled,    // "paniniEnabled"
    Strength,   // "paniniStrength"
    VertComp,   // "paniniVertComp"
    Fill,       // "paniniFill"
    Fov,        // "paniniFov"
    Fxaa,       // "ppFxaa"
    Sharpen,    // "ppSharpen"
    DebugTint,  // "ppDebugTint"
    DebugUV,    // "ppDebugUV"
    Version,    // "ppVersion"
    WowFov,     // "FoV" (WoW built-in, read-only for us)
    _Count
};

struct CVarDesc {
    const char* name;
    const char* defaultVal;
};

constexpr CVarDesc kCVarTable[(size_t)CVar::_Count] = {
    { "paniniEnabled", "1" },
    { "paniniStrength", "0.01" },
    { "paniniVertComp", "0.0" },
    { "paniniFill", "1.0" },
    { "paniniFov", "2.82" },
    { "ppFxaa", "1" },
    { "ppSharpen", "0.2" },
    { "ppDebugTint", "0" },
    { "ppDebugUV", "0" },
    { "ppVersion", "" },         // filled at runtime with PANINI_VERSION
    { "FoV", "" },               // WoW built-in, no default from us
};
static_assert(sizeof(kCVarTable) / sizeof(CVarDesc) == (size_t)CVar::_Count,
    "kCVarTable size must match CVar::_Count");
