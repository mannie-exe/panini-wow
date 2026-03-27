#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include "panini_math.h"

// ---------------------------------------------------------------------------
// This file tests runtime robustness properties of the panini config pipeline.
//
// The clamping logic in hooks.cpp ApplyPostProcess is:
//   strength:     NaN -> 0.5,  <0 -> 0.5,  >1 -> 1
//   verticalComp: NaN -> 0.0,  <-1 -> -1,  >1 -> 1
//   fill:         NaN -> 0.8,  <0 -> 0.8,  >1 -> 1
//
// The zoom safety check in ApplyPostProcess is:
//   NaN or <0.01 or >10 -> fallback to 1.273
//
// PostProcessConfig_ReadFromCVars in cvar.cpp reads raw CVar values with fallbacks.
// UpdateCameraFov in state.cpp manages FOV swap: enable saves user FOV, disable restores.
// ---------------------------------------------------------------------------

// Mirrors the clamping logic from hooks.cpp ApplyPostProcess lines 75-81.
struct ClampedConfig {
    float strength;
    float verticalComp;
    float fill;
};

static ClampedConfig clampConfig(float rawStrength, float rawVertComp, float rawFill) {
    ClampedConfig c;
    c.strength = rawStrength;
    if (c.strength != c.strength || c.strength < 0.0f) c.strength = 0.5f;
    if (c.strength > 1.0f) c.strength = 1.0f;

    c.verticalComp = rawVertComp;
    if (c.verticalComp != c.verticalComp) c.verticalComp = 0.0f;
    if (c.verticalComp < -1.0f) c.verticalComp = -1.0f;
    if (c.verticalComp > 1.0f) c.verticalComp = 1.0f;

    c.fill = rawFill;
    if (c.fill != c.fill || c.fill < 0.0f) c.fill = 0.8f;
    if (c.fill > 1.0f) c.fill = 1.0f;

    return c;
}

// Mirrors the zoom safety check from hooks.cpp line 107-108.
static float safeZoom(float rawZoom) {
    if (rawZoom != rawZoom || rawZoom < 0.01f || rawZoom > 10.0f)
        return 1.273f;
    return rawZoom;
}

class RuntimeRobustnessTest : public ::testing::Test {
protected:
    static constexpr float kDefaultHalfTan = 4.04175606f;
    static constexpr float kDefaultAspect = 16.0f / 9.0f;
};

// ---------------------------------------------------------------------------
// Config clamping: strength
// ---------------------------------------------------------------------------

TEST_F(RuntimeRobustnessTest, ClampStrength_NaN) {
    float nan = std::numeric_limits<float>::quiet_NaN();
    auto c = clampConfig(nan, 0.0f, 0.8f);
    EXPECT_FLOAT_EQ(c.strength, 0.5f);
}

TEST_F(RuntimeRobustnessTest, ClampStrength_Negative) {
    auto c = clampConfig(-0.5f, 0.0f, 0.8f);
    EXPECT_FLOAT_EQ(c.strength, 0.5f);
}

TEST_F(RuntimeRobustnessTest, ClampStrength_Zero) {
    auto c = clampConfig(0.0f, 0.0f, 0.8f);
    EXPECT_FLOAT_EQ(c.strength, 0.0f);
}

TEST_F(RuntimeRobustnessTest, ClampStrength_Normal) {
    auto c = clampConfig(0.03f, 0.0f, 0.8f);
    EXPECT_FLOAT_EQ(c.strength, 0.03f);
}

TEST_F(RuntimeRobustnessTest, ClampStrength_AboveOne) {
    auto c = clampConfig(2.0f, 0.0f, 0.8f);
    EXPECT_FLOAT_EQ(c.strength, 1.0f);
}

TEST_F(RuntimeRobustnessTest, ClampStrength_Inf) {
    auto c = clampConfig(std::numeric_limits<float>::infinity(), 0.0f, 0.8f);
    EXPECT_FLOAT_EQ(c.strength, 1.0f);
}

// ---------------------------------------------------------------------------
// Config clamping: verticalComp
// ---------------------------------------------------------------------------

TEST_F(RuntimeRobustnessTest, ClampVerticalComp_NaN) {
    float nan = std::numeric_limits<float>::quiet_NaN();
    auto c = clampConfig(0.03f, nan, 0.8f);
    EXPECT_FLOAT_EQ(c.verticalComp, 0.0f);
}

TEST_F(RuntimeRobustnessTest, ClampVerticalComp_BelowNegativeOne) {
    auto c = clampConfig(0.03f, -2.0f, 0.8f);
    EXPECT_FLOAT_EQ(c.verticalComp, -1.0f);
}

TEST_F(RuntimeRobustnessTest, ClampVerticalComp_AboveOne) {
    auto c = clampConfig(0.03f, 5.0f, 0.8f);
    EXPECT_FLOAT_EQ(c.verticalComp, 1.0f);
}

TEST_F(RuntimeRobustnessTest, ClampVerticalComp_FullNegativeRange) {
    auto c = clampConfig(0.03f, -1.0f, 0.8f);
    EXPECT_FLOAT_EQ(c.verticalComp, -1.0f);
}

TEST_F(RuntimeRobustnessTest, ClampVerticalComp_FullPositiveRange) {
    auto c = clampConfig(0.03f, 1.0f, 0.8f);
    EXPECT_FLOAT_EQ(c.verticalComp, 1.0f);
}

// ---------------------------------------------------------------------------
// Config clamping: fill
// ---------------------------------------------------------------------------

TEST_F(RuntimeRobustnessTest, ClampFill_NaN) {
    float nan = std::numeric_limits<float>::quiet_NaN();
    auto c = clampConfig(0.03f, 0.0f, nan);
    EXPECT_FLOAT_EQ(c.fill, 0.8f);
}

TEST_F(RuntimeRobustnessTest, ClampFill_Negative) {
    auto c = clampConfig(0.03f, 0.0f, -1.0f);
    EXPECT_FLOAT_EQ(c.fill, 0.8f);
}

TEST_F(RuntimeRobustnessTest, ClampFill_Zero) {
    auto c = clampConfig(0.03f, 0.0f, 0.0f);
    EXPECT_FLOAT_EQ(c.fill, 0.0f);
}

TEST_F(RuntimeRobustnessTest, ClampFill_AboveOne) {
    auto c = clampConfig(0.03f, 0.0f, 2.0f);
    EXPECT_FLOAT_EQ(c.fill, 1.0f);
}

TEST_F(RuntimeRobustnessTest, ClampFill_Inf) {
    auto c = clampConfig(0.03f, 0.0f, std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(c.fill, 1.0f);
}

// ---------------------------------------------------------------------------
// Zoom safety check
// ---------------------------------------------------------------------------

TEST_F(RuntimeRobustnessTest, SafeZoom_NaN) {
    float nan = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FLOAT_EQ(safeZoom(nan), 1.273f);
}

TEST_F(RuntimeRobustnessTest, SafeZoom_Zero) {
    EXPECT_FLOAT_EQ(safeZoom(0.0f), 1.273f);
}

TEST_F(RuntimeRobustnessTest, SafeZoom_TooSmall) {
    EXPECT_FLOAT_EQ(safeZoom(0.005f), 1.273f);
}

TEST_F(RuntimeRobustnessTest, SafeZoom_TooLarge) {
    EXPECT_FLOAT_EQ(safeZoom(20.0f), 1.273f);
}

TEST_F(RuntimeRobustnessTest, SafeZoom_Inf) {
    EXPECT_FLOAT_EQ(safeZoom(std::numeric_limits<float>::infinity()), 1.273f);
}

TEST_F(RuntimeRobustnessTest, SafeZoom_NegativeInf) {
    EXPECT_FLOAT_EQ(safeZoom(-std::numeric_limits<float>::infinity()), 1.273f);
}

TEST_F(RuntimeRobustnessTest, SafeZoom_ValidPassthrough) {
    EXPECT_FLOAT_EQ(safeZoom(1.0f), 1.0f);
    EXPECT_FLOAT_EQ(safeZoom(1.173f), 1.173f);
    EXPECT_FLOAT_EQ(safeZoom(0.01f), 0.01f);
    EXPECT_FLOAT_EQ(safeZoom(10.0f), 10.0f);
}

// ---------------------------------------------------------------------------
// FOV swap state machine (UpdateCameraFov logic)
// ---------------------------------------------------------------------------
// State machine from state.cpp:
//   - enabled && s_paniniWasEnabled=false -> set camera FOV to paniniFov, track enabled
//   - enabled && s_paniniWasEnabled=true  -> set camera FOV to paniniFov (already enabled)
//   - !enabled && s_paniniWasEnabled=true -> restore camera FOV to paniniUserFov
//   - !enabled && s_paniniWasEnabled=false -> no-op
// After each call, s_paniniWasEnabled = enabled

struct FovStateMachine {
    float cameraFov = 1.5708f;
    float paniniFov = 2.6565f;
    float paniniUserFov = 1.5708f;
    bool wasEnabled = false;

    void tick(bool enabled) {
        if (enabled) {
            if (IsValidFov(paniniFov))
                cameraFov = paniniFov;
        } else if (wasEnabled) {
            if (IsValidFov(paniniUserFov))
                cameraFov = paniniUserFov;
        }
        wasEnabled = enabled;
    }
};

TEST_F(RuntimeRobustnessTest, FovSwap_InitialEnable_SetsPaniniFov) {
    FovStateMachine sm;
    sm.tick(true);
    EXPECT_FLOAT_EQ(sm.cameraFov, sm.paniniFov);
    EXPECT_TRUE(sm.wasEnabled);
}

TEST_F(RuntimeRobustnessTest, FovSwap_Disable_RestoresUserFov) {
    FovStateMachine sm;
    sm.tick(true);
    EXPECT_FLOAT_EQ(sm.cameraFov, sm.paniniFov);
    sm.tick(false);
    EXPECT_FLOAT_EQ(sm.cameraFov, sm.paniniUserFov);
    EXPECT_FALSE(sm.wasEnabled);
}

TEST_F(RuntimeRobustnessTest, FovSwap_DoubleEnable_NoChange) {
    FovStateMachine sm;
    sm.tick(true);
    float fovAfterFirst = sm.cameraFov;
    sm.tick(true);
    EXPECT_FLOAT_EQ(sm.cameraFov, fovAfterFirst);
}

TEST_F(RuntimeRobustnessTest, FovSwap_DoubleDisable_NoOp) {
    FovStateMachine sm;
    sm.tick(false);
    EXPECT_FLOAT_EQ(sm.cameraFov, 1.5708f);
    sm.tick(false);
    EXPECT_FLOAT_EQ(sm.cameraFov, 1.5708f);
}

TEST_F(RuntimeRobustnessTest, FovSwap_EnableDisable_ToggleCycle) {
    FovStateMachine sm;
    for (int i = 0; i < 5; ++i) {
        sm.tick(true);
        EXPECT_FLOAT_EQ(sm.cameraFov, sm.paniniFov);
        sm.tick(false);
        EXPECT_FLOAT_EQ(sm.cameraFov, sm.paniniUserFov);
    }
}

TEST_F(RuntimeRobustnessTest, FovSwap_EnableWhileValuesChange) {
    FovStateMachine sm;
    sm.tick(true);
    EXPECT_FLOAT_EQ(sm.cameraFov, 2.6565f);

    // Simulate user changing paniniFov while enabled
    sm.paniniFov = 3.0f;
    sm.tick(true);
    EXPECT_FLOAT_EQ(sm.cameraFov, 3.0f);

    sm.tick(false);
    EXPECT_FLOAT_EQ(sm.cameraFov, sm.paniniUserFov);
}

TEST_F(RuntimeRobustnessTest, FovSwap_InvalidPaniniFov_NoChange) {
    FovStateMachine sm;
    sm.paniniFov = std::numeric_limits<float>::quiet_NaN();
    sm.tick(true);
    EXPECT_FLOAT_EQ(sm.cameraFov, 1.5708f);
}

TEST_F(RuntimeRobustnessTest, FovSwap_InvalidUserFov_NoRestore) {
    FovStateMachine sm;
    sm.paniniUserFov = -1.0f;
    sm.tick(true);
    EXPECT_FLOAT_EQ(sm.cameraFov, sm.paniniFov);
    sm.tick(false);
    // Should NOT restore because paniniUserFov is invalid
    EXPECT_FLOAT_EQ(sm.cameraFov, sm.paniniFov);
}

// ---------------------------------------------------------------------------
// State consistency: all clamped configs produce valid shader parameters
// ---------------------------------------------------------------------------

TEST_F(RuntimeRobustnessTest, ClampedConfigs_ProduceValidShaderParams) {
    // After clamping, compute zoom for each config and verify it passes
    // the shader safety check (no NaN, no inf, in reasonable range).
    float rawStrengths[] = {
        -1.0f, 0.0f, 0.03f, 0.5f, 1.0f, 2.0f,
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity()
    };
    float rawFills[] = {
        -0.5f, 0.0f, 0.2f, 0.8f, 1.0f, 5.0f,
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity()
    };
    float halfTans[] = {
        tanf(0.1f / 2.0f),
        tanf(1.5708f / 2.0f),
        tanf(2.6565f / 2.0f),
        tanf(3.09f / 2.0f)
    };

    for (float rawS : rawStrengths) {
        for (float rawF : rawFills) {
            auto c = clampConfig(rawS, 0.0f, rawF);
            for (float halfTan : halfTans) {
                float zoom = ComputeFillZoom(c.strength, halfTan, kDefaultAspect, c.fill);
                float finalZoom = safeZoom(zoom);
                EXPECT_FALSE(std::isnan(finalZoom))
                    << "rawS=" << rawS << " rawF=" << rawF << " halfTan=" << halfTan;
                EXPECT_FALSE(std::isinf(finalZoom))
                    << "rawS=" << rawS << " rawF=" << rawF << " halfTan=" << halfTan;
                EXPECT_GT(finalZoom, 0.0f)
                    << "rawS=" << rawS << " rawF=" << rawF << " halfTan=" << halfTan;
            }
        }
    }
}

TEST_F(RuntimeRobustnessTest, ClampedConfigs_VerticalCompAlwaysValid) {
    float rawVerts[] = {
        -5.0f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 3.0f,
        std::numeric_limits<float>::quiet_NaN()
    };

    for (float rawV : rawVerts) {
        auto c = clampConfig(0.03f, rawV, 0.8f);
        EXPECT_GE(c.verticalComp, -1.0f) << "rawV=" << rawV;
        EXPECT_LE(c.verticalComp, 1.0f) << "rawV=" << rawV;
        EXPECT_EQ(c.verticalComp, c.verticalComp) << "rawV=" << rawV; // not NaN
    }
}
