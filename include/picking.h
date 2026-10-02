#pragma once

#include "panini_mapping.h"

// Exact constants used for a successfully rendered Panini frame.
struct PickingProjection {
    float strength = 0.0f;
    float halfTan = 1.0f;
    float zoom = 1.0f;
    float verticalComp = 0.0f;
    float aspect = 1.0f;
};

// Converts the client's bottom-left device coordinates through the full backbuffer
// projection. Returns false for black borders or invalid inputs, leaving x/y
// unchanged. Width/height are the client's device extents, not pixel dimensions.
inline bool RemapPickingPoint(const PickingProjection& p, float width, float height,
                              float& x, float& y) {
    if (!(std::isfinite(width) && width > 0.0f &&
          std::isfinite(height) && height > 0.0f &&
          std::isfinite(x) && std::isfinite(y))) return false;
    if (x < 0.0f || x > width || y < 0.0f || y > height) return false;
    if (!std::isfinite(p.strength)) return false;
    if (p.strength < 0.001f) return true;
    if (!(std::isfinite(p.halfTan) && p.halfTan > 0.0f &&
          std::isfinite(p.aspect) && p.aspect > 0.0f &&
          std::isfinite(p.zoom) && p.zoom > 0.0f &&
          std::isfinite(p.verticalComp))) return false;

    float u, v;
    PaniniSourceUV(x / width, 1.0f - y / height, p.strength, p.halfTan,
                   p.zoom, p.verticalComp, p.aspect, u, v);
    if (!(u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f)) return false;
    x = u * width;
    y = (1.0f - v) * height;
    return true;
}

// Installs on the game/render thread for a mapped client PE image. Returns false
// without patching unknown/modified code. Supports Classic 5875 and WotLK 12340.
bool Picking_Install(void* clientImage);

// Publishes rendered constants for the matching world frame and camera.
// Does nothing unless the validated picking hook is installed.
void Picking_Publish(void* worldFrame, const PickingProjection& projection);

// Discards the rendered snapshot on reset, failed rendering, or world exit.
void Picking_Invalidate();
