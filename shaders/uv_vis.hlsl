// uv_vis.hlsl: UV displacement visualization shader
// Target: ps_3_0
//
// Maps panini UV displacement delta to RGB:
//   Red   = horizontal distortion magnitude
//   Green = vertical distortion magnitude
//   Blue  = displacement vector length
//
// Register mapping:
//   c0  = { D, halfTanFov, fillZoom, enabled }
//   c1  = { S, aspect, 0, 0 }

float4 u_params : register(c0);
float4 u_extra  : register(c1);

#include "panini_common.hlsli"

float4 main(float2 texcoord : TEXCOORD0) : COLOR {
    float D       = u_params.x;
    float halfTan = u_params.y;
    float zoom    = u_params.z;
    float S       = u_extra.x;
    float aspect  = u_extra.y;

    if (D < 0.001)
        return float4(0.0, 0.0, 0.0, 1.0);

    float2 maxRectXY = float2(halfTan * aspect, halfTan);
    float2 screenXY = (texcoord * 2.0 - 1.0) / max(zoom, 0.001);
    float2 projXY = screenXY * maxRectXY;
    float3 ray = paniniInverse(projXY, D, S);
    float2 srcUV = (ray.xy / ray.z) / maxRectXY * 0.5 + 0.5;

    float2 delta = srcUV - texcoord;
    float2 scaled = delta * 20.0;

    float r = saturate(abs(scaled.x) * 0.5 + 0.5);
    float g = saturate(abs(scaled.y) * 0.5 + 0.5);
    float b = saturate(length(scaled) * 0.5);

    return float4(r, g, b, 1.0);
}
