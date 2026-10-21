#include <gtest/gtest.h>
#include <cstddef>
#include <cstring>
#include <cmath>
#include <cfenv>
#include <cfloat>
#include <climits>
#include "VxMath.h"
#include "VxSIMDMode.h"
#include <SDL3/SDL_thread.h>

static void ExpectColor(const VxColor &c, float r, float g, float b, float a) {
    EXPECT_FLOAT_EQ(c.r, r); EXPECT_FLOAT_EQ(c.g, g); EXPECT_FLOAT_EQ(c.b, b); EXPECT_FLOAT_EQ(c.a, a);
}
static float ColorFromBits(unsigned bits) { float v; std::memcpy(&v, &bits, 4); return v; }
static unsigned ColorBits(float v) { unsigned bits; std::memcpy(&bits, &v, 4); return bits; }

TEST(ColorReferenceTest, LayoutAndDefaultConstructionMatchTheOriginalRGBAArray) {
    EXPECT_EQ(sizeof(VxColor), 16u); EXPECT_EQ(alignof(VxColor), 4u);
    EXPECT_EQ(offsetof(VxColor, r), 0u); EXPECT_EQ(offsetof(VxColor, g), 4u);
    EXPECT_EQ(offsetof(VxColor, b), 8u); EXPECT_EQ(offsetof(VxColor, a), 12u); EXPECT_EQ(offsetof(VxColor, col), 0u);
    VxColor c; for (int i = 0; i < 4; ++i) EXPECT_EQ(ColorBits(c.col[i]), 0u);
    c.col[0] = 0.25f; c.col[1] = -0.5f; c.col[2] = 2.0f; c.col[3] = 4.0f;
    ExpectColor(c, 0.25f, -0.5f, 2.0f, 4.0f);
}

TEST(ColorReferenceTest, FloatConstructorsAndSettersKeepUnclampedComponentsAndDefaultAlpha) {
    ExpectColor(VxColor(-0.5f, 0.25f, 2.0f, 4.0f), -0.5f, 0.25f, 2.0f, 4.0f);
    ExpectColor(VxColor(-0.5f, 0.25f, 2.0f), -0.5f, 0.25f, 2.0f, 1.0f);
    ExpectColor(VxColor(0.25f), 0.25f, 0.25f, 0.25f, 1.0f);
    VxColor c; c.Set(-0.5f, 0.25f, 2.0f, 4.0f); ExpectColor(c, -0.5f, 0.25f, 2.0f, 4.0f);
    c.Set(-0.5f, 0.25f, 2.0f); ExpectColor(c, -0.5f, 0.25f, 2.0f, 1.0f);
    c.Set(0.25f); ExpectColor(c, 0.25f, 0.25f, 0.25f, 1.0f);
}

TEST(ColorReferenceTest, IntegerConstructorsAndSettersNormalizeWithoutImplicitClamping) {
    for (int code = 0; code < 256; ++code) {
        float r = static_cast<float>(static_cast<double>(code) / 255.0);
        float g = static_cast<float>(static_cast<double>(255 - code) / 255.0);
        VxColor c(code, 255 - code, 255, 0); ExpectColor(c, r, g, 1.0f, 0.0f);
        ExpectColor(VxColor(code, 255 - code, 0), r, g, 0.0f, 1.0f);
        c.Set(code, 255 - code, 255, 0); ExpectColor(c, r, g, 1.0f, 0.0f);
        c.Set(code, 255 - code, 0); ExpectColor(c, r, g, 0.0f, 1.0f);
    }
    ExpectColor(VxColor(-255, 510, 0, 255), -1.0f, 2.0f, 0.0f, 1.0f);
    VxColor c; c.Set(-255, 510, 0, 255); ExpectColor(c, -1.0f, 2.0f, 0.0f, 1.0f);
}

TEST(ColorReferenceTest, PackedConstructorsAndSettersRoundTripEveryByteInEveryChannel) {
    for (int channel = 0; channel < 4; ++channel) for (unsigned code = 0; code < 256; ++code) {
        const unsigned shift = channel * 8;
        XDWORD packed = (0x12345678u & ~(0xffu << shift)) | (code << shift);
        VxColor constructed(packed), set; set.Set(packed);
        for (int i = 0; i < 4; ++i) {
            unsigned sourceShift = i == 3 ? 24 : (2 - i) * 8;
            float value = static_cast<float>(static_cast<double>((packed >> sourceShift) & 255) / 255.0);
            EXPECT_FLOAT_EQ(constructed.col[i], value); EXPECT_FLOAT_EQ(set.col[i], value);
        }
        EXPECT_EQ(constructed.GetRGBA(), packed); EXPECT_EQ(set.GetRGBA(), packed);
        EXPECT_EQ(constructed.GetRGB(), packed | 0xff000000); EXPECT_EQ(set.GetRGB(), packed | 0xff000000);
    }
}

TEST(ColorReferenceTest, ClearResetsAllChannelsIncludingAlphaToPositiveZero) {
    VxColor c(-0.0f, 0.5f, 2.0f, -4.0f); c.Clear();
    for (int i = 0; i < 4; ++i) EXPECT_EQ(ColorBits(c.col[i]), 0u);
}

TEST(ColorReferenceTest, CheckClampsEveryChannelAndRetainsIEEEComparisonSemantics) {
    VxColor c(-0.5f, 0.25f, 2.0f, 4.0f); c.Check(); ExpectColor(c, 0.0f, 0.25f, 1.0f, 1.0f);
    const float nan = ColorFromBits(0x7fc12345), inf = ColorFromBits(0x7f800000);
    c.Set(nan, -0.0f, inf, -inf); c.Check();
    EXPECT_EQ(ColorBits(c.r), 0x7fc12345u); EXPECT_EQ(ColorBits(c.g), 0x80000000u);
    EXPECT_EQ(c.b, 1.0f); EXPECT_EQ(c.a, 0.0f);
}

TEST(ColorReferenceTest, FreeArithmeticOperatorsIncludeAlphaAndDoNotClamp) {
    const VxColor a(-0.5f, 0.25f, 2.0f, 4.0f), b(2.0f, -0.5f, 0.25f, 8.0f);
    ExpectColor(a + b, 1.5f, -0.25f, 2.25f, 12.0f);
    ExpectColor(a - b, -2.5f, 0.75f, 1.75f, -4.0f);
    ExpectColor(a * b, -1.0f, -0.125f, 0.5f, 32.0f);
    ExpectColor(a / b, -0.25f, -0.5f, 8.0f, 0.5f);
    ExpectColor(a * 2.0f, -1.0f, 0.5f, 4.0f, 8.0f);
    ExpectColor(a, -0.5f, 0.25f, 2.0f, 4.0f); ExpectColor(b, 2.0f, -0.5f, 0.25f, 8.0f);
}

TEST(ColorReferenceTest, AssignmentOperatorsReturnTheSameObjectAndUseAllFourComponents) {
    const VxColor a(-0.5f, 0.25f, 2.0f, 4.0f), b(2.0f, -0.5f, 0.25f, 8.0f);
    VxColor c = a; EXPECT_EQ(&(c += b), &c); ExpectColor(c, 1.5f, -0.25f, 2.25f, 12.0f);
    c = a; EXPECT_EQ(&(c -= b), &c); ExpectColor(c, -2.5f, 0.75f, 1.75f, -4.0f);
    c = a; EXPECT_EQ(&(c *= b), &c); ExpectColor(c, -1.0f, -0.125f, 0.5f, 32.0f);
    c = a; EXPECT_EQ(&(c /= b), &c); ExpectColor(c, -0.25f, -0.5f, 8.0f, 0.5f);
    c = a; EXPECT_EQ(&(c *= 2.0f), &c); ExpectColor(c, -1.0f, 0.5f, 4.0f, 8.0f);
    c = a; EXPECT_EQ(&(c /= 0.5f), &c); ExpectColor(c, -1.0f, 0.5f, 4.0f, 8.0f);
}

TEST(ColorReferenceTest, AssignmentOperatorsAllowTheSameColorAsBothOperands) {
    const VxColor a(-0.5f, 0.25f, 2.0f, 4.0f);
    VxColor c = a; c += c; ExpectColor(c, -1.0f, 0.5f, 4.0f, 8.0f);
    c = a; c -= c; ExpectColor(c, 0.0f, 0.0f, 0.0f, 0.0f);
    c = a; c *= c; ExpectColor(c, 0.25f, 0.0625f, 4.0f, 16.0f);
    c = a; c /= c; ExpectColor(c, 1.0f, 1.0f, 1.0f, 1.0f);
}

TEST(ColorReferenceTest, DivisionKeepsIEEEInfinityAndNaNInsteadOfInventingFallbackColors) {
    VxColor c(1.0f, -1.0f, 0.0f, 2.0f); c /= 0.0f;
    EXPECT_TRUE(std::isinf(c.r)); EXPECT_FALSE(std::signbit(c.r));
    EXPECT_TRUE(std::isinf(c.g)); EXPECT_TRUE(std::signbit(c.g)); EXPECT_TRUE(std::isnan(c.b));
    EXPECT_TRUE(std::isinf(c.a));
    VxColor d = VxColor(1.0f, -1.0f, 0.0f, 2.0f) / VxColor(0.0f, 0.0f, 0.0f, 0.0f);
    EXPECT_TRUE(std::isinf(d.r)); EXPECT_TRUE(std::isinf(d.g)); EXPECT_TRUE(std::isnan(d.b)); EXPECT_TRUE(std::isinf(d.a));
}

TEST(ColorReferenceTest, EqualityIsExactNumericComparisonIncludingAlphaNotBitwiseEquality) {
    VxColor positive(0.0f), negative(-0.0f);
    EXPECT_TRUE(positive == negative); EXPECT_FALSE(positive != negative);
    VxColor a(0.25f, 0.5f, 0.75f, 1.0f), b = a;
    for (int channel = 0; channel < 4; ++channel) {
        b = a; b.col[channel] = std::nextafter(b.col[channel], FLT_MAX);
        EXPECT_FALSE(a == b); EXPECT_TRUE(a != b); EXPECT_FALSE(b == a); EXPECT_TRUE(b != a);
    }
    a.r = ColorFromBits(0x7fc12345); EXPECT_FALSE(a == a); EXPECT_TRUE(a != a);
}

TEST(ColorReferenceTest, SquareDistanceUsesRGBOnlyAndIsSymmetric) {
    VxColor a(-0.5f, 0.25f, 2.0f, 4.0f), b(2.0f, -0.5f, 0.25f, 8.0f);
    EXPECT_EQ(a.GetSquareDistance(b), 9.875f); EXPECT_EQ(b.GetSquareDistance(a), 9.875f);
    b.Set(a.r, a.g, a.b, -1000.0f); EXPECT_EQ(a.GetSquareDistance(b), 0.0f);
}

TEST(ColorReferenceTest, IntegerConvertClampsAndKeepsARGBPackingAndDefaultAlpha) {
    EXPECT_EQ(VxColor::Convert(-1, 256, 128, 300), 0xff00ff80u);
    EXPECT_EQ(VxColor::Convert(INT_MIN, INT_MAX, 255, 0), 0x0000ffffu);
    EXPECT_EQ(VxColor::Convert(1, 2, 3), 0xff010203u);
    EXPECT_EQ(VxColor::Convert(255, 255, 255, 255), 0xffffffffu);
}

TEST(ColorReferenceTest, FloatPackingSaturatesFiniteOutOfRangeValuesAndTruncatesAllChannelsUniformly) {
    const VxColor a(0.25f, 0.5f, 0.75f, 1.0f), half(0.5f, 0.5f, 0.5f, 0.5f);
    EXPECT_EQ(RGBAFTOCOLOR(a.r, a.g, a.b, a.a), 0xff3f7fbfu); EXPECT_EQ(RGBAFTOCOLOR(&a), 0xff3f7fbfu);
    EXPECT_EQ(RGBAFTOCOLOR(half.r, half.g, half.b, half.a), 0x7f7f7f7fu); EXPECT_EQ(half.GetRGBA(), 0x7f7f7f7fu);
    EXPECT_EQ(half.GetRGB(), 0xff7f7f7fu);
    EXPECT_EQ(RGBAFTOCOLOR(-0.25f, 0.5f, 2.0f, 1.0f), 0xff007fffu);
    VxColor huge(FLT_MAX, 1.0f, -FLT_MAX, 1.0f);
    EXPECT_EQ(RGBAFTOCOLOR(&huge), 0xffffff00u); EXPECT_EQ(RGBAFTOCOLOR(huge.r, huge.g, huge.b, huge.a), 0xffffff00u);
    EXPECT_EQ(VxColor::Convert(1.5f, 0.5f, -1.0f), 0xffff7f00u);
    EXPECT_EQ(VxColor::Convert(0.25f, 0.5f, 0.75f, 0.125f), 0x1f3f7fbfu);
}

struct ColorRoundingRestore {
    int mode;
    ColorRoundingRestore() : mode(std::fegetround()) {}
    ~ColorRoundingRestore() { std::fesetround(mode); }
};

TEST(ColorReferenceTest, ExactDyadicPackingDoesNotSwitchIntegerRoundingBetweenOverloads) {
    ColorRoundingRestore restore;
    const int modes[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
    for (int i = 0; i < 4; ++i) {
        ASSERT_EQ(std::fesetround(modes[i]), 0);
        volatile float input = 0.5f;
        const VxColor half(input, input, input, input);
        EXPECT_EQ(RGBAFTOCOLOR(&half), 0x7f7f7f7fu); EXPECT_EQ(RGBAFTOCOLOR(half.r, half.g, half.b, half.a), 0x7f7f7f7fu);
        EXPECT_EQ(VxColor::Convert(0.25f, 0.5f, 0.75f, 1.0f), 0xff3f7fbfu);
    }
}

struct ColorWorker { int seed; };
static int SDLCALL PackColorsConcurrently(void *data) {
    const ColorWorker *worker = static_cast<ColorWorker *>(data);
    for (int call = 0; call < 4000; ++call) {
        unsigned r = (call + worker->seed) & 255, g = (call * 17 + worker->seed) & 255;
        VxColor c(r / 256.0f, g / 256.0f, 0.5f, 1.0f);
        XDWORD expected = 0xff00007f | ((r ? r - 1 : 0) << 16) | ((g ? g - 1 : 0) << 8);
        if (RGBAFTOCOLOR(&c) != expected || RGBAFTOCOLOR(c.r, c.g, c.b, c.a) != expected) return 1;
    }
    return 0;
}

TEST(ColorReferenceTest, PackingCallsKeepIndependentLocalStateAcrossThreads) {
    ColorWorker work[3] = {{0}, {31}, {179}}; SDL_Thread *threads[3] = {};
    for (int i = 0; i < 3; ++i) threads[i] = SDL_CreateThread(PackColorsConcurrently, "color-pack", &work[i]);
    for (int i = 0; i < 3; ++i) {
        EXPECT_NE(threads[i], nullptr);
        if (threads[i]) { int result = -1; SDL_WaitThread(threads[i], &result); EXPECT_EQ(result, 0); }
    }
}

class ColorRuntimeReferenceTest : public testing::TestWithParam<int> {
protected:
    int previous;
    void SetUp() override {
        const int modes[] = {VX_SIMD_MODE_NONE, VX_SIMD_MODE_SSE2, VX_SIMD_MODE_SSSE3, VX_SIMD_MODE_AVX2};
        previous = VxGetSIMDOverride(); ASSERT_TRUE(VxSetSIMDOverride(modes[GetParam()]));
        ASSERT_EQ(VxGetSIMDEffectiveBackend(), modes[GetParam()]);
    }
    void TearDown() override { EXPECT_TRUE(VxSetSIMDOverride(previous)); }
};

TEST_P(ColorRuntimeReferenceTest, AllByteBinsUseTheSameTruncationAcrossPackingEntryPoints) {
    for (unsigned n = 0; n < 256; ++n) {
        unsigned codes[] = {n, (n * 17) & 255, (n * 37) & 255, (n * 71) & 255};
        VxColor c(codes[0] / 256.0f, codes[1] / 256.0f, codes[2] / 256.0f, codes[3] / 256.0f);
        unsigned bytes[4]; for (int i = 0; i < 4; ++i) bytes[i] = codes[i] ? codes[i] - 1 : 0;
        XDWORD expected = (bytes[3] << 24) | (bytes[0] << 16) | (bytes[1] << 8) | bytes[2];
        EXPECT_EQ(RGBAFTOCOLOR(c.r, c.g, c.b, c.a), expected); EXPECT_EQ(RGBAFTOCOLOR(&c), expected);
        EXPECT_EQ(c.GetRGBA(), expected); EXPECT_EQ(c.GetRGB(), expected | 0xff000000);
        EXPECT_EQ(VxColor::Convert(c.r, c.g, c.b, c.a), expected);
    }
}

TEST_P(ColorRuntimeReferenceTest, PackedColorsFeedTheRuntimeBlitterInTheOriginalChannelOrder) {
    const VxColor colors[] = {VxColor(0.25f, 0.5f, 0.75f, 0.125f), VxColor(1.0f, 0.0f, 0.5f, 1.0f)};
    XDWORD packed[] = {colors[0].GetRGBA(), colors[1].GetRGBA()}; XBYTE output[8]; std::memset(output, 0xa5, sizeof(output));
    VxImageDescEx src, dst; src.Width = dst.Width = 2; src.Height = dst.Height = 1;
    src.BitsPerPixel = 32; src.BytesPerLine = 8; src.Image = reinterpret_cast<XBYTE *>(packed);
    src.RedMask = dst.RedMask = 0xff0000; src.GreenMask = dst.GreenMask = 0xff00; src.BlueMask = dst.BlueMask = 0xff;
    src.AlphaMask = 0xff000000; dst.BitsPerPixel = 24; dst.BytesPerLine = 8; dst.Image = output;
    VxDoBlit(src, dst); const XBYTE expected[] = {0xbf, 0x7f, 0x3f, 0x7f, 0x00, 0xff, 0xa5, 0xa5};
    EXPECT_EQ(std::memcmp(output, expected, sizeof(output)), 0);
}

INSTANTIATE_TEST_SUITE_P(DispatchModes, ColorRuntimeReferenceTest, testing::Range(0, 4));
