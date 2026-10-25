#include <math.h>
#include <new>
#include <string.h>
#ifndef VX_VECTOR_DOT_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif

typedef float (*DotFunction)(const VxVector &, const VxVector &);
static float MemberDot(const VxVector &a, const VxVector &b) { return a.Dot(b); }
static float FreeDot(const VxVector &a, const VxVector &b) { return DotProduct(a, b); }
#if defined(VX_SIMD_SSE)
static float PointerDot(const VxVector &a, const VxVector &b) { return VxSIMDDotVector(&a, &b); }
static float RegisterDot(const VxVector &a, const VxVector &b) {
    return _mm_cvtss_f32(VxSIMDDotProduct3(VxSIMDLoadFloat3(&a.x), VxSIMDLoadFloat3(&b.x)));
}
#endif

struct DotExample { float a[3], b[3], result; };
static const DotExample DotExamples[] = {
    {{1, 0, 0}, {0, 1, 0}, 0},
    {{1, 2, 3}, {4, -5, 6}, 12},
    {{2, -3, 4}, {-1, 5, 2}, -9},
    {{0.5f, 0.25f, -2}, {4, 8, -0.5f}, 5},
    {{3, 4, 0}, {3, 4, 0}, 25},
    {{1, 2, 3}, {-2, -4, -6}, -28},
    {{2, 3, 4}, {0, 0, 2}, 8},
    {{1, 2, 3}, {3, 6, -5}, 0}
};

#ifndef VX_VECTOR_DOT_PROBE
class VectorDotReference : public testing::TestWithParam<int> {
protected:
    DotFunction Function() {
        switch (GetParam()) {
        case 0: return MemberDot;
        case 1: return FreeDot;
#if defined(VX_SIMD_SSE)
        case 2: return PointerDot;
        case 3: return RegisterDot;
#endif
        default: return nullptr;
        }
    }
};

TEST_P(VectorDotReference,ProjectionLengthAndOrthogonalityExamples) {
    for (const DotExample &e : DotExamples)
        EXPECT_FLOAT_EQ(Function()(VxVector(e.a), VxVector(e.b)), e.result);
}
TEST_P(VectorDotReference,SwappingInputsPreservesTheResult) {
    for (const DotExample &e : DotExamples)
        EXPECT_FLOAT_EQ(Function()(VxVector(e.b), VxVector(e.a)), e.result);
}
TEST_P(VectorDotReference,InputsAreUnchangedAndDerivedWIsIgnored) {
    VxVector4 a(1, 2, 3, HUGE_VALF), b(4, -5, 6, nanf(""));
    unsigned char beforeA[sizeof(a)], beforeB[sizeof(b)];
    memcpy(beforeA, &a, sizeof(a));
    memcpy(beforeB, &b, sizeof(b));
    EXPECT_FLOAT_EQ(Function()(a, b), 12);
    EXPECT_EQ(memcmp(beforeA, &a, sizeof(a)), 0);
    EXPECT_EQ(memcmp(beforeB, &b, sizeof(b)), 0);
}
TEST_P(VectorDotReference,FourByteAlignedVectorsNeedNoSimdAlignment) {
    for (int offset = 0; offset < 16; offset += 4) {
        alignas(16) unsigned char storage[64];
        VxVector *a = ::new(storage + offset) VxVector(1, 2, 3);
        VxVector *b = ::new(storage + 32 + offset) VxVector(4, -5, 6);
        EXPECT_FLOAT_EQ(Function()(*a, *b), 12);
    }
}
TEST_P(VectorDotReference,ZeroAndOrdinaryIeeeSpecialValues) {
    EXPECT_EQ(Function()(VxVector(0.0f), VxVector(1, 2, 3)), 0);
    EXPECT_EQ(Function()(VxVector(HUGE_VALF, 1, 2), VxVector(1, 2, 3)), HUGE_VALF);
    EXPECT_TRUE(isnan(Function()(VxVector(nanf(""), 1, 2), VxVector(1, 2, 3))));
}
#if defined(VX_SIMD_SSE)
INSTANTIATE_TEST_SUITE_P(MemberFreePointerRegister, VectorDotReference, testing::Values(0, 1, 2, 3));
TEST(VectorDotRegister,BroadcastsTheXyzResultToEveryLane) {
    float result[4];
    _mm_storeu_ps(result, VxSIMDDotProduct3(_mm_set_ps(nanf(""), 3, 2, 1), _mm_set_ps(HUGE_VALF, 6, -5, 4)));
    for (float value : result) EXPECT_FLOAT_EQ(value, 12);
}
#else
INSTANTIATE_TEST_SUITE_P(MemberFree, VectorDotReference, testing::Values(0, 1));
#endif
#endif
