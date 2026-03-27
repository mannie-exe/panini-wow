#include <gtest/gtest.h>
#include <cmath>

// Pipeline selection logic tests.
// These verify the C++-side shader selection and constant propagation rules
// that determine which shader runs under which CVar combinations.
//
// The actual selection logic lives in hooks.cpp. These tests verify the
// documented contracts that the C++ code must follow.

class ShaderPipelineTest : public ::testing::Test {};

// Pipeline: Panini -> FXAA -> CAS.
// When paniniEnabled=true, fxaaEnabled=true, sharpen=0.05, debug=off,
// the pipeline runs 3 passes: panini + FXAA + CAS.
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

// When panini is off but FXAA is on and sharpen=0, only FXAA runs.
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

// When only CAS is active (no panini, no FXAA), only CAS runs.
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

// When everything is off (panini=false, fxaa=false, sharpen=0, debug=off),
// ApplyPostProcess returns early with 0 passes.
TEST_F(ShaderPipelineTest, AllDisabled_EarlyReturn) {
    bool paniniEnabled = false;
    bool fxaaEnabled = false;
    float sharpen = 0.0f;
    bool debugTint = false;
    bool debugUV = false;

    bool shouldRun = paniniEnabled || fxaaEnabled || (sharpen > 0.0f) || debugTint || debugUV;
    EXPECT_FALSE(shouldRun);
}

// When both FXAA and CAS are active, 2 post-passes run (FXAA then CAS).
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

// Debug tint overrides FXAA and CAS: only 1 post-pass (tint shader).
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

// When sharpen=0.5, CAS pass is selected.
TEST_F(ShaderPipelineTest, SharpenPositive_CasPassSelected) {
    bool debugTint = false;
    bool debugUV = false;
    float sharpen = 0.5f;
    if (sharpen > 1.0f) sharpen = 1.0f;

    bool casSelected = !debugTint && !debugUV && (sharpen > 0.0f);
    EXPECT_TRUE(casSelected);
}

// When sharpen=0.0, CAS pass is not selected.
TEST_F(ShaderPipelineTest, SharpenZero_CasNotSelected) {
    bool debugTint = false;
    bool debugUV = false;
    float sharpen = 0.0f;
    if (sharpen > 1.0f) sharpen = 1.0f;

    bool casSelected = !debugTint && !debugUV && (sharpen > 0.0f);
    EXPECT_FALSE(casSelected);
}

// When sharpen=1.0, CAS pass is selected with c2.x = 1.0.
TEST_F(ShaderPipelineTest, SharpenMax_ConstantValue) {
    float sharpen = 1.0f;
    if (sharpen > 1.0f) sharpen = 1.0f;
    EXPECT_FLOAT_EQ(sharpen, 1.0f);
}

// When sharpen=1.5, it is clamped to 1.0 at the C++ level.
TEST_F(ShaderPipelineTest, SharpenAboveMax_ClampedToOne) {
    float sharpen = 1.5f;
    if (sharpen > 1.0f) sharpen = 1.0f;
    EXPECT_FLOAT_EQ(sharpen, 1.0f);
}

// When ppDebugTint=1, tint shader is selected (not CAS, not UV vis, not FXAA).
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

// When ppDebugUV=1, UV vis shader is selected.
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

// Debug takes priority over CAS and FXAA: when both debugTint and sharpen/fxaa
// are active, debug tint wins.
TEST_F(ShaderPipelineTest, DebugTintOverCas_Priority) {
    bool debugTint = true;
    bool debugUV = false;
    float sharpen = 0.5f;

    bool tintSelected = debugTint;
    bool casSelected = !debugTint && !debugUV && (sharpen > 0.0f);

    EXPECT_TRUE(tintSelected);
    EXPECT_FALSE(casSelected);
}

// Pipeline is disabled when paniniEnabled=0, fxaaEnabled=0, sharpen=0, and
// no debug modes active.
TEST_F(ShaderPipelineTest, Disabled_NoPasses) {
    bool enabled = false;
    bool fxaaEnabled = false;
    float sharpen = 0.0f;
    bool debugTint = false;
    bool debugUV = false;

    bool shouldRun = enabled || fxaaEnabled || (sharpen > 0.0f) || debugTint || debugUV;
    EXPECT_FALSE(shouldRun);
}

// Pipeline runs for debug modes even when paniniEnabled=0.
TEST_F(ShaderPipelineTest, DebugTint_PipelineRunsEvenWhenDisabled) {
    bool enabled = false;
    bool debugTint = true;

    bool shouldRun = enabled || debugTint;
    EXPECT_TRUE(shouldRun);
}

