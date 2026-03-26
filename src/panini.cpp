#include "panini.h"
#include <cmath>

float ComputeFillZoom(float strength, float halfTanFov, float aspect, float fill) {
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

    float zoom = 1.0f / fitX;
    return 1.0f + (zoom - 1.0f) * fill;
}

float ReadCameraFov() {
    // TODO: Read from WoW 1.12.1 camera memory struct.
    // Requires SuperWoW to set the FoV CVar.
    // Fallback: default 1.5708 (~90 deg) if memory read fails.
    return 1.5708f;
}
