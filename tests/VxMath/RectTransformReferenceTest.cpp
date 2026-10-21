#include <cmath>
#include <cstring>
#include <cstdio>
#include <cfenv>
#include <cstddef>
#ifndef VX_RECT_NATIVE_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#else
#include "C:/Users/kakut/Works/Virtools/Virtools-SDK-2.1/Include/VxRect.h"
#endif

typedef void (*RectMapCall)(VxRect &, const VxRect &, const VxRect &);
typedef void (*RectSizeCall)(VxRect &, const Vx2DVector &, const Vx2DVector &);
typedef void (*RectScreenCall)(VxRect &, const VxRect &);
struct RectTransformCalls { RectMapCall Map; RectSizeCall Size; RectScreenCall To; RectScreenCall From; };
struct RectTransformSamples { int errors = 0, rectangles = 0; char bits[2050] = {}; };

static RectTransformSamples RectTransformDataset(const RectTransformCalls &calls, int operation) {
    RectTransformSamples samples;
    for (int n = 0; n < 64; ++n) {
        const float sl = float(n % 7 - 3) * 8, st = float(n % 5 - 2) * 16;
        const float sw = float(8 << (n % 4)), sh = float(16 << (n % 3));
        const float dl = float(n % 3) * 32 - 16, dt = float(n % 4) * -8;
        const float dw = float(16 << (n % 3)), dh = float(8 << (n % 4));
        const float nl = -0.25f + float(n % 3) * 0.125f, nt = 0.125f + float(n % 2) * 0.25f;
        const float nr = 1.25f + float(n % 2) * 0.125f, nb = 1.5f + float(n % 3) * 0.25f;
        const VxRect src(sl, st, sl + sw, st + sh), dst(dl, dt, dl + dw, dt + dh);
        VxRect rect(sl + nl * sw, st + nt * sh, sl + nr * sw, st + nb * sh);
        VxRect expected(dl + nl * dw, dt + nt * dh, dl + nr * dw, dt + nb * dh);
        if (operation == 0) { calls.Map(rect, dst, src); }
        else if (operation == 1) {
            const Vx2DVector destSize(dw, dh), sourceSize(sw, sh);
            expected = VxRect(rect.left / sw * dw, rect.top / sh * dh, rect.right / sw * dw, rect.bottom / sh * dh);
            calls.Size(rect, destSize, sourceSize);
        } else if (operation == 2) {
            expected = VxRect(nl, nt, nr, nb); calls.To(rect, src);
        } else {
            rect = VxRect(nl, nt, nr, nb); calls.From(rect, dst);
        }
        const float actual[] = {rect.left, rect.top, rect.right, rect.bottom};
        const float wanted[] = {expected.left, expected.top, expected.right, expected.bottom};
        for (int c = 0; c < 4; ++c) {
            if (actual[c] != wanted[c]) ++samples.errors;
            unsigned bits; std::memcpy(&bits, actual + c, sizeof(bits));
            std::snprintf(samples.bits + (n * 4 + c) * 8, 9, "%08x", bits);
        }
        ++samples.rectangles;
    }
    return samples;
}

#ifndef VX_RECT_NATIVE_PROBE
#if defined(_MSC_VER)
#define RECT_TEST_NOINLINE __declspec(noinline)
#else
#define RECT_TEST_NOINLINE __attribute__((noinline))
#endif
static RECT_TEST_NOINLINE void RectMap(VxRect &r, const VxRect &d, const VxRect &s) { r.Transform(d, s); }
static RECT_TEST_NOINLINE void RectSize(VxRect &r, const Vx2DVector &d, const Vx2DVector &s) { r.Transform(d, s); }
static RECT_TEST_NOINLINE void RectTo(VxRect &r, const VxRect &s) { r.TransformToHomogeneous(s); }
static RECT_TEST_NOINLINE void RectFrom(VxRect &r, const VxRect &s) { r.TransformFromHomogeneous(s); }
static const RectTransformCalls RectCalls = {RectMap, RectSize, RectTo, RectFrom};

static void ExpectRectTransform(const VxRect &r, float l, float t, float right, float b) {
    EXPECT_FLOAT_EQ(r.left, l); EXPECT_FLOAT_EQ(r.top, t); EXPECT_FLOAT_EQ(r.right, right); EXPECT_FLOAT_EQ(r.bottom, b);
}
static void CheckRectDataset(int operation) {
    const RectTransformSamples r = RectTransformDataset(RectCalls, operation);
    EXPECT_EQ(r.errors, 0); EXPECT_EQ(r.rectangles, 64);
    testing::Test::RecordProperty("rectangles", r.rectangles); testing::Test::RecordProperty("float_bits", r.bits);
#if defined(VX_SIMD_SSE)
    testing::Test::RecordProperty("header_backend", "SSE");
#else
    testing::Test::RecordProperty("header_backend", "Scalar");
#endif
}

TEST(RectTransformReferenceTest, BetweenScreensMapsEveryCornerIncludingOutsideCoordinates) { CheckRectDataset(0); }
TEST(RectTransformReferenceTest, SizeOnlyMappingScalesBothCornersAboutZero) { CheckRectDataset(1); }
TEST(RectTransformReferenceTest, ToHomogeneousUsesScreenOriginAndSize) { CheckRectDataset(2); }
TEST(RectTransformReferenceTest, FromHomogeneousUsesDestinationOriginAndSize) { CheckRectDataset(3); }

TEST(RectTransformReferenceTest, NonBinaryScreenRatiosAgreeWithIndependentGeometry) {
    const VxRect src(-17, 23, 196, 150), dst(31, -11, 430, 246);
    for (int n = 0; n < 37; ++n) {
        VxRect r(float(n - 20) * 0.75f, float(n + 30) * 1.25f, float(n + 70) * 0.75f, float(n + 90) * 1.25f);
        const double expected[] = {31.0 + (r.left + 17.0) * 399.0 / 213.0, -11.0 + (r.top - 23.0) * 257.0 / 127.0,
                                   31.0 + (r.right + 17.0) * 399.0 / 213.0, -11.0 + (r.bottom - 23.0) * 257.0 / 127.0};
        RectMap(r, dst, src); const float actual[] = {r.left, r.top, r.right, r.bottom};
        for (int c = 0; c < 4; ++c) EXPECT_NEAR(actual[c], expected[c], 0.00008);
    }
}

TEST(RectTransformReferenceTest, HomogeneousRoundTripKeepsDimensionsAndOffsets) {
    const VxRect screen(10, -20, 210, 380); VxRect r(-40, 30, 260, 430);
    RectTo(r, screen); ExpectRectTransform(r, -0.25f, 0.125f, 1.25f, 1.125f);
    RectFrom(r, screen); ExpectRectTransform(r, -40, 30, 260, 430);
}

TEST(RectTransformReferenceTest, PointMappingUsesTopForYAndSupportsInPlacePoint) {
    const VxRect screen(10, 20, 110, 220); Vx2DVector point(0.25f, 0.5f), output;
    screen.TransformFromHomogeneous(output, point); EXPECT_FLOAT_EQ(output.x, 35); EXPECT_FLOAT_EQ(output.y, 120);
    screen.TransformFromHomogeneous(point, point); EXPECT_FLOAT_EQ(point.x, 35); EXPECT_FLOAT_EQ(point.y, 120);
}

TEST(RectTransformReferenceTest, ScreenMayAliasTheRectangleForNormalization) {
    VxRect r(10, 20, 110, 220); RectTo(r, r); ExpectRectTransform(r, 0, 0, 1, 1);
}

TEST(RectTransformReferenceTest, SourceOrDestinationMayAliasTheRectangleForMapping) {
    const VxRect destination(30, 40, 430, 640), source(0, 0, 100, 100);
    VxRect r(10, 20, 110, 220); RectMap(r, destination, r); ExpectRectTransform(r, 30, 40, 430, 640);
    r = VxRect(10, 20, 60, 80); RectMap(r, r, source); ExpectRectTransform(r, 15, 32, 40, 68);
}

struct RectFloatEnvironment {
    std::fenv_t saved;
    RectFloatEnvironment() { std::fegetenv(&saved); }
    ~RectFloatEnvironment() { std::fesetenv(&saved); }
};

TEST(RectTransformReferenceTest, ValidScreenDoesNotRaiseDivideByZeroOrInvalidInUnusedLanes) {
    RectFloatEnvironment environment;
    volatile float values[] = {10, 20, 110, 220};
    const VxRect screen(values[0], values[1], values[2], values[3]); VxRect r(35, 70, 85, 170);
    std::feclearexcept(FE_ALL_EXCEPT); RectTo(r, screen);
    const int flags = std::fetestexcept(FE_INVALID | FE_DIVBYZERO);
    testing::Test::RecordProperty("severe_fp_flags", flags); EXPECT_EQ(flags, 0);
    ExpectRectTransform(r, 0.25f, 0.25f, 0.75f, 0.75f);
}

TEST(RectTransformReferenceTest, CompleteScreenAndAliasedScreenDoNotRaiseInvalid) {
    RectFloatEnvironment environment;
    volatile float values[] = {10, 20, 110, 220};
    const VxRect screen(values[0], values[1], values[2], values[3]); VxRect r = screen;
    std::feclearexcept(FE_ALL_EXCEPT); RectTo(r, screen);
    EXPECT_EQ(std::fetestexcept(FE_INVALID | FE_DIVBYZERO), 0); ExpectRectTransform(r, 0, 0, 1, 1);
    r = screen; std::feclearexcept(FE_ALL_EXCEPT); RectTo(r, r);
    EXPECT_EQ(std::fetestexcept(FE_INVALID | FE_DIVBYZERO), 0); ExpectRectTransform(r, 0, 0, 1, 1);
}

TEST(RectTransformReferenceTest, LayoutUsesFourFloatsWithBorrowedCornerReferences) {
    EXPECT_EQ(sizeof(VxRect), 16u); EXPECT_EQ(alignof(VxRect), 4u);
    EXPECT_EQ(offsetof(VxRect, left), 0u); EXPECT_EQ(offsetof(VxRect, top), 4u);
    EXPECT_EQ(offsetof(VxRect, right), 8u); EXPECT_EQ(offsetof(VxRect, bottom), 12u);
    VxRect r(1, 2, 3, 4); r.GetTopLeft().x = 5; r.GetBottomRight().y = 6;
    ExpectRectTransform(r, 5, 2, 3, 6);
}
#endif
