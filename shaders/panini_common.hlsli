#ifndef PANINI_COMMON_HLSLI
#define PANINI_COMMON_HLSLI

// Panini inverse: screen-space XY -> view-space ray direction (unnormalized).
// Returns (x/z, y/z, 1.0) so that ray.xy / ray.z gives the source UV ratio.
//
// sqrt-based Panini_Generic formulation. Mathematically equivalent to the
// atan2/sin/cos formulation but ~3x cheaper in ALU (~15 vs ~80 instructions).
//
// D=0  : rectilinear (no distortion, passthrough)
// D=1  : full cylindrical (Pannini stereographic)
// D=0.5: typical sweet spot for gaming
//
// S controls vertical compensation:
//   S=0  : no vertical compensation
//   S=1  : full vertical compression (keeps verticals straight at edges)
//   S=-1 : vertical expansion
float3 paniniInverse(float2 projXY, float D, float S) {
    float view_dist = D + 1.0;
    float view_hyp_sq = projXY.x * projXY.x + view_dist * view_dist;
    float isect_discrim = view_hyp_sq - projXY.x * projXY.x * D * D;

    if (isect_discrim <= 0.0)
        return normalize(float3(projXY, 1.0));

    float c_lon = (-projXY.x * projXY.x * D + view_dist * sqrt(isect_discrim)) / view_hyp_sq;

    float factor = (c_lon + D) / (view_dist * c_lon);
    float2 dir = projXY * factor;

    float q = dir.x * dir.x / (view_dist * view_dist);
    dir.y *= lerp(1.0, rsqrt(1.0 + q), S);

    return float3(dir, 1.0);
}

#endif // PANINI_COMMON_HLSLI
