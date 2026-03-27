#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include "panini_math.h"

// Config parameter bounds validation.
// These test the documented valid ranges for each config parameter.
// The Lua addon enforces these ranges; the DLL relies on them.

class ConfigValidationTest : public ::testing::Test {};

// Strength: valid [0, 1], Lua enforces [0, 0.10] via slider
TEST_F(ConfigValidationTest, Strength_ZeroIsValid) {
    float zoom = ComputeFillZoom(0.0f, 1.0f, 1.7778f, 1.0f);
    EXPECT_FLOAT_EQ(zoom, 1.0f);
}

TEST_F(ConfigValidationTest, Strength_TypicalValue) {
    float zoom = ComputeFillZoom(0.0285f, tanf(2.6565f / 2.0f), 16.0f / 9.0f, 1.0f);
    EXPECT_FALSE(std::isnan(zoom));
    EXPECT_GT(zoom, 1.0f);
}

TEST_F(ConfigValidationTest, Strength_MaxSlider) {
    float zoom = ComputeFillZoom(0.10f, tanf(2.6565f / 2.0f), 16.0f / 9.0f, 1.0f);
    EXPECT_FALSE(std::isnan(zoom));
    EXPECT_GT(zoom, 1.0f);
}

TEST_F(ConfigValidationTest, Strength_AboveMaxSlider_ClampedByCompute) {
    // Values above 0.10 are technically valid inputs to ComputeFillZoom.
    // The Lua slider caps at 0.10 but the function itself handles any float.
    float zoom = ComputeFillZoom(1.0f, tanf(2.6565f / 2.0f), 16.0f / 9.0f, 1.0f);
    EXPECT_FALSE(std::isnan(zoom));
    EXPECT_GT(zoom, 0.0f);
}

// Vertical Comp: valid [-1, 1]
// ComputeFillZoom does not use vertical comp; that's a shader parameter.
// But we validate the bounds here to document the contract.

TEST_F(ConfigValidationTest, VerticalComp_NotUsedByComputeFillZoom) {
    // ComputeFillZoom does not take a verticalComp parameter.
    // The verticalComp value passes through to the shader as-is.
    // This test documents that contract: changing verticalComp does not
    // affect the zoom calculation (it only affects the shader's projection).
    float zoom = ComputeFillZoom(0.03f, 1.0f, 1.7778f, 1.0f);
    EXPECT_FALSE(std::isnan(zoom));
    EXPECT_GT(zoom, 1.0f);
}

// Fill: valid [0, 1]
TEST_F(ConfigValidationTest, Fill_Zero) {
    EXPECT_FLOAT_EQ(ComputeFillZoom(0.03f, 1.0f, 1.7778f, 0.0f), 1.0f);
}

TEST_F(ConfigValidationTest, Fill_One) {
    float zoom = ComputeFillZoom(0.03f, 1.0f, 1.7778f, 1.0f);
    EXPECT_GT(zoom, 1.0f);
}

TEST_F(ConfigValidationTest, Fill_Half_Interpolates) {
    float zoom1 = ComputeFillZoom(0.03f, 1.0f, 1.7778f, 1.0f);
    float zoom05 = ComputeFillZoom(0.03f, 1.0f, 1.7778f, 0.5f);
    EXPECT_GT(zoom05, 1.0f);
    EXPECT_LT(zoom05, zoom1);
}

TEST_F(ConfigValidationTest, Fill_Negative) {
    EXPECT_FLOAT_EQ(ComputeFillZoom(0.03f, 1.0f, 1.7778f, -0.5f), 1.0f);
}

// FOV: valid (0.05, 3.094) per IsValidFov
TEST_F(ConfigValidationTest, Fov_LowerBound_Exclusive) {
    EXPECT_FALSE(IsValidFov(0.05f));
}

TEST_F(ConfigValidationTest, Fov_UpperBound_Exclusive) {
    EXPECT_FALSE(IsValidFov(3.094f));
}

TEST_F(ConfigValidationTest, Fov_TypicalRange) {
    EXPECT_TRUE(IsValidFov(0.1f));
    EXPECT_TRUE(IsValidFov(1.5708f));   // 90 degrees
    EXPECT_TRUE(IsValidFov(2.6565f));   // ~152 degrees (user's default)
    EXPECT_TRUE(IsValidFov(3.09f));     // near max
}

TEST_F(ConfigValidationTest, Fov_BelowSliderMin) {
    // Lua slider min is 0.1. IsValidFov threshold is 0.05.
    EXPECT_TRUE(IsValidFov(0.06f));
    EXPECT_FALSE(IsValidFov(0.05f));
}

TEST_F(ConfigValidationTest, Fov_AboveSliderMax) {
    // Lua slider max is 3.09. IsValidFov threshold is 3.094.
    EXPECT_TRUE(IsValidFov(3.093f));
    EXPECT_FALSE(IsValidFov(3.094f));
}

TEST_F(ConfigValidationTest, Fov_NaN) {
    EXPECT_FALSE(IsValidFov(std::numeric_limits<float>::quiet_NaN()));
}

TEST_F(ConfigValidationTest, Fov_Infinity) {
    EXPECT_FALSE(IsValidFov(std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(IsValidFov(-std::numeric_limits<float>::infinity()));
}
