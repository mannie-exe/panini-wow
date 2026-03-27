#pragma once

#include <cmath>

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

inline bool IsValidFov(float f) {
    return f > 0.05f && f < 3.094f && f == f;
}
