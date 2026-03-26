// panini.hlsl — PaniniWoW post-process pixel shader
// Target: ps_3_0 (D3D9 Shader Model 3.0)
//
// Applies panini/cylindrical projection to the rendered scene.
// Ported from fabric engine (shaders/panini/fs_panini.sc).
//
// Register mapping:
//   s0  = scene texture (backbuffer copy)
//   c0  = { D (strength), halfTanFov, fillZoom, enabled }
//   c1  = { S (verticalComp), aspect, 0, 0 }

sampler2D sceneTex : register(s0);

float4 u_params : register(c0); // x=D, y=halfTanFov, z=fillZoom, w=enabled
float4 u_extra  : register(c1); // x=S, y=aspect, z=0, w=0

// Panini inverse mapping: screen-space XY -> view-space ray direction.
//
// Given a point on the panini projection plane, compute the 3D ray direction
// that would project to that point on a cylinder of radius (D+1).
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
    float dp1 = D + 1.0;

    // Solve quadratic for cos(longitude) from the projected X coordinate.
    float k = (projXY.x * projXY.x) / (dp1 * dp1);
    float disc = k * k * D * D - (k + 1.0) * (k * D * D - 1.0);

    // Negative discriminant = outside projection domain; graceful fallback.
    if (disc <= 0.0)
        return normalize(float3(projXY, 1.0));

    float c_lon = (-k * D + sqrt(disc)) / (k + 1.0);

    // Recover the cylinder scale factor for this longitude.
    float Sv = dp1 / (D + c_lon);

    // Compute latitude and longitude angles.
    float2 ang = float2(
        atan2(projXY.y, Sv),          // latitude
        atan2(projXY.x, Sv * c_lon)   // longitude
    );

    // Convert spherical angles to 3D ray direction.
    float cos_lat = cos(ang.x);
    float3 ray = float3(
        cos_lat * sin(ang.y),  // x
        sin(ang.x),            // y
        cos_lat * cos(ang.y)   // z (depth)
    );

    // Vertical compensation: reduce vertical stretching at edges.
    if (ray.z > 0.0) {
        float q = ray.x / ray.z;
        q = (q * q) / (dp1 * dp1);
        ray.y *= lerp(1.0, rsqrt(1.0 + q), S);
    }

    return ray;
}

float4 main(float2 texcoord : TEXCOORD0) : COLOR0 {
    float D       = u_params.x;
    float halfTan = u_params.y;
    float zoom    = u_params.z;
    float S       = u_extra.x;
    float aspect  = u_extra.y;

    // Passthrough when disabled or negligible strength.
    if (D < 0.001)
        return tex2D(sceneTex, texcoord);

    float2 maxRectXY = float2(halfTan * aspect, halfTan);

    // Map [0,1] UV to [-1,1] screen space, apply zoom.
    float2 screenXY = (texcoord * 2.0 - 1.0) / max(zoom, 0.001);
    float2 projXY   = screenXY * maxRectXY;

    float3 ray = paniniInverse(projXY, D, S);

    float2 srcUV = (ray.xy / ray.z) / maxRectXY * 0.5 + 0.5;

    if (srcUV.x >= 0.0 && srcUV.x <= 1.0 && srcUV.y >= 0.0 && srcUV.y <= 1.0)
        return tex2D(sceneTex, srcUV);

    return float4(0.0, 0.0, 0.0, 1.0);
}
