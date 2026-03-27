#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include "panini_math.h"

// ComputeFillZoom reference value from Python:
//   strength=0.0285, fov=2.6565, aspect=16/9, fill=1.0 -> zoom=1.17331648
//   halfTanFov = tan(2.6565/2) = 4.04175606

class PaniniMathTest : public ::testing::Test {
protected:
    static constexpr float kDefaultAspect = 16.0f / 9.0f;
    static constexpr float kDefaultHalfTan = 4.04175606f;
    static constexpr float kReferenceZoom = 1.17331648f;
};

TEST_F(PaniniMathTest, ComputeFillZoom_ZeroStrength_ReturnsOne) {
    EXPECT_FLOAT_EQ(ComputeFillZoom(0.0f, kDefaultHalfTan, kDefaultAspect, 1.0f), 1.0f);
}

TEST_F(PaniniMathTest, ComputeFillZoom_ZeroFill_ReturnsOne) {
    EXPECT_FLOAT_EQ(ComputeFillZoom(0.03f, kDefaultHalfTan, kDefaultAspect, 0.0f), 1.0f);
}

TEST_F(PaniniMathTest, ComputeFillZoom_VerySmallStrength_ReturnsOne) {
    EXPECT_FLOAT_EQ(ComputeFillZoom(0.0005f, kDefaultHalfTan, kDefaultAspect, 1.0f), 1.0f);
}

TEST_F(PaniniMathTest, ComputeFillZoom_SmallStrengthAboveThreshold_Computes) {
    float zoom = ComputeFillZoom(0.001f, kDefaultHalfTan, kDefaultAspect, 1.0f);
    EXPECT_GT(zoom, 1.0f);
    EXPECT_LT(zoom, 2.0f);
}

TEST_F(PaniniMathTest, ComputeFillZoom_KnownReferenceValue) {
    float zoom = ComputeFillZoom(0.0285f, kDefaultHalfTan, kDefaultAspect, 1.0f);
    EXPECT_NEAR(zoom, kReferenceZoom, 0.001f);
}

TEST_F(PaniniMathTest, ComputeFillZoom_HighFov_NearPi) {
    float halfTanHigh = tanf(3.09f / 2.0f);
    float zoom = ComputeFillZoom(0.03f, halfTanHigh, kDefaultAspect, 1.0f);
    EXPECT_FALSE(std::isnan(zoom));
    EXPECT_FALSE(std::isinf(zoom));
    EXPECT_GT(zoom, 0.0f);
}

TEST_F(PaniniMathTest, ComputeFillZoom_MaxStrength_LowFov) {
    float halfTanLow = tanf(0.1f / 2.0f);
    float zoom = ComputeFillZoom(1.0f, halfTanLow, kDefaultAspect, 1.0f);
    EXPECT_FALSE(std::isnan(zoom));
    EXPECT_FALSE(std::isinf(zoom));
    EXPECT_GT(zoom, 0.0f);
}

TEST_F(PaniniMathTest, ComputeFillZoom_NaNStrength_ProducesNaN) {
    float nan = std::numeric_limits<float>::quiet_NaN();
    float result = ComputeFillZoom(nan, kDefaultHalfTan, kDefaultAspect, 1.0f);
    EXPECT_TRUE(std::isnan(result));
}

TEST_F(PaniniMathTest, ComputeFillZoom_InfStrength_ProducesNaN) {
    float inf = std::numeric_limits<float>::infinity();
    float result = ComputeFillZoom(inf, kDefaultHalfTan, kDefaultAspect, 1.0f);
    EXPECT_TRUE(std::isnan(result));
}

TEST_F(PaniniMathTest, ComputeFillZoom_NegativeStrength_ReturnsOne) {
    EXPECT_FLOAT_EQ(ComputeFillZoom(-0.5f, kDefaultHalfTan, kDefaultAspect, 1.0f), 1.0f);
}

TEST_F(PaniniMathTest, ComputeFillZoom_NegativeFill_ReturnsOne) {
    EXPECT_FLOAT_EQ(ComputeFillZoom(0.03f, kDefaultHalfTan, kDefaultAspect, -1.0f), 1.0f);
}

TEST_F(PaniniMathTest, ComputeFillZoom_PartialFill_Interpolates) {
    float zoomFull = ComputeFillZoom(0.0285f, kDefaultHalfTan, kDefaultAspect, 1.0f);
    float zoomHalf = ComputeFillZoom(0.0285f, kDefaultHalfTan, kDefaultAspect, 0.5f);
    EXPECT_GT(zoomHalf, 1.0f);
    EXPECT_LT(zoomHalf, zoomFull);
}

TEST_F(PaniniMathTest, IsValidFov_Typical90Degrees) {
    EXPECT_TRUE(IsValidFov(1.5708f));
}

TEST_F(PaniniMathTest, IsValidFov_Typical149Degrees) {
    EXPECT_TRUE(IsValidFov(2.6f));
}

TEST_F(PaniniMathTest, IsValidFov_LowerBoundary_Excluded) {
    EXPECT_FALSE(IsValidFov(0.05f));
}

TEST_F(PaniniMathTest, IsValidFov_UpperBoundary_Excluded) {
    EXPECT_FALSE(IsValidFov(3.094f));
}

TEST_F(PaniniMathTest, IsValidFov_JustBelowUpperBound) {
    EXPECT_TRUE(IsValidFov(3.093f));
}

TEST_F(PaniniMathTest, IsValidFov_JustAboveLowerBound) {
    EXPECT_TRUE(IsValidFov(0.051f));
}

TEST_F(PaniniMathTest, IsValidFov_Zero) {
    EXPECT_FALSE(IsValidFov(0.0f));
}

TEST_F(PaniniMathTest, IsValidFov_Negative) {
    EXPECT_FALSE(IsValidFov(-1.0f));
}

TEST_F(PaniniMathTest, IsValidFov_NaN) {
    EXPECT_FALSE(IsValidFov(std::numeric_limits<float>::quiet_NaN()));
}

TEST_F(PaniniMathTest, IsValidFov_Inf) {
    EXPECT_FALSE(IsValidFov(std::numeric_limits<float>::infinity()));
}

