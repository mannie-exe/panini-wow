#include <gtest/gtest.h>

// Lua slider math validation (C++ equivalent).
// From PaniniClassicWoW_Settings.lua:
//   STEP_DIVISOR = 200
//   step = (maxVal - minVal) / STEP_DIVISOR
//
// Sliders:
//   Strength:      [0, 0.10]
//   Vertical Comp: [-1, 1]
//   Fill:          [0, 1]
//   FOV:           [0.1, 3.09]

static constexpr int STEP_DIVISOR = 200;

static float sliderStep(float minVal, float maxVal) {
    return (maxVal - minVal) / static_cast<float>(STEP_DIVISOR);
}

static float snapToStep(float value, float minVal, float maxVal) {
    float step = sliderStep(minVal, maxVal);
    float normalized = (value - minVal) / step;
    float snapped = std::round(normalized) * step + minVal;
    // Clamp to [min, max]
    if (snapped < minVal) return minVal;
    if (snapped > maxVal) return maxVal;
    return snapped;
}

class SliderStepTest : public ::testing::Test {};

TEST_F(SliderStepTest, StrengthStepSize) {
    float step = sliderStep(0.0f, 0.10f);
    EXPECT_FLOAT_EQ(step, 0.0005f);
}

TEST_F(SliderStepTest, VerticalCompStepSize) {
    float step = sliderStep(-1.0f, 1.0f);
    EXPECT_FLOAT_EQ(step, 0.01f);
}

TEST_F(SliderStepTest, FillStepSize) {
    float step = sliderStep(0.0f, 1.0f);
    EXPECT_FLOAT_EQ(step, 0.005f);
}

TEST_F(SliderStepTest, FovStepSize) {
    float step = sliderStep(0.1f, 3.09f);
    EXPECT_NEAR(step, 0.01495f, 0.0001f);
}

TEST_F(SliderStepTest, StrengthDivisionCount) {
    // 0.10 / 0.0005 = 200 steps exactly
    float step = sliderStep(0.0f, 0.10f);
    float count = std::round((0.10f) / step);
    EXPECT_EQ(static_cast<int>(count), STEP_DIVISOR);
}

TEST_F(SliderStepTest, VerticalDivisionCount) {
    float step = sliderStep(-1.0f, 1.0f);
    float count = std::round(2.0f / step);
    EXPECT_EQ(static_cast<int>(count), STEP_DIVISOR);
}

TEST_F(SliderStepTest, FillDivisionCount) {
    float step = sliderStep(0.0f, 1.0f);
    float count = std::round(1.0f / step);
    EXPECT_EQ(static_cast<int>(count), STEP_DIVISOR);
}

TEST_F(SliderStepTest, FovDivisionCount) {
    float step = sliderStep(0.1f, 3.09f);
    float range = 3.09f - 0.1f;
    float count = std::round(range / step);
    EXPECT_EQ(static_cast<int>(count), STEP_DIVISOR);
}

TEST_F(SliderStepTest, SnapStrength_ExactStep) {
    float snapped = snapToStep(0.0285f, 0.0f, 0.10f);
    // 0.0285 / 0.0005 = 57.0 (exact step)
    EXPECT_NEAR(snapped, 0.0285f, 0.00001f);
}

TEST_F(SliderStepTest, SnapStrength_OffStep) {
    float snapped = snapToStep(0.0286f, 0.0f, 0.10f);
    // 0.0286 / 0.0005 = 57.2, rounds to 57 -> 0.0285
    EXPECT_NEAR(snapped, 0.0285f, 0.00001f);
}

TEST_F(SliderStepTest, SnapFov_Default) {
    float snapped = snapToStep(2.6565f, 0.1f, 3.09f);
    // Verify the user's default FOV value snaps cleanly
    float step = sliderStep(0.1f, 3.09f);
    float normalized = (2.6565f - 0.1f) / step;
    EXPECT_NEAR(std::round(normalized) * step + 0.1f, snapped, 0.00001f);
}

TEST_F(SliderStepTest, SnapBelowMin_ClampsToMin) {
    float snapped = snapToStep(-5.0f, 0.0f, 0.10f);
    EXPECT_FLOAT_EQ(snapped, 0.0f);
}

TEST_F(SliderStepTest, SnapAboveMax_ClampsToMax) {
    float snapped = snapToStep(5.0f, 0.0f, 0.10f);
    EXPECT_FLOAT_EQ(snapped, 0.10f);
}
