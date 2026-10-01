#ifndef PANINI_MAPPING_H
#define PANINI_MAPPING_H

// Compiled by both C++ and HLSL so picking follows the displayed projection.
#ifdef __cplusplus
#include <cmath>
#define PANINI_INLINE inline
#define PANINI_OUT float&
#define PANINI_SQRT sqrtf
#else
#define PANINI_INLINE
#define PANINI_OUT out float
#define PANINI_SQRT sqrt
#endif

// Maps top-left display UV to source UV using the shader constants. Out-of-range
// results describe black borders; callers must not clamp them to the scene edge.
PANINI_INLINE void PaniniSourceUV(float u, float v, float D, float halfTan,
                                  float zoom, float S, float aspect,
                                  PANINI_OUT sourceU, PANINI_OUT sourceV) {
    sourceU = u;
    sourceV = v;
    if (D < 0.001f) return;

    float scaleX = halfTan * aspect;
    float safeZoom = zoom > 0.001f ? zoom : 0.001f;
    float x = (u * 2.0f - 1.0f) / safeZoom * scaleX;
    float y = (v * 2.0f - 1.0f) / safeZoom * halfTan;
    float distance = D + 1.0f;
    float hypSq = x * x + distance * distance;
    float discriminant = hypSq - x * x * D * D;

    // The old normalized fallback has the same x/z and y/z ratios.
    if (discriminant > 0.0f) {
        float cosine = (-x * x * D + distance * PANINI_SQRT(discriminant)) / hypSq;
        float factor = (cosine + D) / (distance * cosine);
        x *= factor;
        y *= factor;
        float q = x * x / (distance * distance);
        y *= 1.0f + S * (1.0f / PANINI_SQRT(1.0f + q) - 1.0f);
    }

    sourceU = x / scaleX * 0.5f + 0.5f;
    sourceV = y / halfTan * 0.5f + 0.5f;
}

#undef PANINI_INLINE
#undef PANINI_OUT
#undef PANINI_SQRT
#endif
