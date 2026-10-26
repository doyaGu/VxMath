#include <new>
#include <string.h>
#ifndef VX_VECTOR_INTERPOLATION_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef void (*VectorInterpolationFunction)(VxVector *, const VxVector *, const VxVector *, float);
static const float VectorInterpolationCases[][4] = {
    {0,6,-8,12}, {0.25f,5,-5,8.25f}, {0.5f,4,-2,4.5f},
    {1,2,4,-3}, {2,-2,16,-18}, {-1,10,-20,27}
};
static void FreeVectorInterpolation(VxVector *result, const VxVector *a, const VxVector *b, float step) {
    *result = Interpolate(step, *a, *b);
}
#if defined(VX_SIMD_SSE)
static void PointerVectorInterpolation(VxVector *result, const VxVector *a, const VxVector *b, float step) {
    VxSIMDLerpVector(result, a, b, step);
}
static void RegisterVectorInterpolation(VxVector *result, const VxVector *a, const VxVector *b, float step) {
    VxSIMDStoreFloat3(&result->x, VxSIMDLerp(VxSIMDLoadFloat3(&a->x), VxSIMDLoadFloat3(&b->x), step));
}
#endif

#ifndef VX_VECTOR_INTERPOLATION_PROBE
class VectorInterpolationReference : public testing::TestWithParam<int> {
protected:
    VectorInterpolationFunction Function() {
        switch (GetParam()) {
        case 0: return FreeVectorInterpolation;
#if defined(VX_SIMD_SSE)
        case 1: return PointerVectorInterpolation;
        case 2: return RegisterVectorInterpolation;
#endif
        default: return nullptr;
        }
    }
    void ExpectVector(const VxVector &result, const float *sample) {
        EXPECT_FLOAT_EQ(result.x, sample[1]);
        EXPECT_FLOAT_EQ(result.y, sample[2]);
        EXPECT_FLOAT_EQ(result.z, sample[3]);
    }
};

TEST_P(VectorInterpolationReference, EndpointsAndFractionalPositionsFollowTheOriginalFormula) {
    const VxVector a(6,-8,12), b(2,4,-3);
    for (int i = 0; i < 4; ++i) {
        VxVector result;
        Function()(&result, &a, &b, VectorInterpolationCases[i][0]);
        ExpectVector(result, VectorInterpolationCases[i]);
    }
}
TEST_P(VectorInterpolationReference, ExtrapolationDoesNotClampTheStep) {
    const VxVector a(6,-8,12), b(2,4,-3);
    for (int i = 4; i < 6; ++i) {
        VxVector result;
        Function()(&result, &a, &b, VectorInterpolationCases[i][0]);
        ExpectVector(result, VectorInterpolationCases[i]);
    }
}
TEST_P(VectorInterpolationReference, ResultCanReplaceEitherInput) {
    for (const auto &sample : VectorInterpolationCases) {
        VxVector a(6,-8,12), b(2,4,-3);
        Function()(&a, &a, &b, sample[0]);
        ExpectVector(a, sample);
        a.Set(6,-8,12);
        Function()(&b, &a, &b, sample[0]);
        ExpectVector(b, sample);
    }
}
TEST_P(VectorInterpolationReference, IdenticalFiniteEndpointsStayFixed) {
    const VxVector a(6,-8,12);
    for (const auto &sample : VectorInterpolationCases) {
        VxVector result;
        Function()(&result, &a, &a, sample[0]);
        EXPECT_EQ(result, a);
    }
}
TEST_P(VectorInterpolationReference, FourByteAlignmentPreservesInputsAndOutputW) {
    for (int offset = 0; offset < 16; offset += 4) {
        alignas(16) unsigned char storage[96], before[96];
        memset(storage, 0x6d, sizeof(storage));
        VxVector4 *a = ::new(storage + offset) VxVector4(6,-8,12,-42);
        VxVector4 *b = ::new(storage + 32 + offset) VxVector4(2,4,-3,-17);
        VxVector4 *result = ::new(storage + 64 + offset) VxVector4(0,0,0,-7);
        memcpy(before, storage, sizeof(storage));
        Function()(result, a, b, 0.25f);
        ExpectVector(*result, VectorInterpolationCases[1]);
        EXPECT_EQ(memcmp(before, storage, 64 + offset), 0);
        EXPECT_EQ(memcmp(before + 76 + offset, storage + 76 + offset, 20 - offset), 0);
    }
}
#if defined(_WIN32)
TEST_P(VectorInterpolationReference, ReadonlyXyzInputsAndExactTwelveByteOutput) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    const size_t page = info.dwPageSize;
    unsigned char *memory = static_cast<unsigned char *>(VirtualAlloc(nullptr, 7 * page, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
    ASSERT_NE(memory, nullptr);
    DWORD old;
    ASSERT_TRUE(VirtualProtect(memory + page, page, PAGE_READWRITE, &old));
    ASSERT_TRUE(VirtualProtect(memory + 3 * page, page, PAGE_READWRITE, &old));
    ASSERT_TRUE(VirtualProtect(memory + 5 * page, page, PAGE_READWRITE, &old));
    VxVector *a = ::new(memory + 2 * page - 12) VxVector(6,-8,12);
    VxVector *b = ::new(memory + 4 * page - 12) VxVector(2,4,-3);
    VxVector *result = ::new(memory + 6 * page - 12) VxVector;
    ASSERT_TRUE(VirtualProtect(memory + page, page, PAGE_READONLY, &old));
    ASSERT_TRUE(VirtualProtect(memory + 3 * page, page, PAGE_READONLY, &old));
    Function()(result, a, b, 0.25f);
    ExpectVector(*result, VectorInterpolationCases[1]);
    VirtualFree(memory, 0, MEM_RELEASE);
}
#endif
#if defined(VX_SIMD_SSE)
INSTANTIATE_TEST_SUITE_P(FreePointerRegister, VectorInterpolationReference, testing::Values(0,1,2));
TEST(VectorInterpolationRegister, TheFourthLaneUsesTheSameFormula) {
    float result[4];
    _mm_storeu_ps(result, VxSIMDLerp(_mm_set_ps(20,12,-8,6), _mm_set_ps(40,-3,4,2), 0.25f));
    EXPECT_FLOAT_EQ(result[0], 5);
    EXPECT_FLOAT_EQ(result[1], -5);
    EXPECT_FLOAT_EQ(result[2], 8.25f);
    EXPECT_FLOAT_EQ(result[3], 25);
}
#else
INSTANTIATE_TEST_SUITE_P(Free, VectorInterpolationReference, testing::Values(0));
#endif
#endif
