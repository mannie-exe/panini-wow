// panini.hlsl: PaniniClassicWoW post-process pixel shader
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

#include "panini_common.hlsli"

float4 paniniSample(float2 texcoord, float D, float halfTan, float zoom, float S, float aspect, float2 gradX, float2 gradY) {
    float2 maxRectXY = float2(halfTan * aspect, halfTan);
    float2 screenXY = (texcoord * 2.0 - 1.0) / max(zoom, 0.001);
    float2 projXY = screenXY * maxRectXY;
    float3 ray = paniniInverse(projXY, D, S);
    float2 srcUV = (ray.xy / ray.z) / maxRectXY * 0.5 + 0.5;

    if (srcUV.x >= 0.0 && srcUV.x <= 1.0 && srcUV.y >= 0.0 && srcUV.y <= 1.0)
        return tex2Dgrad(sceneTex, srcUV, gradX, gradY);

    return float4(0.0, 0.0, 0.0, 1.0);
}

float4 main(float2 texcoord : TEXCOORD0) : COLOR0 {
    float D       = u_params.x;
    float halfTan = u_params.y;
    float zoom    = u_params.z;
    float S       = u_extra.x;
    float aspect  = u_extra.y;

    if (D < 0.001)
        return tex2D(sceneTex, texcoord);

    float2 gradX = ddx(texcoord);
    float2 gradY = ddy(texcoord);

    return paniniSample(texcoord, D, halfTan, zoom, S, aspect, gradX, gradY);
}
