#include "VxQuaternion.h"
#include <gtest/gtest.h>
#include <fenv.h>
#include <math.h>
#if defined(VX_SIMD_X86)
#include <xmmintrin.h>
#endif

class QuaternionRangeRegressionTest : public ::testing::Test {
    fenv_t Saved;
protected:
    void SetUp() override {
        fegetenv(&Saved);
        fesetround(FE_TONEAREST);
#if defined(VX_SIMD_X86)
        _mm_setcsr(_mm_getcsr() & ~0x8040u);
#endif
    }
    void TearDown() override { fesetenv(&Saved); }
    void ExpectProduct(const VxQuaternion &result, float expected) {
        EXPECT_EQ(expected, result.x);
        EXPECT_EQ(expected, result.y);
        EXPECT_EQ(expected, result.z);
        EXPECT_EQ(-expected, result.w);
    }
    void Product(float input) {
        volatile float stored = input;
        const float a = stored;
        const float expected = static_cast<float>(2.0 * double(a) * a);
        const VxQuaternion q(a, a, a, a);
        const VxQuaternion result = Vx3DQuaternionMultiply(q, q);
        ExpectProduct(result, expected);
        ExpectProduct(q * q, expected);
        VxQuaternion modified(q);
        modified.Multiply(modified);
        ExpectProduct(modified, expected);
#if defined(VX_SIMD_SSE)
        VxQuaternion direct;
        VxSIMDMultiplyQuaternion(&direct, &q, &q);
        ExpectProduct(direct, expected);
#endif
        printf("product a=%a actual=%a,%a,%a,%a expected=%a\n", a, result.x, result.y, result.z, result.w, expected);
    }
    void Quotient(float input) {
        volatile float stored = input;
        const float a = stored;
        const float expected = static_cast<float>(2.0 * double(a) * a);
        const VxQuaternion p(expected, expected, expected, -expected), q(0.5f, 0.5f, 0.5f, 0.5f);
        const VxQuaternion result = Vx3DQuaternionDivide(p, q);
        EXPECT_EQ(expected, result.x);
        EXPECT_EQ(expected, result.y);
        EXPECT_EQ(expected, result.z);
        EXPECT_EQ(expected, result.w);
        const VxQuaternion operated = p / q;
        for (int i = 0; i < 4; ++i) EXPECT_EQ(expected, operated[i]);
#if defined(VX_SIMD_SSE)
        VxQuaternion direct;
        VxSIMDDivideQuaternion(&direct, &p, &q);
        EXPECT_EQ(expected, direct.x);
        EXPECT_EQ(expected, direct.y);
        EXPECT_EQ(expected, direct.z);
        EXPECT_EQ(expected, direct.w);
#endif
        printf("divide a=%a actual=%a,%a,%a,%a expected=%a\n", a, result.x, result.y, result.z, result.w, expected);
    }
};

TEST_F(QuaternionRangeRegressionTest, RepresentableLargeProduct) { Product(0x1.6p63f); }
TEST_F(QuaternionRangeRegressionTest, RepresentableSmallProduct) { Product(0x1p-75f); }
TEST_F(QuaternionRangeRegressionTest, RepresentableLargeQuotient) { Quotient(0x1.6p63f); }
TEST_F(QuaternionRangeRegressionTest, RepresentableSmallQuotient) { Quotient(0x1p-75f); }
TEST_F(QuaternionRangeRegressionTest, OrdinaryProductControl) { Product(0.25f); }
TEST_F(QuaternionRangeRegressionTest, OrdinaryQuotientControl) { Quotient(0.25f); }

TEST_F(QuaternionRangeRegressionTest, MixedRangesPreserveProductOrderAndAliasedOutputs) {
    const int exponents[][2] = {{100, -100}, {-100, 100}, {60, 60}, {-70, -70}, {-75, -75}, {0, 0}};
    for (const auto &exponent : exponents) {
        const float a = static_cast<float>(ldexp(1.0, exponent[0]));
        const float b = static_cast<float>(ldexp(1.0, exponent[1]));
        const double scale = ldexp(1.0, exponent[0] + exponent[1]);
        const VxQuaternion left(a, 2*a, 3*a, 4*a), right(-3*b, 2*b, -b, 5*b);
        const float product[] = {float(-15*scale), float(10*scale), float(19*scale), float(22*scale)};
        const float divided[] = {float(9*scale), float(-6*scale), float(27*scale), float(18*scale)};
        VxQuaternion result = left * right, quotient = left / right;
        for (int i = 0; i < 4; ++i) {
            EXPECT_EQ(product[i], result[i]);
            EXPECT_EQ(divided[i], quotient[i]);
        }
#if defined(VX_SIMD_SSE)
        result = left;
        VxSIMDMultiplyQuaternion(&result, &result, &right);
        quotient = right;
        VxSIMDDivideQuaternion(&quotient, &left, &quotient);
        for (int i = 0; i < 4; ++i) {
            EXPECT_EQ(product[i], result[i]);
            EXPECT_EQ(divided[i], quotient[i]);
        }
#endif
    }
}

TEST_F(QuaternionRangeRegressionTest, RangeFallbackPreservesCallerControlsAndAvoidsSpuriousOverflow) {
    fesetround(FE_DOWNWARD);
    feclearexcept(FE_ALL_EXCEPT);
#if defined(VX_SIMD_X86)
    const unsigned controls = _mm_getcsr() & ~0x3fu;
#endif
    volatile float stored = 0x1.6p63f;
    const VxQuaternion q(stored, stored, stored, stored);
    const float expected = static_cast<float>(2.0 * double(stored) * stored);
    ExpectProduct(q * q, expected);
    EXPECT_EQ(0, fetestexcept(FE_OVERFLOW | FE_INVALID));
    EXPECT_EQ(FE_DOWNWARD, fegetround());
#if defined(VX_SIMD_X86)
    EXPECT_EQ(controls, _mm_getcsr() & ~0x3fu);
#endif
}

TEST_F(QuaternionRangeRegressionTest, ZeroComponentsDoNotHideTinyOrLargeTerms) {
    const float values[] = {0.0f, -0.0f, 0x1p-149f, 0x1p-61f, 0x1p-60f,
                            0x1.fffffep60f, 0x1p61f, 0x1.fffffep127f};
    const VxQuaternion identity(0, 0, 0, 1);
    for (float value : values) {
        const VxQuaternion q(value, 0, 0, 0);
        const VxQuaternion product = q * identity, quotient = q / identity;
        EXPECT_EQ(value, product.x);
        EXPECT_EQ(value, quotient.x);
        for (int i = 1; i < 4; ++i) {
            EXPECT_EQ(0, product[i]);
            EXPECT_EQ(0, quotient[i]);
        }
    }
}
