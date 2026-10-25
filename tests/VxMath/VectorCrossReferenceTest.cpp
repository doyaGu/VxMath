#include <math.h>
#include <new>
#include <string.h>
#ifndef VX_VECTOR_CROSS_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef void (*CrossFunction)(VxVector *, const VxVector *, const VxVector *);
static void FreeCross(VxVector *r, const VxVector *a, const VxVector *b) { *r = CrossProduct(*a, *b); }
#if defined(VX_SIMD_SSE)
static void PointerCross(VxVector *r, const VxVector *a, const VxVector *b) { VxSIMDCrossVector(r, a, b); }
static void RegisterCross(VxVector *r, const VxVector *a, const VxVector *b) {
    VxSIMDStoreFloat3(&r->x, VxSIMDCrossProduct3(VxSIMDLoadFloat3(&a->x), VxSIMDLoadFloat3(&b->x)));
}
#endif

struct CrossExample { float a[3], b[3], result[3]; };
static const CrossExample CrossExamples[] = {
    {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}},
    {{0, 1, 0}, {0, 0, 1}, {1, 0, 0}},
    {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},
    {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}},
    {{1, 2, 3}, {4, -5, 6}, {27, 6, -13}},
    {{2, -3, 4}, {-1, 5, 2}, {-26, -8, 7}},
    {{0.5f, 0.25f, -2}, {4, 8, -0.5f}, {15.875f, -7.75f, 3}},
    {{1, 2, 3}, {2, -1, 0}, {3, 6, -5}}
};

static bool CallCross(CrossFunction fn, VxVector *r, const VxVector *a, const VxVector *b) {
#if defined(_WIN32)
    __try { fn(r, a, b); return true; } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
#else
    fn(r, a, b); return true;
#endif
}

#ifndef VX_VECTOR_CROSS_PROBE
class VectorCrossReference : public testing::TestWithParam<int> {
protected:
    CrossFunction Function() {
        switch (GetParam()) {
        case 0: return FreeCross;
#if defined(VX_SIMD_SSE)
        case 1: return PointerCross;
        case 2: return RegisterCross;
#endif
        default: return nullptr;
        }
    }
    void ExpectVector(const VxVector &v, const float expected[3]) {
        EXPECT_FLOAT_EQ(v.x, expected[0]);
        EXPECT_FLOAT_EQ(v.y, expected[1]);
        EXPECT_FLOAT_EQ(v.z, expected[2]);
    }
};

TEST_P(VectorCrossReference,RightHandedAxesAndUnnormalizedAreaVectors) {
    for (const CrossExample &e : CrossExamples) {
        VxVector a(e.a), b(e.b), result;
        Function()(&result, &a, &b);
        ExpectVector(result, e.result);
    }
}
TEST_P(VectorCrossReference,ReversingInputsReversesTheNormal) {
    for (const CrossExample &e : CrossExamples) {
        VxVector a(e.a), b(e.b), result;
        const float reversed[] = {-e.result[0], -e.result[1], -e.result[2]};
        Function()(&result, &b, &a);
        ExpectVector(result, reversed);
    }
}
TEST_P(VectorCrossReference,SelfAndParallelVectorsHaveZeroArea) {
    const float values[][3] = {{1.1f, 1.2f, 1.3f}, {0.7f, -3.1f, 2.9f}, {0, 0, 0}, {3, 4, 5}};
    const float zero[] = {0, 0, 0};
    for (const auto &value : values) {
        VxVector a(value), b(-2 * a.x, -2 * a.y, -2 * a.z), result;
        Function()(&result, &a, &a);
        ExpectVector(result, zero);
        Function()(&result, &a, &b);
        ExpectVector(result, zero);
    }
}
TEST_P(VectorCrossReference,ResultCanReplaceEitherInput) {
    for (const CrossExample &e : CrossExamples) {
        VxVector a(e.a), b(e.b);
        Function()(&a, &a, &b);
        ExpectVector(a, e.result);
        a = VxVector(e.a);
        Function()(&b, &a, &b);
        ExpectVector(b, e.result);
    }
}
TEST_P(VectorCrossReference,InputsAndOutputWArePreservedWithFourByteAlignment) {
    for (int offset = 0; offset < 16; offset += 4) {
        alignas(16) unsigned char storage[96], before[96];
        memset(storage, 0x6d, sizeof(storage));
        VxVector4 *a = ::new(storage + offset) VxVector4(1, 2, 3, nanf(""));
        VxVector4 *b = ::new(storage + 32 + offset) VxVector4(4, -5, 6, HUGE_VALF);
        VxVector4 *r = ::new(storage + 64 + offset) VxVector4(0, 0, 0, 42);
        memcpy(before, storage, sizeof(storage));
        Function()(r, a, b);
        const float expected[] = {27, 6, -13};
        ExpectVector(*r, expected);
        EXPECT_EQ(memcmp(before, storage, 64 + offset), 0);
        EXPECT_EQ(memcmp(before + 76 + offset, storage + 76 + offset, 20 - offset), 0);
        EXPECT_FLOAT_EQ(r->w, 42);
    }
}
#if defined(_WIN32)
TEST_P(VectorCrossReference,ReadonlyXyzInputsAndExactTwelveByteOutput) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    const size_t page = info.dwPageSize;
    unsigned char *memory = static_cast<unsigned char *>(VirtualAlloc(nullptr, 7 * page, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
    ASSERT_NE(memory, nullptr);
    DWORD old;
    ASSERT_TRUE(VirtualProtect(memory + page, page, PAGE_READWRITE, &old));
    ASSERT_TRUE(VirtualProtect(memory + 3 * page, page, PAGE_READWRITE, &old));
    ASSERT_TRUE(VirtualProtect(memory + 5 * page, page, PAGE_READWRITE, &old));
    VxVector *a = ::new(memory + 2 * page - 12) VxVector(1, 2, 3);
    VxVector *b = ::new(memory + 4 * page - 12) VxVector(4, -5, 6);
    VxVector *r = ::new(memory + 6 * page - 12) VxVector;
    ASSERT_TRUE(VirtualProtect(memory + page, page, PAGE_READONLY, &old));
    ASSERT_TRUE(VirtualProtect(memory + 3 * page, page, PAGE_READONLY, &old));
    const bool called = CallCross(Function(), r, a, b);
    EXPECT_TRUE(called);
    if (called) {
        const float expected[] = {27, 6, -13};
        ExpectVector(*r, expected);
        EXPECT_FLOAT_EQ(a->x, 1);
        EXPECT_FLOAT_EQ(b->x, 4);
    }
    VirtualFree(memory, 0, MEM_RELEASE);
}
#endif
#if defined(VX_SIMD_SSE)
INSTANTIATE_TEST_SUITE_P(FreePointerRegister, VectorCrossReference, testing::Values(0, 1, 2));
TEST(VectorCrossRegister,InputWDoesNotAffectXyz) {
    const __m128 value = VxSIMDCrossProduct3(_mm_set_ps(nanf(""), 3, 2, 1), _mm_set_ps(HUGE_VALF, 6, -5, 4));
    float result[4];
    _mm_storeu_ps(result, value);
    EXPECT_FLOAT_EQ(result[0], 27);
    EXPECT_FLOAT_EQ(result[1], 6);
    EXPECT_FLOAT_EQ(result[2], -13);
}
#else
INSTANTIATE_TEST_SUITE_P(Free, VectorCrossReference, testing::Values(0));
#endif
#endif
