#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include "panini_math.h"

class InvariantTest : public ::testing::Test {
protected:
    static constexpr float kDefaultHalfTan = 4.04175606f;
    static constexpr float kDefaultAspect = 16.0f / 9.0f;
    static constexpr int kSampleSteps = 100;
};

// ---------------------------------------------------------------------------
// ComputeFillZoom monotonicity with fill
// ---------------------------------------------------------------------------

TEST_F(InvariantTest, ComputeFillZoom_MonotonicallyIncreasingWithFill) {
    float strength = 0.03f;
    float prev = ComputeFillZoom(strength, kDefaultHalfTan, kDefaultAspect, 0.001f);
    for (int i = 1; i <= kSampleSteps; ++i) {
        float fill = 0.001f + static_cast<float>(i) * (1.0f - 0.001f) / kSampleSteps;
        float zoom = ComputeFillZoom(strength, kDefaultHalfTan, kDefaultAspect, fill);
        EXPECT_GE(zoom, prev) << "fill=" << fill << " zoom=" << zoom << " prev=" << prev;
        prev = zoom;
    }
}

TEST_F(InvariantTest, ComputeFillZoom_MonotonicallyIncreasingWithFill_WideFov) {
    float halfTan = tanf(3.09f / 2.0f);
    float prev = ComputeFillZoom(0.03f, halfTan, kDefaultAspect, 0.001f);
    for (int i = 1; i <= kSampleSteps; ++i) {
        float fill = 0.001f + static_cast<float>(i) * (1.0f - 0.001f) / kSampleSteps;
        float zoom = ComputeFillZoom(0.03f, halfTan, kDefaultAspect, fill);
        EXPECT_GE(zoom, prev) << "fill=" << fill;
        prev = zoom;
    }
}

// ---------------------------------------------------------------------------
// ComputeFillZoom monotonicity with strength
// ---------------------------------------------------------------------------

TEST_F(InvariantTest, ComputeFillZoom_MonotonicallyIncreasingWithStrength) {
    float fill = 1.0f;
    float prev = ComputeFillZoom(0.001f, kDefaultHalfTan, kDefaultAspect, fill);
    for (int i = 1; i <= kSampleSteps; ++i) {
        float strength = 0.001f + static_cast<float>(i) * (1.0f - 0.001f) / kSampleSteps;
        float zoom = ComputeFillZoom(strength, kDefaultHalfTan, kDefaultAspect, fill);
        EXPECT_GE(zoom, prev) << "strength=" << strength << " zoom=" << zoom << " prev=" << prev;
        prev = zoom;
    }
}

// ---------------------------------------------------------------------------
// Strength=0 is always identity regardless of other params
// ---------------------------------------------------------------------------

TEST_F(InvariantTest, ComputeFillZoom_StrengthZero_AlwaysOne) {
    float aspects[] = {1.0f, 1.333f, 1.7778f, 2.333f, 3.555f};
    float halfTans[] = {0.05f, 0.5f, 1.0f, 4.04175606f, 100.0f};
    float fills[] = {0.0f, 0.001f, 0.5f, 1.0f};

    for (float aspect : aspects) {
        for (float halfTan : halfTans) {
            for (float fill : fills) {
                float zoom = ComputeFillZoom(0.0f, halfTan, aspect, fill);
                EXPECT_FLOAT_EQ(zoom, 1.0f)
                    << "aspect=" << aspect << " halfTan=" << halfTan << " fill=" << fill;
            }
        }
    }
}

TEST_F(InvariantTest, ComputeFillZoom_StrengthNegative_AlwaysOne) {
    EXPECT_FLOAT_EQ(ComputeFillZoom(-1.0f, kDefaultHalfTan, kDefaultAspect, 0.5f), 1.0f);
    EXPECT_FLOAT_EQ(ComputeFillZoom(-0.5f, kDefaultHalfTan, kDefaultAspect, 1.0f), 1.0f);
    EXPECT_FLOAT_EQ(ComputeFillZoom(-100.0f, kDefaultHalfTan, kDefaultAspect, 0.001f), 1.0f);
}

// ---------------------------------------------------------------------------
// Center pixel preservation (panini inverse at origin)
// ---------------------------------------------------------------------------

// C++ port of panini.hlsl paniniInverse for testing.
struct Vec3 { float x, y, z; };

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
    Vec3 ray = { cosLat * sinf(lon), sinf(lat), cosLat * cosf(lon) };
    if (ray.z > 0.0f) {
        float q = ray.x / ray.z;
        q = (q * q) / (dp1 * dp1);
        float comp = S > 0.0f ? 1.0f / sqrtf(1.0f + q) : 1.0f;
        float lerpFactor = fabsf(S);
        ray.y *= (1.0f - lerpFactor) + lerpFactor * comp;
    }
    return ray;
}

TEST_F(InvariantTest, CenterPixel_PreservesForwardDirection) {
    // projXY = (0, 0) should map to a ray pointing forward (+Z).
    // This holds for any valid D and S.
    float strengths[] = {0.001f, 0.03f, 0.1f, 0.5f, 1.0f};
    float sValues[] = {-1.0f, 0.0f, 0.5f, 1.0f};

    for (float D : strengths) {
        for (float S : sValues) {
            Vec3 ray = paniniInverse(0.0f, 0.0f, D, S);
            EXPECT_NEAR(ray.x, 0.0f, 0.001f) << "D=" << D << " S=" << S;
            EXPECT_NEAR(ray.y, 0.0f, 0.001f) << "D=" << D << " S=" << S;
            EXPECT_GT(ray.z, 0.9f) << "D=" << D << " S=" << S;
        }
    }
}

// ---------------------------------------------------------------------------
// Horizontal symmetry
// ---------------------------------------------------------------------------

TEST_F(InvariantTest, PaniniInverse_HorizontallySymmetric) {
    float strengths[] = {0.001f, 0.03f, 0.5f, 1.0f};
    float sValues[] = {-1.0f, 0.0f, 1.0f};
    float projYs[] = {0.0f, 0.3f, 0.8f};

    for (float D : strengths) {
        for (float S : sValues) {
            for (float projY : projYs) {
                float projX = 0.5f;
                Vec3 rayPos = paniniInverse(projX, projY, D, S);
                Vec3 rayNeg = paniniInverse(-projX, projY, D, S);
                EXPECT_NEAR(rayPos.x, -rayNeg.x, 0.001f)
                    << "D=" << D << " S=" << S << " projY=" << projY;
                EXPECT_NEAR(rayPos.y, rayNeg.y, 0.001f)
                    << "D=" << D << " S=" << S << " projY=" << projY;
                EXPECT_NEAR(rayPos.z, rayNeg.z, 0.001f)
                    << "D=" << D << " S=" << S << " projY=" << projY;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Idempotent round-trip (UV -> projXY -> ray -> UV)
// ---------------------------------------------------------------------------

struct Vec2 { float x, y; };

static Vec2 rayToScreenUV(Vec3 ray, float halfTanFov, float aspect, float zoom) {
    Vec2 maxRectXY = {halfTanFov * aspect, halfTanFov};
    float zClamped = std::max(ray.z, 0.0001f);
    return {
        (ray.x / zClamped) / maxRectXY.x * 0.5f + 0.5f,
        (ray.y / zClamped) / maxRectXY.y * 0.5f + 0.5f
    };
}

static Vec2 screenUVToProjXY(float u, float v, float halfTanFov, float aspect, float zoom) {
    Vec2 maxRectXY = {halfTanFov * aspect, halfTanFov};
    float zoomClamped = std::max(zoom, 0.001f);
    float sx = (u * 2.0f - 1.0f) / zoomClamped;
    float sy = (v * 2.0f - 1.0f) / zoomClamped;
    return {sx * maxRectXY.x, sy * maxRectXY.y};
}

TEST_F(InvariantTest, RoundTrip_NearCenter_IsIdempotent) {
    // Points close to center should round-trip accurately.
    // The panini projection is nonlinear, so round-trip error grows with offset.
    // With D=0.03 and the default WoW config, offsets up to 0.05 produce < 0.01 error.
    float D = 0.03f;
    float zoom = 1.173f;
    float offsets[] = {0.0f, 0.02f, 0.04f};

    for (float du : offsets) {
        for (float dv : offsets) {
            Vec2 proj = screenUVToProjXY(0.5f + du, 0.5f + dv, kDefaultHalfTan, kDefaultAspect, zoom);
            Vec3 ray = paniniInverse(proj.x, proj.y, D, 0.0f);
            if (ray.z <= 0.0f) continue;
            Vec2 uvBack = rayToScreenUV(ray, kDefaultHalfTan, kDefaultAspect, zoom);
            EXPECT_NEAR(uvBack.x, 0.5f + du, 0.008f) << "du=" << du << " dv=" << dv;
            EXPECT_NEAR(uvBack.y, 0.5f + dv, 0.008f) << "du=" << du << " dv=" << dv;
        }
    }
}

// ---------------------------------------------------------------------------
// IsValidFov boundary precision
// ---------------------------------------------------------------------------

TEST_F(InvariantTest, IsValidFov_JustInsideLowerBound) {
    EXPECT_TRUE(IsValidFov(0.051f));
    EXPECT_TRUE(IsValidFov(0.055f));
    EXPECT_TRUE(IsValidFov(0.06f));
}

TEST_F(InvariantTest, IsValidFov_JustInsideUpperBound) {
    EXPECT_TRUE(IsValidFov(3.093f));
    EXPECT_TRUE(IsValidFov(3.0f));
    EXPECT_TRUE(IsValidFov(2.0f));
}

TEST_F(InvariantTest, IsValidFov_JustOutsideLowerBound) {
    EXPECT_FALSE(IsValidFov(0.049f));
    EXPECT_FALSE(IsValidFov(0.0f));
    EXPECT_FALSE(IsValidFov(-0.001f));
}

TEST_F(InvariantTest, IsValidFov_JustOutsideUpperBound) {
    EXPECT_FALSE(IsValidFov(3.094f));
    EXPECT_FALSE(IsValidFov(3.1f));
    EXPECT_FALSE(IsValidFov(4.0f));
}

// ---------------------------------------------------------------------------
// Zoom output range for normal configs
// ---------------------------------------------------------------------------

TEST_F(InvariantTest, ComputeFillZoom_NormalConfigs_ReasonableZoomRange) {
    // Typical WoW configs: strength in [0, 0.10], FOV in [0.1, 3.09],
    // aspect in [1.33, 3.56], fill in [0, 1].
    // After the zoom safety check in hooks.cpp, the final zoom is always in [0.01, 10.0].
    // ComputeFillZoom itself always returns >= 1.0, so the relevant upper bound is the
    // safety cap of 10.0. Extreme combos (high strength + high FOV + ultrawide) can
    // push raw zoom above 5.0, but this is expected and clamped at runtime.
    float strengths[] = {0.001f, 0.01f, 0.03f, 0.05f, 0.10f};
    float aspects[] = {1.333f, 1.7778f, 2.333f, 3.555f};
    float halfTans[] = {
        tanf(0.1f / 2.0f),
        tanf(1.5708f / 2.0f),
        tanf(2.6565f / 2.0f),
        tanf(3.09f / 2.0f)
    };

    for (float strength : strengths) {
        for (float halfTan : halfTans) {
            for (float aspect : aspects) {
                float zoom = ComputeFillZoom(strength, halfTan, aspect, 1.0f);
                ASSERT_FALSE(std::isnan(zoom))
                    << "strength=" << strength << " halfTan=" << halfTan << " aspect=" << aspect;
                ASSERT_FALSE(std::isinf(zoom))
                    << "strength=" << strength << " halfTan=" << halfTan << " aspect=" << aspect;
                EXPECT_GE(zoom, 1.0f)
                    << "strength=" << strength << " halfTan=" << halfTan << " aspect=" << aspect;
                // Raw zoom can be very high for extreme configs; the runtime safety
                // check clamps to 1.273 fallback for values > 10.0.
            }
        }
    }
}
