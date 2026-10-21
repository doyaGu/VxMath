#include <cmath>
#include <cstdio>
#include <cstring>
#ifndef VX_RECT_CLIP_NATIVE_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#else
#include "C:/Users/kakut/Works/Virtools/Virtools-SDK-2.1/Include/VxRect.h"
#endif

struct RectClipSamples {
    unsigned digest = 2166136261u;
    int cases = 0;
    int errors = 0;
};

static void RectClipHash(RectClipSamples &result, unsigned value) {
    for (int n = 0; n < 4; ++n) {
        result.digest = (result.digest ^ (value & 255u)) * 16777619u;
        value >>= 8;
    }
}

static void RectClipObserve(RectClipSamples &result, float actual, float expected) {
    if (actual != expected) ++result.errors;
    unsigned bits;
    std::memcpy(&bits, &actual, sizeof(bits));
    RectClipHash(result, bits);
}

// Ordered endpoint pairs on {-1, 0, 1, 2, 3}, tested against [0, 2].
// These literal interval classifications preserve the SDK's directional tangency.
static const int RectClipPairs[15][3] = {
    {0,0,ALLOUTSIDE}, {0,1,PARTINSIDE}, {0,2,PARTINSIDE},
    {0,3,PARTINSIDE}, {0,4,PARTINSIDE}, {1,1,ALLINSIDE},
    {1,2,ALLINSIDE}, {1,3,ALLINSIDE}, {1,4,PARTINSIDE},
    {2,2,ALLINSIDE}, {2,3,ALLINSIDE}, {2,4,PARTINSIDE},
    {3,3,ALLOUTSIDE}, {3,4,ALLOUTSIDE}, {4,4,ALLOUTSIDE}
};

static RectClipSamples RectClipMatrix() {
    RectClipSamples result;
    const float origins[] = {0, 10, -8, 3};
    const float steps[] = {1, 0.5f, 2, 0.25f};
    const int clipped[] = {0, 0, 1, 2, 2};
    for (int placement = 0; placement < 4; ++placement) {
        const float ox = origins[placement], oy = -ox - 4, step = steps[placement];
        const VxRect bounds(ox, oy, ox + 2 * step, oy + 2 * step);
        for (int x = 0; x < 15; ++x) for (int y = 0; y < 15; ++y) {
            const int *xp = RectClipPairs[x], *yp = RectClipPairs[y];
            const VxRect source(ox + (xp[0] - 1) * step, oy + (yp[0] - 1) * step,
                                ox + (xp[1] - 1) * step, oy + (yp[1] - 1) * step);
            const int classification = xp[2] == ALLOUTSIDE || yp[2] == ALLOUTSIDE ? ALLOUTSIDE :
                (xp[2] == ALLINSIDE && yp[2] == ALLINSIDE ? ALLINSIDE : PARTINSIDE);
            const int inside = source.IsInside(bounds), outside = source.IsOutside(bounds);
            VxRect actual = source;
            const int accepted = actual.Clip(bounds);
            if (inside != classification || outside != (classification == ALLOUTSIDE) ||
                accepted != (classification != ALLOUTSIDE)) ++result.errors;
            RectClipHash(result, unsigned(inside));
            RectClipHash(result, unsigned(outside));
            RectClipHash(result, unsigned(accepted));
            const VxRect expected = classification == ALLOUTSIDE ? source :
                VxRect(ox + clipped[xp[0]] * step, oy + clipped[yp[0]] * step,
                       ox + clipped[xp[1]] * step, oy + clipped[yp[1]] * step);
            RectClipObserve(result, actual.left, expected.left);
            RectClipObserve(result, actual.top, expected.top);
            RectClipObserve(result, actual.right, expected.right);
            RectClipObserve(result, actual.bottom, expected.bottom);
            if (bounds.left != ox || bounds.top != oy || bounds.right != ox + 2 * step ||
                bounds.bottom != oy + 2 * step) ++result.errors;
            ++result.cases;
        }
    }
    return result;
}

static RectClipSamples RectPointMatrix() {
    RectClipSamples result;
    const float origins[] = {0, 10, -8, 3};
    const float steps[] = {1, 0.5f, 2, 0.25f};
    const int modes[] = {0, 1, -7};
    for (int placement = 0; placement < 4; ++placement) {
        const float ox = origins[placement], oy = -ox - 4, step = steps[placement];
        const VxRect bounds(ox, oy, ox + 2 * step, oy + 2 * step);
        const float xs[] = {ox, ox, ox + step, bounds.right, bounds.right};
        const float ys[] = {oy, oy, oy + step, bounds.bottom, bounds.bottom};
        for (int x = 0; x < 5; ++x) for (int y = 0; y < 5; ++y) {
            const Vx2DVector source(ox + (x - 1) * step, oy + (y - 1) * step);
            const int inside = bounds.IsInside(source);
            if (inside != (x >= 1 && x <= 3 && y >= 1 && y <= 3)) ++result.errors;
            RectClipHash(result, unsigned(inside));
            for (int mode = 0; mode < 3; ++mode) {
                Vx2DVector point = source;
                bounds.Clip(point, modes[mode]);
                RectClipObserve(result, point.x, xs[x] - (modes[mode] && x >= 3 ? 1 : 0));
                RectClipObserve(result, point.y, ys[y] - (modes[mode] && y >= 3 ? 1 : 0));
                ++result.cases;
            }
        }
    }
    return result;
}

#ifndef VX_RECT_CLIP_NATIVE_PROBE
static void ExpectClippedRect(const VxRect &r, float left, float top, float right, float bottom) {
    EXPECT_FLOAT_EQ(r.left, left); EXPECT_FLOAT_EQ(r.top, top);
    EXPECT_FLOAT_EQ(r.right, right); EXPECT_FLOAT_EQ(r.bottom, bottom);
}

TEST(RectClipReferenceTest, ClassifiesContainedCrossingAndSeparatedRectangles) {
    const VxRect bounds(10, 20, 30, 40);
    EXPECT_EQ(VxRect(12, 23, 28, 37).IsInside(bounds), ALLINSIDE);
    EXPECT_EQ(bounds.IsInside(bounds), ALLINSIDE);
    EXPECT_EQ(VxRect(5, 15, 35, 45).IsInside(bounds), PARTINSIDE);
    EXPECT_EQ(VxRect(5, 25, 15, 35).IsInside(bounds), PARTINSIDE);
    EXPECT_EQ(VxRect(31, 25, 35, 35).IsInside(bounds), ALLOUTSIDE);
    EXPECT_TRUE(VxRect(31, 25, 35, 35).IsOutside(bounds));
    EXPECT_FALSE(VxRect(5, 25, 15, 35).IsOutside(bounds));
}

TEST(RectClipReferenceTest, TangencyKeepsTheOriginalDirectionalClassification) {
    const VxRect bounds(10, 20, 30, 40);
    const VxRect touching[] = {VxRect(5,25,10,35), VxRect(30,25,35,35),
        VxRect(15,15,25,20), VxRect(15,40,25,45), VxRect(5,15,10,20), VxRect(30,40,35,45)};
    const int expected[] = {PARTINSIDE, ALLOUTSIDE, PARTINSIDE, ALLOUTSIDE, PARTINSIDE, ALLOUTSIDE};
    for (int n = 0; n < 6; ++n) {
        SCOPED_TRACE(n);
        EXPECT_EQ(touching[n].IsInside(bounds), expected[n]);
        EXPECT_EQ(touching[n].IsOutside(bounds), expected[n] == ALLOUTSIDE);
    }
    EXPECT_EQ(bounds.IsInside(touching[0]), ALLOUTSIDE);
    EXPECT_EQ(bounds.IsInside(touching[1]), PARTINSIDE);
}

TEST(RectClipReferenceTest, ClipIntersectsAllFourEdgesAndPreservesTheClippingInput) {
    const VxRect bounds(10, 20, 30, 40);
    VxRect source(5, 15, 35, 45);
    EXPECT_TRUE(source.Clip(bounds));
    ExpectClippedRect(source, 10, 20, 30, 40);
    ExpectClippedRect(bounds, 10, 20, 30, 40);
    EXPECT_TRUE(source.Clip(bounds));
    ExpectClippedRect(source, 10, 20, 30, 40);
}

TEST(RectClipReferenceTest, RejectedClipLeavesAllCoordinatesUnchanged) {
    const VxRect bounds(10, 20, 30, 40);
    const VxRect inputs[] = {VxRect(1,25,9,35), VxRect(30,25,35,35),
        VxRect(15,1,25,19), VxRect(15,40,25,45)};
    for (int n = 0; n < 4; ++n) {
        VxRect actual = inputs[n];
        EXPECT_FALSE(actual.Clip(bounds));
        EXPECT_EQ(std::memcmp(&actual, inputs + n, sizeof(actual)), 0);
    }
}

TEST(RectClipReferenceTest, AcceptedLeftAndTopTangenciesProduceZeroAreaResults) {
    const VxRect bounds(10, 20, 30, 40);
    VxRect left(5,25,10,35), top(15,15,25,20), corner(5,15,10,20);
    EXPECT_TRUE(left.Clip(bounds)); ExpectClippedRect(left, 10,25,10,35);
    EXPECT_TRUE(top.Clip(bounds)); ExpectClippedRect(top, 15,20,25,20);
    EXPECT_TRUE(corner.Clip(bounds)); ExpectClippedRect(corner, 10,20,10,20);
}

TEST(RectClipReferenceTest, ZeroAreaClassificationDependsOnItsPosition) {
    const VxRect bounds(10, 20, 30, 40);
    EXPECT_EQ(VxRect(15,25,15,35).IsInside(bounds), ALLINSIDE);
    EXPECT_EQ(VxRect(10,20,10,20).IsInside(bounds), ALLINSIDE);
    EXPECT_EQ(VxRect(30,25,30,35).IsInside(bounds), ALLOUTSIDE);
    EXPECT_EQ(VxRect(15,40,25,40).IsInside(bounds), ALLOUTSIDE);
    VxRect empty(15,25,15,35);
    EXPECT_TRUE(empty.Clip(bounds)); ExpectClippedRect(empty, 15,25,15,35);
}

TEST(RectClipReferenceTest, SelfClipPreservesANonemptyRectangle) {
    VxRect r(-3.5f, 2.25f, 6.5f, 8.75f);
    EXPECT_TRUE(r.Clip(r));
    ExpectClippedRect(r, -3.5f, 2.25f, 6.5f, 8.75f);
}

TEST(RectClipReferenceTest, PointContainmentIncludesEveryBorderAndCorner) {
    const VxRect bounds(10, 20, 30, 40);
    const Vx2DVector points[] = {Vx2DVector(10,20), Vx2DVector(30,20),
        Vx2DVector(10,40), Vx2DVector(30,40), Vx2DVector(10,30), Vx2DVector(30,30),
        Vx2DVector(20,20), Vx2DVector(20,40), Vx2DVector(20,30)};
    for (int n = 0; n < 9; ++n) EXPECT_TRUE(bounds.IsInside(points[n]));
}

TEST(RectClipReferenceTest, PointContainmentRejectsTheNearestExteriorFloats) {
    const VxRect bounds(10, 20, 30, 40);
    EXPECT_FALSE(bounds.IsInside(Vx2DVector(std::nextafter(10.0f, 0.0f), 30.0f)));
    EXPECT_FALSE(bounds.IsInside(Vx2DVector(std::nextafter(30.0f, 50.0f), 30.0f)));
    EXPECT_FALSE(bounds.IsInside(Vx2DVector(20.0f, std::nextafter(20.0f, 0.0f))));
    EXPECT_FALSE(bounds.IsInside(Vx2DVector(20.0f, std::nextafter(40.0f, 50.0f))));
}

TEST(RectClipReferenceTest, DefaultPointClipExcludesRightAndBottomByOneCoordinateUnit) {
    const VxRect bounds(10, 20, 30, 40);
    Vx2DVector edge(30,40), outside(80,90), lower(1,2);
    bounds.Clip(edge); bounds.Clip(outside); bounds.Clip(lower);
    EXPECT_FLOAT_EQ(edge.x,29); EXPECT_FLOAT_EQ(edge.y,39);
    EXPECT_FLOAT_EQ(outside.x,29); EXPECT_FLOAT_EQ(outside.y,39);
    EXPECT_FLOAT_EQ(lower.x,10); EXPECT_FLOAT_EQ(lower.y,20);
}

TEST(RectClipReferenceTest, FalseFlagAllowsRightBottomAndNonzeroFlagsExcludeThem) {
    const VxRect bounds(10, 20, 30, 40);
    Vx2DVector inclusive(80,90), nonzero(80,90);
    bounds.Clip(inclusive, FALSE); bounds.Clip(nonzero, -7);
    EXPECT_FLOAT_EQ(inclusive.x,30); EXPECT_FLOAT_EQ(inclusive.y,40);
    EXPECT_FLOAT_EQ(nonzero.x,29); EXPECT_FLOAT_EQ(nonzero.y,39);
}

TEST(RectClipReferenceTest, FractionalSpanStillUsesOneUnitRatherThanOnePixelFraction) {
    const VxRect bounds(10, 20, 10.5f, 20.25f);
    Vx2DVector outside(11,21);
    bounds.Clip(outside);
    EXPECT_FLOAT_EQ(outside.x,9.5f); EXPECT_FLOAT_EQ(outside.y,19.25f);
    bounds.Clip(outside, FALSE);
    EXPECT_FLOAT_EQ(outside.x,10); EXPECT_FLOAT_EQ(outside.y,20);
}

TEST(RectClipReferenceTest, InteriorFractionalPointsAreNotRoundedOrMoved) {
    const VxRect bounds(10, 20, 30, 40);
    Vx2DVector point(29.75f,39.875f);
    bounds.Clip(point);
    EXPECT_FLOAT_EQ(point.x,29.75f); EXPECT_FLOAT_EQ(point.y,39.875f);
}

TEST(RectClipReferenceTest, RectangleBoundaryMatrixMatchesOriginalSdkAndIntervalTable) {
    const RectClipSamples result = RectClipMatrix();
    EXPECT_EQ(result.cases,900); EXPECT_EQ(result.errors,0);
    char digest[16]; std::snprintf(digest,sizeof(digest),"%u",result.digest);
    RecordProperty("samples",result.cases); RecordProperty("digest",digest);
}

TEST(RectClipReferenceTest, PointBoundaryMatrixMatchesOriginalSdkAndLiteralClampResults) {
    const RectClipSamples result = RectPointMatrix();
    EXPECT_EQ(result.cases,300); EXPECT_EQ(result.errors,0);
    char digest[16]; std::snprintf(digest,sizeof(digest),"%u",result.digest);
    RecordProperty("samples",result.cases); RecordProperty("digest",digest);
}

TEST(RectClipReferenceTest, DefaultClipProducesUsableIntegerRasterCoordinates) {
    const VxRect raster(5, 7, 9, 10);
    const unsigned pixels[12] = {11,12,13,14,21,22,23,24,31,32,33,34};
    const Vx2DVector input[] = {Vx2DVector(0,0), Vx2DVector(9,10), Vx2DVector(8,9),
        Vx2DVector(6,8), Vx2DVector(100,7), Vx2DVector(5,100)};
    const unsigned expected[] = {11,34,34,22,14,31};
    for (int n = 0; n < 6; ++n) {
        Vx2DVector point = input[n]; raster.Clip(point);
        const int x = int(point.x) - 5, y = int(point.y) - 7;
        ASSERT_GE(x,0); ASSERT_LT(x,4); ASSERT_GE(y,0); ASSERT_LT(y,3);
        EXPECT_EQ(pixels[y * 4 + x],expected[n]);
    }
}
#endif
