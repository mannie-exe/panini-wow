// fxaa.hlsl: FXAA 3.11 for D3D9 (Timothy Lottes, NVIDIA)
// Target: ps_3_0
// Simplified single-pass port. Quality preset 12 (5 search steps).
//
// Register mapping:
//   s0 = input texture (panini output or scene)
//   c0 = { 1.0/width, 1.0/height, 0, 0 }

sampler inputTex : register(s0);
float4 rcpFrame  : register(c0);

#define EDGE_THRESHOLD     0.166  // 1/6
#define EDGE_THRESHOLD_MIN 0.0833 // 1/12
#define SUBPIX_QUALITY     0.75

float4 main(float2 uv : TEXCOORD0) : COLOR0 {
    float2 invPx = rcpFrame.xy;

    float4 centerSample = tex2D(inputTex, uv);
    float lumaC = centerSample.g;

    float lumaN = tex2D(inputTex, uv + float2( 0, -invPx.y)).g;
    float lumaS = tex2D(inputTex, uv + float2( 0,  invPx.y)).g;
    float lumaE = tex2D(inputTex, uv + float2( invPx.x, 0)).g;
    float lumaW = tex2D(inputTex, uv + float2(-invPx.x, 0)).g;

    float lumaMin = min(lumaC, min(min(lumaN, lumaS), min(lumaE, lumaW)));
    float lumaMax = max(lumaC, max(max(lumaN, lumaS), max(lumaE, lumaW)));
    float lumaRange = lumaMax - lumaMin;

    if (lumaRange < max(EDGE_THRESHOLD_MIN, lumaMax * EDGE_THRESHOLD))
        return centerSample;

    float lumaNW = tex2D(inputTex, uv + float2(-invPx.x, -invPx.y)).g;
    float lumaNE = tex2D(inputTex, uv + float2( invPx.x, -invPx.y)).g;
    float lumaSW = tex2D(inputTex, uv + float2(-invPx.x,  invPx.y)).g;
    float lumaSE = tex2D(inputTex, uv + float2( invPx.x,  invPx.y)).g;

    float lumaNS = lumaN + lumaS;
    float lumaEW = lumaE + lumaW;
    float lumaNESE = lumaNE + lumaSE;
    float lumaNWNE = lumaNW + lumaNE;
    float lumaNWSW = lumaNW + lumaSW;
    float lumaSWSE = lumaSW + lumaSE;

    float edgeH = abs(lumaNWSW - 2.0 * lumaW) + abs(lumaNS - 2.0 * lumaC) * 2.0 + abs(lumaNESE - 2.0 * lumaE);
    float edgeV = abs(lumaNWNE - 2.0 * lumaN) + abs(lumaEW - 2.0 * lumaC) * 2.0 + abs(lumaSWSE - 2.0 * lumaS);
    float isHorz = step(edgeV, edgeH); // 1.0 if horizontal, 0.0 if vertical

    // Sub-pixel aliasing factor
    float subpixA = (2.0 * (lumaNS + lumaEW) + lumaNWSW + lumaNESE) * (1.0 / 12.0);
    float subpixB = saturate(abs(subpixA - lumaC) / lumaRange);
    float subpixC = (-2.0 * subpixB + 3.0) * subpixB * subpixB;
    float subpixFinal = subpixC * subpixC * SUBPIX_QUALITY;

    // Step direction perpendicular to edge
    float stepLen = lerp(invPx.x, invPx.y, isHorz);
    float lumaP = lerp(lumaE, lumaN, isHorz);
    float lumaN2 = lerp(lumaW, lumaS, isHorz);

    float gradP = abs(lumaP - lumaC);
    float gradN = abs(lumaN2 - lumaC);

    // Pick the side with steeper gradient
    float pickNeg = step(gradP, gradN); // 1.0 if negative side is steeper
    stepLen = lerp(stepLen, -stepLen, pickNeg);
    float lumaEdgeSide = lerp(lumaP, lumaN2, pickNeg);

    float2 edgeUV = uv;
    edgeUV += lerp(float2(stepLen * 0.5, 0), float2(0, stepLen * 0.5), isHorz);

    // Search direction along the edge
    float2 edgeStep = lerp(float2(0, invPx.y), float2(invPx.x, 0), isHorz);

    float edgeLuma = (lumaC + lumaEdgeSide) * 0.5;
    float gradThresh = lumaRange * 0.25;

    // Endpoint search: manually unrolled for ps_3_0
    // Positive direction: steps at 1.0, 1.5, 2.0, 4.0, 12.0
    float2 uvP = edgeUV + edgeStep * 1.0;
    float lumaEndP = tex2D(inputTex, uvP).g - edgeLuma;
    float doneP = step(gradThresh, abs(lumaEndP));

    uvP += edgeStep * 1.5 * (1.0 - doneP);
    lumaEndP = lerp(tex2D(inputTex, uvP).g - edgeLuma, lumaEndP, doneP);
    doneP = max(doneP, step(gradThresh, abs(lumaEndP)));

    uvP += edgeStep * 2.0 * (1.0 - doneP);
    lumaEndP = lerp(tex2D(inputTex, uvP).g - edgeLuma, lumaEndP, doneP);
    doneP = max(doneP, step(gradThresh, abs(lumaEndP)));

    uvP += edgeStep * 4.0 * (1.0 - doneP);
    lumaEndP = lerp(tex2D(inputTex, uvP).g - edgeLuma, lumaEndP, doneP);
    doneP = max(doneP, step(gradThresh, abs(lumaEndP)));

    uvP += edgeStep * 12.0 * (1.0 - doneP);
    lumaEndP = lerp(tex2D(inputTex, uvP).g - edgeLuma, lumaEndP, doneP);

    // Negative direction: steps at 1.0, 1.5, 2.0, 4.0, 12.0
    float2 uvN = edgeUV - edgeStep * 1.0;
    float lumaEndN = tex2D(inputTex, uvN).g - edgeLuma;
    float doneN = step(gradThresh, abs(lumaEndN));

    uvN -= edgeStep * 1.5 * (1.0 - doneN);
    lumaEndN = lerp(tex2D(inputTex, uvN).g - edgeLuma, lumaEndN, doneN);
    doneN = max(doneN, step(gradThresh, abs(lumaEndN)));

    uvN -= edgeStep * 2.0 * (1.0 - doneN);
    lumaEndN = lerp(tex2D(inputTex, uvN).g - edgeLuma, lumaEndN, doneN);
    doneN = max(doneN, step(gradThresh, abs(lumaEndN)));

    uvN -= edgeStep * 4.0 * (1.0 - doneN);
    lumaEndN = lerp(tex2D(inputTex, uvN).g - edgeLuma, lumaEndN, doneN);
    doneN = max(doneN, step(gradThresh, abs(lumaEndN)));

    uvN -= edgeStep * 12.0 * (1.0 - doneN);
    lumaEndN = lerp(tex2D(inputTex, uvN).g - edgeLuma, lumaEndN, doneN);

    // Distance to endpoints
    float distP = lerp(uvP.y - uv.y, uvP.x - uv.x, isHorz);
    float distN = lerp(uv.y - uvN.y, uv.x - uvN.x, isHorz);

    float distClosest = min(distP, distN);
    float edgeLength = distP + distN;
    float pixelOffset = -distClosest / edgeLength + 0.5;

    // Select endpoint luma for the closer side
    float closerP = step(distN, distP); // 1.0 if negative is closer
    float lumaEndClosest = lerp(lumaEndP, lumaEndN, closerP);

    // Reject if gradient at endpoint is in wrong direction
    float centerSign = step(0.0, lumaC - edgeLuma); // 1.0 if center > edge
    float endSign = step(0.0, lumaEndClosest);       // 1.0 if end > 0
    float sameSign = step(0.5, abs(centerSign - endSign)); // 1.0 if different signs
    float finalOffset = pixelOffset * sameSign;

    finalOffset = max(finalOffset, subpixFinal);

    float2 finalUV = uv;
    finalUV += lerp(float2(finalOffset * stepLen, 0), float2(0, finalOffset * stepLen), isHorz);

    return tex2D(inputTex, finalUV);
}
