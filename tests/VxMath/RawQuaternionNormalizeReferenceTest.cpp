#include <float.h>
#include <math.h>
#include <string.h>

// The native probe uses these inputs and this scaled reference with the 2.1 DLL.
static unsigned RawQuaternionBits(float value) {
    unsigned bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float RawQuaternionFloat(unsigned bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void RawQuaternionExpected(const float input[4], float expected[4]) {
    double scale = 0;
    for (int lane = 0; lane < 4; ++lane) {
        if (input[lane] != input[lane]) {
            for (int k = 0; k < 4; ++k) expected[k] = RawQuaternionFloat(0x7fc00000u);
            return;
        }
        if (fabs(input[lane]) > scale) scale = fabs(input[lane]);
    }
    if (scale == 0) {
        expected[0] = expected[1] = expected[2] = 0;
        expected[3] = 1;
        return;
    }
    if (scale == HUGE_VAL) {
        for (int lane = 0; lane < 4; ++lane) expected[lane] = input[lane] * 0.0f;
        return;
    }
    double square = 0;
    for (int lane = 0; lane < 4; ++lane) {
        const double ratio = input[lane] / scale;
        square += ratio * ratio;
    }
    const double length = sqrt(square);
    for (int lane = 0; lane < 4; ++lane) {
        expected[lane] = static_cast<float>((input[lane] / scale) / length);
    }
}

static bool RawQuaternionClose(float actual, float expected) {
    if (expected != expected) return actual != actual;
    if (actual != actual || fabs(actual) > FLT_MAX) return false;
    const unsigned a = RawQuaternionBits(actual), b = RawQuaternionBits(expected);
    if ((a ^ b) & 0x80000000u) return false;
    return (a > b ? a - b : b - a) <= 3;
}

static void RawQuaternionRandom(unsigned &state, float input[4]) {
    for (int lane = 0; lane < 4; ++lane) {
        state = state * 1664525u + 1013904223u;
        unsigned bits = state;
        if ((bits & 0x7f800000u) == 0x7f800000u) bits ^= 0x00800000u;
        input[lane] = RawQuaternionFloat(bits);
    }
}

#ifndef VX_RAW_QUATERNION_NORMALIZE_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>

#if defined(VX_SIMD_SSE)
static void CheckRawQuaternion(const float input[4]) {
    float actual[4], expected[4];
    _mm_storeu_ps(actual, VxSIMDQuaternionNormalize(_mm_loadu_ps(input)));
    RawQuaternionExpected(input, expected);
    bool finite = true;
    for (int lane = 0; lane < 4; ++lane) {
        EXPECT_TRUE(RawQuaternionClose(actual[lane], expected[lane]))
            << "lane " << lane << ": " << actual[lane] << " expected " << expected[lane];
        finite = finite && expected[lane] == expected[lane];
    }
    if (finite) {
        double square = 0;
        for (int lane = 0; lane < 4; ++lane) square += double(actual[lane]) * actual[lane];
        EXPECT_NEAR(square, 1.0, 3e-7);
    }
}

TEST(RawQuaternionNormalizeReferenceTest, ZeroReturnsIdentity) {
    for (unsigned signs = 0; signs < 16; ++signs) {
        float input[4];
        for (int lane = 0; lane < 4; ++lane) {
            input[lane] = RawQuaternionFloat((signs & (1u << lane)) ? 0x80000000u : 0);
        }
        CheckRawQuaternion(input);
    }
}

TEST(RawQuaternionNormalizeReferenceTest, EveryLaneContributesToLength) {
    const float inputs[][4] = {{1, 2, 2, 4}, {0, 0, 3, 4}, {4, 3, 0, 0}, {0, 0, 0, -5}};
    for (const auto &input : inputs) CheckRawQuaternion(input);
}

TEST(RawQuaternionNormalizeReferenceTest, FiniteScalePreservesDirectionAcrossFloatRange) {
    for (int exponent = -149; exponent <= 127; ++exponent) {
        SCOPED_TRACE(exponent);
        const float scale = ldexpf(1.0f, exponent);
        const float input[4] = {scale, -scale * 0.5f, scale * 0.25f, -scale * 0.125f};
        CheckRawQuaternion(input);
    }
    const float largest[4] = {FLT_MAX, -FLT_MAX, FLT_MAX, -FLT_MAX};
    CheckRawQuaternion(largest);
}

TEST(RawQuaternionNormalizeReferenceTest, MixedExponentsPreserveSmallComponents) {
    const float inputs[][4] = {
        {1, 0x1p-100f, -0x1p-120f, 0},
        {0x1p100f, 1, -0x1p-40f, 0},
        {0x1p-120f, -0x1p-130f, 0x1p-140f, 0},
        {0x1p64f, -0x1p64f, 0x1p63f, 0x1p-64f}
    };
    for (const auto &input : inputs) CheckRawQuaternion(input);
}

TEST(RawQuaternionNormalizeReferenceTest, ArbitraryFiniteInputsRemainUnitQuaternions) {
    unsigned state = 0x7ad134b9u;
    for (int sample = 0; sample < 2048; ++sample) {
        SCOPED_TRACE(sample);
        float input[4];
        RawQuaternionRandom(state, input);
        CheckRawQuaternion(input);
    }
}

TEST(RawQuaternionNormalizeReferenceTest, OrdinaryRangeAndRepeatedNormalizationRemainAccurate) {
    unsigned state = 0x127ac981u;
    for (int sample = 0; sample < 4096; ++sample) {
        SCOPED_TRACE(sample);
        float input[4];
        for (int lane = 0; lane < 4; ++lane) {
            state = state * 1664525u + 1013904223u;
            input[lane] = (int(state >> 8) % 2001 - 1000) * 0.001f;
        }
        CheckRawQuaternion(input);
    }
    const float input[4] = {3, -4, 5, -6};
    __m128 q = _mm_loadu_ps(input);
    for (int i = 0; i < 1024; ++i) q = VxSIMDQuaternionNormalize(q);
    float actual[4], expected[4];
    _mm_storeu_ps(actual, q);
    RawQuaternionExpected(input, expected);
    for (int lane = 0; lane < 4; ++lane) EXPECT_NEAR(actual[lane], expected[lane], 1e-6f);
}

TEST(RawQuaternionNormalizeReferenceTest, OppositeInputsPreserveQuaternionSign) {
    const float input[4] = {3, -4, 5, -6};
    float a[4], b[4];
    const __m128 q = _mm_loadu_ps(input);
    _mm_storeu_ps(a, VxSIMDQuaternionNormalize(q));
    _mm_storeu_ps(b, VxSIMDQuaternionNormalize(_mm_xor_ps(q, _mm_set1_ps(-0.0f))));
    for (int lane = 0; lane < 4; ++lane) EXPECT_EQ(RawQuaternionBits(a[lane]) ^ 0x80000000u, RawQuaternionBits(b[lane]));
}

TEST(RawQuaternionNormalizeReferenceTest, NonfiniteInputsFollowOriginalArithmetic) {
    for (int lane = 0; lane < 4; ++lane) {
        float input[4] = {1, -2, 3, -4};
        input[lane] = RawQuaternionFloat(0x7fc00000u);
        CheckRawQuaternion(input);
        input[lane] = HUGE_VALF;
        CheckRawQuaternion(input);
        input[lane] = -HUGE_VALF;
        CheckRawQuaternion(input);
    }
}
#endif
#endif
