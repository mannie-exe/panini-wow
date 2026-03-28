// cas.hlsl: Contrast Adaptive Sharpening (CAS) for D3D9
// Target: ps_3_0
// Ported from butterw CAS dx9 (gist ceb89a68bc0aa3b0e317660fb4bacaa3)
//
// Register mapping:
//   s0  = input texture
//   c1  = { 1.0/width, 1.0/height, 0, 0 }
//   c2  = { casSharpness, 0, 0, 0 }

sampler s0 : register(s0);
float2 p1  : register(c1);
float  cas : register(c2);
#define px p1.x
#define py p1.y
#define texo(x,y) tex2D(s0, tex + float2(x,y)).rgb
#define min4(x1,x2,x3,x4) min(min(x1,x2),min(x3,x4))
#define max4(x1,x2,x3,x4) max(max(x1,x2),max(x3,x4))

float4 main(float2 tex : TEXCOORD0) : COLOR {
    float peak = 3.0 * cas - 8.0;
    if (peak >= 0.0)
        return tex2D(s0, tex);

    float3 c2 = texo( 0, -py);
    float3 c4 = texo(-px, 0);
    float3 c6 = texo( px, 0);
    float3 c8 = texo( 0,  py);
    float4 ori = tex2D(s0, tex);
    float3 minRGB = min(min4(c2,c4,c6,c8), ori.rgb);
    float3 maxRGB = max(max4(c2,c4,c6,c8), ori.rgb);
    c2 = c2 + c4 + c6 + c8;

    float3 c1 = texo(-px,-py);
    float3 c9 = texo( px, py);
    float3 c3 = texo( px,-py);
    float3 c7 = texo(-px, py);
    minRGB += min(minRGB, min4(c1,c3,c7,c9));
    maxRGB += max(maxRGB, max4(c1,c3,c7,c9));
    minRGB = min(minRGB, 2 - maxRGB);

    float3 ampRGB = minRGB * rcp(maxRGB);
    float3 wRGB = sqrt(ampRGB) * 1.0 / peak;
    float3 sharp = ori.rgb + c2 * wRGB;
    sharp = saturate(sharp * rcp(4 * wRGB + 1.));

    ori.rgb = sharp;
    return ori;
}
