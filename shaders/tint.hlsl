// tint.hlsl — PaniniWoW debug shader
// Target: ps_3_0
// Reduces red channel by 33%. Visible cyan/green tint confirms the full
// shader pipeline (EndScene hook, StretchRect, shader load, quad draw).

sampler2D sceneTex : register(s0);

float4 main(float2 texcoord : TEXCOORD0) : COLOR0 {
    float4 color = tex2D(sceneTex, texcoord);
    color.r *= 0.67;
    return color;
}
