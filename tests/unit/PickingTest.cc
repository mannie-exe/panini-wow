#include <gtest/gtest.h>
#include "picking.h"
#include "panini_math.h"
#include <limits>
#include <utility>

namespace {
PickingProjection Projection(float d = 0.5f, float fov = 2.82f,
                             float aspect = 16.0f / 9.0f, float fill = 1.0f, float s = 0.0f) {
    float halfTan = tanf(fov * 0.5f);
    return {d, halfTan, ComputeFillZoom(d, halfTan, aspect, fill), s, aspect};
}
}

TEST(Picking, ReproducesVerticalOffsetAndMapsBackToSource) {
    // At display pixel (960,270), the pre-fix pick used source y=270.
    // Independent shader reproduction gives these source pixels instead.
    for (auto entry : {std::pair<float, float>{0.01f, 294.338f}, {0.5f, 477.722f}}) {
        auto p = Projection(entry.first);
        float x = 0.5f * 0.8f, y = 0.75f * 0.45f;
        ASSERT_TRUE(RemapPickingPoint(p, 0.8f, 0.45f, x, y));
        EXPECT_NEAR(x / 0.8f * 1920.0f, 960.0f, 0.001f);
        EXPECT_NEAR((1.0f - y / 0.45f) * 1080.0f, entry.second, 0.002f);
    }
}

TEST(Picking, SharedShaderMappingMatchesIndependentTrigonometricReference) {
    // Solve x=(D+1)*sin(longitude)/(D+cos(longitude)) with trig,
    // independently of the production shader's quadratic solution.
    for (float d : {0.001f, 0.01f, 0.25f, 0.5f, 1.0f})
    for (float fov : {0.8f, 1.57f, 2.82f, 3.0f})
    for (float aspect : {4.0f/3.0f, 16.0f/9.0f, 21.0f/9.0f})
    for (float fill : {0.0f, 0.5f, 1.0f})
    for (float s : {-1.0f, 0.0f, 1.0f}) {
        auto p = Projection(d, fov, aspect, fill, s);
        for (int ix = 0; ix <= 24; ++ix)
        for (int iy = 0; iy <= 16; ++iy) {
            double u = ix / 24.0, v = iy / 16.0;
            double px = (2*u-1) / p.zoom * p.halfTan * p.aspect;
            double py = (2*v-1) / p.zoom * p.halfTan;
            double theta = atan(px / (1.0 + d));
            double longitude = theta + asin(d * sin(theta));
            double cosine = cos(longitude);
            double rx = tan(longitude);
            double ry = py * (d + cosine) / ((1.0+d) * cosine);
            ry *= 1.0 + s * (1.0 / sqrt(1.0 + rx*rx / ((1.0+d)*(1.0+d))) - 1.0);
            double expectedU = rx / (p.halfTan*p.aspect) * 0.5 + 0.5;
            double expectedV = ry / p.halfTan * 0.5 + 0.5;
            float actualU, actualV;
            PaniniSourceUV(float(u), float(v), d, p.halfTan, p.zoom, s, aspect, actualU, actualV);
            // Only visible samples affect picking; singular rays are borders.
            if (expectedU > 0.0001 && expectedU < 0.9999 && expectedV > 0.0001 && expectedV < 0.9999) {
                ASSERT_NEAR(actualU, expectedU, 0.00003) << d << ',' << fov << ',' << fill;
                ASSERT_NEAR(actualV, expectedV, 0.00003);
            }
        }
    }
}

TEST(Picking, DisabledAndBelowShaderThresholdPreserveExactCoordinates) {
    for (float d : {0.0f, 0.000999f}) {
        auto p = Projection(d);
        p.zoom = 4.0f;
        float x = 0.123f, y = 0.234f;
        ASSERT_TRUE(RemapPickingPoint(p, 0.8f, 0.6f, x, y));
        EXPECT_FLOAT_EQ(x, 0.123f);
        EXPECT_FLOAT_EQ(y, 0.234f);
    }
}

TEST(Picking, BlackBordersReturnNoHitWithoutClamping) {
    auto p = Projection(0.5f, 1.57f, 16.0f/9.0f, 0.0f);
    float x = 0.8f, y = 0.225f;
    ASSERT_FALSE(RemapPickingPoint(p, 0.8f, 0.45f, x, y));
    EXPECT_FLOAT_EQ(x, 0.8f);
    EXPECT_FLOAT_EQ(y, 0.225f);
}

TEST(Picking, CenterIsStableAcrossSettings) {
    for (float d : {0.01f, 0.5f, 1.0f})
    for (float s : {-1.0f, 0.0f, 1.0f}) {
        float x = 0.4f, y = 0.3f;
        ASSERT_TRUE(RemapPickingPoint(Projection(d, 2.82f, 4.0f/3.0f, 1.0f, s), 0.8f, 0.6f, x, y));
        EXPECT_FLOAT_EQ(x, 0.4f);
        EXPECT_FLOAT_EQ(y, 0.3f);
    }
}

TEST(Picking, DeviceScaleAndResolutionDoNotChangeNormalizedResult) {
    auto p = Projection();
    float referenceX = 0.65f, referenceY = 0.3f;
    ASSERT_TRUE(RemapPickingPoint(p, 1.0f, 1.0f, referenceX, referenceY));
    for (auto size : {std::pair<float,float>{0.8f,0.45f}, {1920.0f,1080.0f}, {2560.0f,1440.0f}}) {
        float x = 0.65f*size.first, y = 0.3f*size.second;
        ASSERT_TRUE(RemapPickingPoint(p, size.first, size.second, x, y));
        EXPECT_NEAR(x/size.first, referenceX, 0.000001f);
        EXPECT_NEAR(y/size.second, referenceY, 0.000001f);
    }
}

TEST(Picking, RejectsInvalidCoordinatesAndExtents) {
    auto p = Projection();
    for (float bad : {0.0f, -1.0f, std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN()}) {
        float x = 0.4f, y = 0.3f;
        EXPECT_FALSE(RemapPickingPoint(p, bad, 0.6f, x, y));
        EXPECT_FALSE(RemapPickingPoint(p, 0.8f, bad, x, y));
    }
    for (float bad : {-1.0f, 2.0f, std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN()}) {
        float x = bad, y = 0.3f;
        EXPECT_FALSE(RemapPickingPoint(p, 0.8f, 0.6f, x, y));
        x = 0.4f; y = bad;
        EXPECT_FALSE(RemapPickingPoint(p, 0.8f, 0.6f, x, y));
    }
}

TEST(Picking, RejectsInvalidProjectionConstants) {
    for (int member = 0; member < 5; ++member) {
        auto p = Projection();
        float* fields[] = {&p.strength, &p.halfTan, &p.zoom, &p.verticalComp, &p.aspect};
        *fields[member] = std::numeric_limits<float>::quiet_NaN();
        float x = 0.4f, y = 0.3f;
        EXPECT_FALSE(RemapPickingPoint(p, 0.8f, 0.6f, x, y));
    }
}
