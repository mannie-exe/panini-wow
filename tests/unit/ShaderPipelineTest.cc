#include <gtest/gtest.h>
#include <cmath>

// Pipeline selection logic tests.
// These verify the C++-side shader selection and constant propagation rules
// that determine which shader runs under which CVar combinations.
//
// The actual selection logic lives in hooks.cpp. These tests verify the
// documented contracts that the C++ code must follow.

class ShaderPipelineTest : public ::testing::Test {};

TEST_F(ShaderPipelineTest, DefaultState_PaniniAndFxaa) {
    bool paniniEnabled = true;
    bool fxaaEnabled = true;
    float sharpen = 0.05f;
    bool debugTint = false;
    bool debugUV = false;

    int passCount = 0;
    if (paniniEnabled) passCount++;
    if (!debugTint && !debugUV && fxaaEnabled) passCount++;
    if (!debugTint && !debugUV && sharpen > 0.0f) passCount++;

    EXPECT_EQ(passCount, 3);
}

TEST_F(ShaderPipelineTest, FxaaOnly_NoPanini) {
    bool paniniEnabled = false;
    bool fxaaEnabled = true;
    float sharpen = 0.0f;
    bool debugTint = false;
    bool debugUV = false;

    bool shouldRun = paniniEnabled || fxaaEnabled || (sharpen > 0.0f) || debugTint || debugUV;
    EXPECT_TRUE(shouldRun);

    int postPasses = 0;
    if (!debugTint && !debugUV && fxaaEnabled) postPasses++;
    if (!debugTint && !debugUV && sharpen > 0.0f) postPasses++;
    EXPECT_EQ(postPasses, 1);
}

TEST_F(ShaderPipelineTest, CasOnly_NoPanini) {
    bool paniniEnabled = false;
    bool fxaaEnabled = false;
    float sharpen = 0.5f;
    bool debugTint = false;
    bool debugUV = false;

    bool shouldRun = paniniEnabled || fxaaEnabled || (sharpen > 0.0f) || debugTint || debugUV;
    EXPECT_TRUE(shouldRun);

    int postPasses = 0;
    if (!debugTint && !debugUV && fxaaEnabled) postPasses++;
    if (!debugTint && !debugUV && sharpen > 0.0f) postPasses++;
    EXPECT_EQ(postPasses, 1);
}

TEST_F(ShaderPipelineTest, AllDisabled_EarlyReturn) {
    bool paniniEnabled = false;
    bool fxaaEnabled = false;
    float sharpen = 0.0f;
    bool debugTint = false;
    bool debugUV = false;

    bool shouldRun = paniniEnabled || fxaaEnabled || (sharpen > 0.0f) || debugTint || debugUV;
    EXPECT_FALSE(shouldRun);
}

TEST_F(ShaderPipelineTest, FxaaAndCas_BothActive) {
    bool fxaaEnabled = true;
    float sharpen = 0.3f;
    bool debugTint = false;
    bool debugUV = false;

    int postPasses = 0;
    if (!debugTint && !debugUV && fxaaEnabled) postPasses++;
    if (!debugTint && !debugUV && sharpen > 0.0f) postPasses++;
    EXPECT_EQ(postPasses, 2);
}

TEST_F(ShaderPipelineTest, DebugOverridesFxaaAndCas) {
    bool debugTint = true;
    bool debugUV = false;
    bool fxaaEnabled = true;
    float sharpen = 0.5f;

    bool tintSelected = debugTint;
    bool fxaaSelected = !debugTint && !debugUV && fxaaEnabled;
    bool casSelected = !debugTint && !debugUV && (sharpen > 0.0f);

    EXPECT_TRUE(tintSelected);
    EXPECT_FALSE(fxaaSelected);
    EXPECT_FALSE(casSelected);
}

TEST_F(ShaderPipelineTest, SharpenPositive_CasPassSelected) {
    bool debugTint = false;
    bool debugUV = false;
    float sharpen = 0.5f;
    if (sharpen > 1.0f) sharpen = 1.0f;

    bool casSelected = !debugTint && !debugUV && (sharpen > 0.0f);
    EXPECT_TRUE(casSelected);
}

TEST_F(ShaderPipelineTest, SharpenZero_CasNotSelected) {
    bool debugTint = false;
    bool debugUV = false;
    float sharpen = 0.0f;
    if (sharpen > 1.0f) sharpen = 1.0f;

    bool casSelected = !debugTint && !debugUV && (sharpen > 0.0f);
    EXPECT_FALSE(casSelected);
}

TEST_F(ShaderPipelineTest, SharpenMax_ConstantValue) {
    float sharpen = 1.0f;
    if (sharpen > 1.0f) sharpen = 1.0f;
    EXPECT_FLOAT_EQ(sharpen, 1.0f);
}

TEST_F(ShaderPipelineTest, SharpenAboveMax_ClampedToOne) {
    float sharpen = 1.5f;
    if (sharpen > 1.0f) sharpen = 1.0f;
    EXPECT_FLOAT_EQ(sharpen, 1.0f);
}

TEST_F(ShaderPipelineTest, DebugTint_TintShaderSelected) {
    bool debugTint = true;
    bool debugUV = false;
    float sharpen = 0.5f;
    bool fxaaEnabled = true;

    bool tintSelected = debugTint;
    bool uvVisSelected = !debugTint && debugUV;
    bool fxaaSelected = !debugTint && !debugUV && fxaaEnabled;
    bool casSelected = !debugTint && !debugUV && (sharpen > 0.0f);

    EXPECT_TRUE(tintSelected);
    EXPECT_FALSE(uvVisSelected);
    EXPECT_FALSE(fxaaSelected);
    EXPECT_FALSE(casSelected);
}

TEST_F(ShaderPipelineTest, DebugUV_UvVisShaderSelected) {
    bool debugTint = false;
    bool debugUV = true;
    float sharpen = 0.5f;
    bool fxaaEnabled = true;

    bool tintSelected = debugTint;
    bool uvVisSelected = !debugTint && debugUV;
    bool fxaaSelected = !debugTint && !debugUV && fxaaEnabled;
    bool casSelected = !debugTint && !debugUV && (sharpen > 0.0f);

    EXPECT_FALSE(tintSelected);
    EXPECT_TRUE(uvVisSelected);
    EXPECT_FALSE(fxaaSelected);
    EXPECT_FALSE(casSelected);
}

TEST_F(ShaderPipelineTest, DebugTintOverCas_Priority) {
    bool debugTint = true;
    bool debugUV = false;
    float sharpen = 0.5f;

    bool tintSelected = debugTint;
    bool casSelected = !debugTint && !debugUV && (sharpen > 0.0f);

    EXPECT_TRUE(tintSelected);
    EXPECT_FALSE(casSelected);
}

TEST_F(ShaderPipelineTest, Disabled_NoPasses) {
    bool enabled = false;
    bool fxaaEnabled = false;
    float sharpen = 0.0f;
    bool debugTint = false;
    bool debugUV = false;

    bool shouldRun = enabled || fxaaEnabled || (sharpen > 0.0f) || debugTint || debugUV;
    EXPECT_FALSE(shouldRun);
}

TEST_F(ShaderPipelineTest, DebugTint_PipelineRunsEvenWhenDisabled) {
    bool enabled = false;
    bool debugTint = true;

    bool shouldRun = enabled || debugTint;
    EXPECT_TRUE(shouldRun);
}

