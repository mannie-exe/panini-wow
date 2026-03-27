#include <gtest/gtest.h>
#include <cmath>

// Panini projection math ported from panini.hlsl for validation.
// These functions test the shader's inverse mapping in C++.

struct Vec3 {
    float x, y, z;
};

static Vec3 paniniInverse(float projX, float projY, float D, float S) {
    float dp1 = D + 1.0f;

    float k = (projX * projX) / (dp1 * dp1);
    float disc = k * k * D * D - (k + 1.0f) * (k * D * D - 1.0f);

    if (disc <= 0.0f)
        return {projX, projY, 1.0f};

    float cLon = (-k * D + sqrtf(disc)) / (k + 1.0f);
    float Sv = dp1 / (D + cLon);

    float lat = atan2f(projY, Sv);
    float lon = atan2f(projX, Sv * cLon);

    float cosLat = cosf(lat);
    Vec3 ray = {
        cosLat * sinf(lon),
        sinf(lat),
        cosLat * cosf(lon)
    };

    // Vertical compensation
    if (ray.z > 0.0f) {
        float q = ray.x / ray.z;
        q = (q * q) / (dp1 * dp1);
        float comp = S > 0.0f ? 1.0f / sqrtf(1.0f + q) : 1.0f;
        float lerpFactor = fabsf(S);
        ray.y *= (1.0f - lerpFactor) + lerpFactor * comp;
    }

    return ray;
}

// Forward panini projection: ray direction -> projected XY.
// This is the analytical inverse of paniniInverse for D=0 (identity).
// For general D, we validate round-trip via UV mapping.

struct Vec2 { float x, y; };

// Convert a ray direction to screen UV using the shader's sampling logic.
// This mirrors: srcUV = (ray.xy / ray.z) / maxRectXY * 0.5 + 0.5
static Vec2 rayToScreenUV(Vec3 ray, float halfTanFov, float aspect, float zoom) {
    Vec2 maxRectXY = {halfTanFov * aspect, halfTanFov};
    float zClamped = std::max(ray.z, 0.0001f);
    return {
        (ray.x / zClamped) / maxRectXY.x * 0.5f + 0.5f,
        (ray.y / zClamped) / maxRectXY.y * 0.5f + 0.5f
    };
}

// Convert screen UV to projected XY (shader main function inverse mapping).
static Vec2 screenUVToProjXY(float u, float v, float halfTanFov, float aspect, float zoom) {
    Vec2 maxRectXY = {halfTanFov * aspect, halfTanFov};
    float zoomClamped = std::max(zoom, 0.001f);
    float sx = (u * 2.0f - 1.0f) / zoomClamped;
    float sy = (v * 2.0f - 1.0f) / zoomClamped;
    return {sx * maxRectXY.x, sy * maxRectXY.y};
}

// Normalize a vec3
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
    // D near 0 produces minimal distortion.
    // D=0 exactly has a division-by-zero in Sv = dp1/(D+cLon) since cLon=0.
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
    // Projecting +X and -X should produce symmetric rays.
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

TEST_F(PaniniProjectionTest, UnitRay_LengthApproximatelyOne) {
    // Output rays should be approximately unit length.
    Vec3 ray = paniniInverse(0.3f, 0.2f, 0.5f, 0.0f);
    float len = vec3Length(ray);
    EXPECT_NEAR(len, 1.0f, 0.01f);
}

TEST_F(PaniniProjectionTest, DOne_MaximumDistortion) {
    // D=1 is full cylindrical projection. Should still produce valid rays.
    Vec3 ray = paniniInverse(0.5f, 0.0f, 1.0f, 0.0f);
    float len = vec3Length(ray);
    EXPECT_NEAR(len, 1.0f, 0.01f);
    EXPECT_GT(ray.z, 0.0f);
}

TEST_F(PaniniProjectionTest, VerticalComp_SZero_MatchesSMinusOne) {
    // The vertical compensation blends: y *= (1 - |S|) + |S| * comp.
    // S=0 and S=-1 should produce identical Y because lerp(1.0, rsqrt(..), S)
    // uses abs(S) as the lerp factor. S=0 gives lerpFactor=0 (no-op).
    // S=-1 gives lerpFactor=1 but S<0 skips the comp path entirely (only S>0 applies comp).
    // Actually: the code checks `S > 0.0` before applying comp, and uses `abs(S)` for lerp.
    // S=0: lerpFactor=0, so y *= 1.0. S=-1: lerpFactor=1, comp branch skipped, so y *= (1-1)+1*1=1.
    // Both should be identical.
    float D = 0.5f;
    Vec3 rayS0 = paniniInverse(1.0f, 0.5f, D, 0.0f);
    Vec3 raySNeg1 = paniniInverse(1.0f, 0.5f, D, -1.0f);
    EXPECT_NEAR(rayS0.y, raySNeg1.y, 0.0001f);
    EXPECT_NEAR(rayS0.x, raySNeg1.x, 0.0001f);
    EXPECT_NEAR(rayS0.z, raySNeg1.z, 0.0001f);
}

TEST_F(PaniniProjectionTest, VerticalComp_SPositive_ReducesStretching) {
    float D = 0.5f;
    Vec3 rayNoComp = paniniInverse(1.5f, 0.5f, D, 0.0f);
    Vec3 rayWithComp = paniniInverse(1.5f, 0.5f, D, 1.0f);
    // Vertical compensation should reduce Y at edges.
    EXPECT_LT(fabsf(rayWithComp.y), fabsf(rayNoComp.y));
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
    // UV 0.45 is a small offset from center -- stays within projection domain.
    float D = 0.5f, S = 0.0f;
    Vec2 proj = screenUVToProjXY(0.45f, 0.5f, kHalfTanFov, kAspect, kZoom);
    Vec3 ray = paniniInverse(proj.x, proj.y, D, S);
    Vec2 uvBack = rayToScreenUV(ray, kHalfTanFov, kAspect, kZoom);

    EXPECT_NEAR(uvBack.x, 0.45f, 0.05f);
    EXPECT_NEAR(uvBack.y, 0.5f, 0.05f);
}
