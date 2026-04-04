#pragma once

#include <cmath>
#include <cstddef>

// Default paniniFov CVar value in radians, matching kCVarTable[CVar::Fov].defaultVal parsed as float.
constexpr float kCVarFovDefault = 2.82f;

// pi/2 in radians, used as a fallback FoV when no valid value is available.
constexpr float kPiOver2 = 1.5708f;

// Computes the zoom factor that scales the projected image to fill the viewport.
// When strength=0 or fill=0, returns 1.0 (no zoom). The fill parameter lerps
// between the exact-fit zoom (fill=0) and full-fill zoom (fill=1).
inline float ComputeFillZoom(float strength, float halfTanFov, float aspect, float fill) {
    if (strength < 0.001f || fill < 0.001f)
        return 1.0f;

    float lonMax = atanf(halfTanFov * aspect);
    float denom  = strength + cosf(lonMax);
    if (denom < 0.001f)
        return 1.0f;

    float k     = (strength + 1.0f) / denom;
    float edgeX = k * sinf(lonMax);
    float fitX  = fabsf(edgeX) / (halfTanFov * aspect);
    if (fitX < 0.001f)
        return 1.0f;

    float zoom   = 1.0f / fitX;
    return 1.0f + (zoom - 1.0f) * fill;
}

// Returns true if f is a plausible FoV in radians: positive, not NaN, within WoW's camera range.
inline bool IsValidFov(float f) {
    return f > 0.05f && f < 3.094f && f == f;
}
