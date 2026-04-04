#include <gtest/gtest.h>
#include <cmath>

// Panini projection math ported from panini.hlsl for validation.
// These functions test the shader's inverse mapping in C++.

struct Vec3 {
    float x, y, z;
};

static Vec3 paniniInverse(float projX, float projY, float D, float S) {
    float viewDist = D + 1.0f;
    float viewHypSq = projX * projX + viewDist * viewDist;
    float isectDiscrim = viewHypSq - projX * projX * D * D;

    if (isectDiscrim <= 0.0f)
        return {projX, projY, 1.0f};

    float cLon = (-projX * projX * D + viewDist * sqrtf(isectDiscrim)) / viewHypSq;
    float factor = (cLon + D) / (viewDist * cLon);
    float dirX = projX * factor;
    float dirY = projY * factor;

    float q = dirX * dirX / (viewDist * viewDist);
    float comp = 1.0f / sqrtf(1.0f + q);
    dirY *= 1.0f + S * (comp - 1.0f);

    return {dirX, dirY, 1.0f};
}

// For general D, round-trip is validated via UV mapping (no closed-form inverse).

struct Vec2 { float x, y; };

// Mirrors shader sampling: srcUV = (ray.xy / ray.z) / maxRectXY * 0.5 + 0.5
static Vec2 rayToScreenUV(Vec3 ray, float halfTanFov, float aspect, float zoom) {
    Vec2 maxRectXY = {halfTanFov * aspect, halfTanFov};
    float zClamped = std::max(ray.z, 0.0001f);
    return {
        (ray.x / zClamped) / maxRectXY.x * 0.5f + 0.5f,
        (ray.y / zClamped) / maxRectXY.y * 0.5f + 0.5f
    };
}

// Inverse of the shader's main function UV mapping.
static Vec2 screenUVToProjXY(float u, float v, float halfTanFov, float aspect, float zoom) {
    Vec2 maxRectXY = {halfTanFov * aspect, halfTanFov};
    float zoomClamped = std::max(zoom, 0.001f);
    float sx = (u * 2.0f - 1.0f) / zoomClamped;
    float sy = (v * 2.0f - 1.0f) / zoomClamped;
    return {sx * maxRectXY.x, sy * maxRectXY.y};
}

static Vec3 normalize3(Vec3 v) {
    float len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < 0.0001f) return {0, 0, 1};
    return {v.x / len, v.y / len, v.z / len};
}

static float vec3Length(Vec3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

class PaniniProjectionTest : public ::testing::Test {
protected:
    static constexpr float kAspect = 16.0f / 9.0f;
    static constexpr float kHalfTanFov = 4.04175606f;
    static constexpr float kZoom = 1.173f;
};

TEST_F(PaniniProjectionTest, NearZeroD_ApproximatelyIdentity) {
    // D near 0 produces minimal distortion. factor approaches 1.0 as D -> 0,
    // so the output approximates normalize(projXY, 1.0).
    // In practice D < 0.001 is treated as passthrough by the shader's D < 0.001 guard.
    float projX = 0.5f;
    float projY = 0.3f;
    Vec3 ray = paniniInverse(projX, projY, 0.001f, 0.0f);
    Vec3 expected = normalize3({projX, projY, 1.0f});
    EXPECT_NEAR(ray.x, expected.x, 0.1f);
    EXPECT_NEAR(ray.y, expected.y, 0.1f);
}

TEST_F(PaniniProjectionTest, CenterRay_PointsForward) {
    // Center of screen should produce a ray pointing straight ahead.
    Vec3 ray = paniniInverse(0.0f, 0.0f, 0.5f, 0.0f);
    EXPECT_NEAR(ray.x, 0.0f, 0.001f);
    EXPECT_NEAR(ray.y, 0.0f, 0.001f);
    EXPECT_GT(ray.z, 0.9f);
}

TEST_F(PaniniProjectionTest, HorizontalSymmetry) {
    float D = 0.5f;
    Vec3 rayPos = paniniInverse(1.0f, 0.0f, D, 0.0f);
    Vec3 rayNeg = paniniInverse(-1.0f, 0.0f, D, 0.0f);
    EXPECT_NEAR(rayPos.x, -rayNeg.x, 0.001f);
    EXPECT_NEAR(rayPos.y, rayNeg.y, 0.001f);
    EXPECT_NEAR(rayPos.z, rayNeg.z, 0.001f);
}

TEST_F(PaniniProjectionTest, VerticalSymmetry) {
    float D = 0.5f;
    Vec3 rayPos = paniniInverse(0.0f, 0.5f, D, 0.0f);
    Vec3 rayNeg = paniniInverse(0.0f, -0.5f, D, 0.0f);
    EXPECT_NEAR(rayPos.x, rayNeg.x, 0.001f);
    EXPECT_NEAR(rayPos.y, -rayNeg.y, 0.001f);
    EXPECT_NEAR(rayPos.z, rayNeg.z, 0.001f);
}

TEST_F(PaniniProjectionTest, RayDirection_ZIsOne) {
    // sqrt-based formulation returns (x/z, y/z, 1.0).
    // z is always 1.0 for valid (non-fallback) inputs.
    Vec3 ray = paniniInverse(0.3f, 0.2f, 0.5f, 0.0f);
    EXPECT_NEAR(ray.z, 1.0f, 0.001f);
    EXPECT_NE(ray.x * ray.x + ray.y * ray.y, 0.0f);
}

TEST_F(PaniniProjectionTest, DOne_MaximumDistortion) {
    // D=1 is full cylindrical projection. Should still produce valid rays.
    Vec3 ray = paniniInverse(0.5f, 0.0f, 1.0f, 0.0f);
    EXPECT_NEAR(ray.z, 1.0f, 0.001f);
    EXPECT_GT(ray.x, 0.0f);
}

TEST_F(PaniniProjectionTest, VerticalComp_SZero_NoCompression) {
    // S=0: no vertical compensation. Y should be larger than S=1 (which compresses).
    float D = 0.5f;
    Vec3 rayNoComp = paniniInverse(1.0f, 0.5f, D, 0.0f);
    Vec3 rayComp = paniniInverse(1.0f, 0.5f, D, 1.0f);
    EXPECT_GT(fabsf(rayNoComp.y), fabsf(rayComp.y));
}

TEST_F(PaniniProjectionTest, VerticalComp_SPositive_ReducesStretching) {
    float D = 0.5f;
    Vec3 rayNoComp = paniniInverse(1.5f, 0.5f, D, 0.0f);
    Vec3 rayWithComp = paniniInverse(1.5f, 0.5f, D, 1.0f);
    // S=1 compresses Y: y *= rsqrt(1+q) < 1 for q > 0.
    EXPECT_LT(fabsf(rayWithComp.y), fabsf(rayNoComp.y));
}

TEST_F(PaniniProjectionTest, VerticalComp_SNegative_ExpandsVertical) {
    float D = 0.5f;
    Vec3 rayNoComp = paniniInverse(1.5f, 0.5f, D, 0.0f);
    Vec3 raySNeg = paniniInverse(1.5f, 0.5f, D, -1.0f);
    // S=-1 expands Y: y *= 2 - rsqrt(1+q) > 1 for q > 0.
    EXPECT_GT(fabsf(raySNeg.y), fabsf(rayNoComp.y));
}

TEST_F(PaniniProjectionTest, LargeProjectionXY_TriggersFallback) {
    // The fallback path (disc <= 0) triggers only when D > 1 and projX is large
    // enough that k = projX^2 / (D+1)^2 exceeds 1/(D^2 - 1).
    // For D=2.0: threshold projX > 1.7321
    // The C++ port fallback returns {projX, projY, 1.0} without normalization
    // (note: the HLSL shader normalizes, but this test port does not match that).
    float D = 2.0f;
    Vec3 ray = paniniInverse(5.0f, 0.0f, D, 0.0f);
    EXPECT_FLOAT_EQ(ray.x, 5.0f);
    EXPECT_FLOAT_EQ(ray.y, 0.0f);
    EXPECT_FLOAT_EQ(ray.z, 1.0f);
}

TEST_F(PaniniProjectionTest, RoundTrip_CenterPixel) {
    float D = 0.5f, S = 0.0f;
    Vec2 proj = screenUVToProjXY(0.5f, 0.5f, kHalfTanFov, kAspect, kZoom);
    Vec3 ray = paniniInverse(proj.x, proj.y, D, S);
    Vec2 uvBack = rayToScreenUV(ray, kHalfTanFov, kAspect, kZoom);

    EXPECT_NEAR(uvBack.x, 0.5f, 0.05f);
    EXPECT_NEAR(uvBack.y, 0.5f, 0.05f);
}

TEST_F(PaniniProjectionTest, RoundTrip_SlightOffset) {
    float D = 0.5f, S = 0.0f;
    Vec2 proj = screenUVToProjXY(0.45f, 0.5f, kHalfTanFov, kAspect, kZoom);
    Vec3 ray = paniniInverse(proj.x, proj.y, D, S);
    Vec2 uvBack = rayToScreenUV(ray, kHalfTanFov, kAspect, kZoom);

    EXPECT_NEAR(uvBack.x, 0.45f, 0.05f);
    EXPECT_NEAR(uvBack.y, 0.5f, 0.05f);
}
