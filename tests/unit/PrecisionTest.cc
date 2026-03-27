#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include "panini_math.h"

class PrecisionTest : public ::testing::Test {
protected:
    static constexpr float kDefaultHalfTan = 4.04175606f;
    static constexpr float kDefaultAspect = 16.0f / 9.0f;
};

// ---------------------------------------------------------------------------
// Determinism: same inputs always produce same output
// ---------------------------------------------------------------------------

TEST_F(PrecisionTest, ComputeFillZoom_Deterministic) {
    float inputs[][4] = {
        {0.0285f, 4.04175606f, 1.7778f, 1.0f},
        {0.03f, tanf(2.6565f / 2.0f), 16.0f / 9.0f, 0.8f},
        {0.10f, tanf(3.09f / 2.0f), 1.333f, 1.0f},
        {0.001f, 0.05f, 1.0f, 1.0f},
        {1.0f, tanf(0.1f / 2.0f), 3.555f, 0.5f},
        {0.5f, 1.0f, 2.333f, 0.001f},
    };

    for (auto& in : inputs) {
        float z1 = ComputeFillZoom(in[0], in[1], in[2], in[3]);
        float z2 = ComputeFillZoom(in[0], in[1], in[2], in[3]);
        float z3 = ComputeFillZoom(in[0], in[1], in[2], in[3]);
        EXPECT_EQ(z1, z2) << "strength=" << in[0] << " halfTan=" << in[1];
        EXPECT_EQ(z2, z3) << "strength=" << in[0] << " halfTan=" << in[1];
    }
}

// ---------------------------------------------------------------------------
// Python reference precision
// ---------------------------------------------------------------------------

// Python reference: compute_fill_zoom(0.0285, 4.04175606, 16/9, 1.0) = 1.17331648
TEST_F(PrecisionTest, ComputeFillZoom_MatchesPythonReference) {
    float zoom = ComputeFillZoom(0.0285f, 4.04175606f, 16.0f / 9.0f, 1.0f);
    EXPECT_NEAR(zoom, 1.17331648f, 0.0001f);
}

TEST_F(PrecisionTest, ComputeFillZoom_MatchesPythonReference_LowFov) {
    // Python: compute_fill_zoom(0.03, tan(0.5/2), 16/9, 1.0)
    // halfTan = tan(0.25) = 0.25534192
    // lon_max = atan(0.25534192 * 1.7778) = 0.426127
    // denom = 0.03 + cos(0.426127) = 0.03 + 0.910574 = 0.940574
    // k = 1.03 / 0.940574 = 1.095076
    // edge_x = 1.095076 * sin(0.426127) = 1.095076 * 0.413213 = 0.452646
    // fit_x = 0.452646 / 0.453962 = 0.997100
    // zoom = 1 / 0.997100 = 1.002908
    // result = 1.0 + (1.002908 - 1.0) * 1.0 = 1.002860
    float halfTan = tanf(0.25f);
    float zoom = ComputeFillZoom(0.03f, halfTan, 16.0f / 9.0f, 1.0f);
    EXPECT_NEAR(zoom, 1.00286f, 0.001f);
}

TEST_F(PrecisionTest, ComputeFillZoom_MatchesPythonReference_HalfFill) {
    // Same as primary reference but fill=0.5 -> result should be midpoint between 1.0 and 1.1733
    float zoom = ComputeFillZoom(0.0285f, 4.04175606f, 16.0f / 9.0f, 0.5f);
    float expected = 1.0f + (1.17331648f - 1.0f) * 0.5f;
    EXPECT_NEAR(zoom, expected, 0.001f);
}

// ---------------------------------------------------------------------------
// Ultrawide aspect ratios
// ---------------------------------------------------------------------------

TEST_F(PrecisionTest, ComputeFillZoom_Ultrawide21x9) {
    float aspect = 21.0f / 9.0f;
    float zoom = ComputeFillZoom(0.03f, kDefaultHalfTan, aspect, 1.0f);
    EXPECT_FALSE(std::isnan(zoom));
    EXPECT_FALSE(std::isinf(zoom));
    EXPECT_GT(zoom, 1.0f);
}

TEST_F(PrecisionTest, ComputeFillZoom_Ultrawide32x9) {
    float aspect = 32.0f / 9.0f;
    float zoom = ComputeFillZoom(0.03f, kDefaultHalfTan, aspect, 1.0f);
    EXPECT_FALSE(std::isnan(zoom));
    EXPECT_FALSE(std::isinf(zoom));
    EXPECT_GT(zoom, 1.0f);
}

TEST_F(PrecisionTest, ComputeFillZoom_Ultrawide32x9_HighStrength) {
    float aspect = 32.0f / 9.0f;
    float zoom = ComputeFillZoom(0.10f, kDefaultHalfTan, aspect, 1.0f);
    EXPECT_FALSE(std::isnan(zoom));
    EXPECT_FALSE(std::isinf(zoom));
}

// ---------------------------------------------------------------------------
// Portrait aspect ratios
// ---------------------------------------------------------------------------

TEST_F(PrecisionTest, ComputeFillZoom_Portrait9x16) {
    float aspect = 9.0f / 16.0f;
    float zoom = ComputeFillZoom(0.03f, kDefaultHalfTan, aspect, 1.0f);
    EXPECT_FALSE(std::isnan(zoom));
    EXPECT_FALSE(std::isinf(zoom));
    EXPECT_GT(zoom, 1.0f);
}

TEST_F(PrecisionTest, ComputeFillZoom_Portrait9x16_HighStrength) {
    float aspect = 9.0f / 16.0f;
    float zoom = ComputeFillZoom(0.10f, kDefaultHalfTan, aspect, 1.0f);
    EXPECT_FALSE(std::isnan(zoom));
    EXPECT_FALSE(std::isinf(zoom));
}

TEST_F(PrecisionTest, ComputeFillZoom_Portrait9x19dot5) {
    float aspect = 9.0f / 19.5f;
    float zoom = ComputeFillZoom(0.05f, kDefaultHalfTan, aspect, 1.0f);
    EXPECT_FALSE(std::isnan(zoom));
    EXPECT_FALSE(std::isinf(zoom));
}

// ---------------------------------------------------------------------------
// Extreme but valid FOV combinations: no denormals or zero
// ---------------------------------------------------------------------------

TEST_F(PrecisionTest, ComputeFillZoom_ExtremeFov_NoDenormals) {
    float strengths[] = {0.001f, 0.03f, 0.10f};
    float fovs[] = {0.06f, 0.1f, 1.0f, 1.5708f, 2.5f, 3.0f, 3.093f};

    for (float strength : strengths) {
        for (float fov : fovs) {
            float halfTan = tanf(fov / 2.0f);
            float zoom = ComputeFillZoom(strength, halfTan, kDefaultAspect, 1.0f);
            EXPECT_FALSE(std::isnan(zoom))
                << "strength=" << strength << " fov=" << fov;
            EXPECT_FALSE(std::isinf(zoom))
                << "strength=" << strength << " fov=" << fov;
            EXPECT_FALSE(std::fpclassify(zoom) == FP_SUBNORMAL)
                << "strength=" << strength << " fov=" << fov << " zoom=" << zoom;
        }
    }
}

TEST_F(PrecisionTest, ComputeFillZoom_MaxStrength_MaxFov_MinAspect) {
    float halfTan = tanf(3.093f / 2.0f);
    float zoom = ComputeFillZoom(1.0f, halfTan, 1.0f, 1.0f);
    EXPECT_FALSE(std::isnan(zoom));
    EXPECT_FALSE(std::isinf(zoom));
}

TEST_F(PrecisionTest, ComputeFillZoom_MinThreshold_NoDenormal) {
    // Just above the strength threshold (0.001) with low fill.
    float zoom = ComputeFillZoom(0.002f, 0.05f, 1.0f, 0.002f);
    EXPECT_FALSE(std::fpclassify(zoom) == FP_SUBNORMAL);
    EXPECT_FALSE(std::isnan(zoom));
}

// ---------------------------------------------------------------------------
// Edge case: very small aspect ratios (near-square or portrait)
// ---------------------------------------------------------------------------

TEST_F(PrecisionTest, ComputeFillZoom_SquareAspect) {
    float zoom = ComputeFillZoom(0.05f, kDefaultHalfTan, 1.0f, 1.0f);
    EXPECT_FALSE(std::isnan(zoom));
    EXPECT_FALSE(std::isinf(zoom));
    EXPECT_GE(zoom, 1.0f);
}

TEST_F(PrecisionTest, ComputeFillZoom_VeryWideAspect) {
    float zoom = ComputeFillZoom(0.05f, kDefaultHalfTan, 5.0f, 1.0f);
    EXPECT_FALSE(std::isnan(zoom));
    EXPECT_FALSE(std::isinf(zoom));
}
