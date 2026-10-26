#include <new>
#include <string.h>
#ifndef VX_VECTOR_EXTREMA_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

static const unsigned ExtremaValues[] = {
    0xc0600000u, 0xbf800000u, 0x80000000u, 0u, 0x40000000u,
    0x7f800000u, 0xff800000u, 0x7fc12345u, 0xffc54321u
};

static float ExtremaFloat(unsigned bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static unsigned ExtremaBits(float value) {
    unsigned bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float ExpectedMinimum(const VxVector &value) {
    float selected = value.x < value.y ? value.x : value.y;
    if (value.z < selected) selected = value.z;
    return selected;
}

static float ExpectedMaximum(const VxVector &value) {
    float selected = value.x;
    if (selected < value.y) selected = value.y;
    if (selected < value.z) selected = value.z;
    return selected;
}

typedef void (*VectorExtremaFunction)(VxVector *, const VxVector *, const VxVector *);
static void FreeMinimum(VxVector *result, const VxVector *a, const VxVector *b) { *result = Minimize(*a, *b); }
static void FreeMaximum(VxVector *result, const VxVector *a, const VxVector *b) { *result = Maximize(*a, *b); }
#if defined(VX_SIMD_SSE)
static void PointerMinimum(VxVector *result, const VxVector *a, const VxVector *b) { VxSIMDMinimizeVector(result, a, b); }
static void PointerMaximum(VxVector *result, const VxVector *a, const VxVector *b) { VxSIMDMaximizeVector(result, a, b); }
static void RegisterMinimum(VxVector *result, const VxVector *a, const VxVector *b) {
    VxSIMDStoreFloat3(&result->x, VxSIMDMin(VxSIMDLoadFloat3(&a->x), VxSIMDLoadFloat3(&b->x)));
}
static void RegisterMaximum(VxVector *result, const VxVector *a, const VxVector *b) {
    VxSIMDStoreFloat3(&result->x, VxSIMDMax(VxSIMDLoadFloat3(&a->x), VxSIMDLoadFloat3(&b->x)));
}
#endif

static bool CallExtrema(VectorExtremaFunction function, VxVector *result, const VxVector *a, const VxVector *b) {
#if defined(_WIN32)
    __try { function(result, a, b); return true; } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
#else
    function(result, a, b);
    return true;
#endif
}

#ifndef VX_VECTOR_EXTREMA_PROBE
TEST(VectorExtremaReference, HorizontalFinitePermutationsHaveTheSameRange) {
    const float values[][3] = {{-3.5f, 2, 9}, {-3.5f, 9, 2}, {2, -3.5f, 9}, {2, 9, -3.5f}, {9, -3.5f, 2}, {9, 2, -3.5f}};
    for (const auto &value : values) {
        VxVector vector(value);
        EXPECT_FLOAT_EQ(Min(vector), -3.5f);
        EXPECT_FLOAT_EQ(Max(vector), 9);
    }
}

TEST(VectorExtremaReference, HorizontalMinimumSelectsTheOriginalOperand) {
    for (unsigned x : ExtremaValues) for (unsigned y : ExtremaValues) for (unsigned z : ExtremaValues) {
        VxVector value(ExtremaFloat(x), ExtremaFloat(y), ExtremaFloat(z));
        EXPECT_EQ(ExtremaBits(Min(value)), ExtremaBits(ExpectedMinimum(value)));
    }
}

TEST(VectorExtremaReference, HorizontalMaximumSelectsTheOriginalOperand) {
    for (unsigned x : ExtremaValues) for (unsigned y : ExtremaValues) for (unsigned z : ExtremaValues) {
        VxVector value(ExtremaFloat(x), ExtremaFloat(y), ExtremaFloat(z));
        EXPECT_EQ(ExtremaBits(Max(value)), ExtremaBits(ExpectedMaximum(value)));
    }
}

TEST(VectorExtremaReference, ReductionPreservesInputAndIgnoresTheFourthLane) {
    VxVector4 value(1, 2, 3, ExtremaFloat(0x7fc12345));
    unsigned char before[sizeof(value)];
    memcpy(before, &value, sizeof(value));
    EXPECT_FLOAT_EQ(Min(value), 1);
    EXPECT_FLOAT_EQ(Max(value), 3);
    EXPECT_EQ(memcmp(before, &value, sizeof(value)), 0);
}

class VectorExtremaReference : public testing::TestWithParam<int> {
protected:
    bool Maximum() { return GetParam() % 2 != 0; }
    VectorExtremaFunction Function() {
        switch (GetParam()) {
        case 0: return FreeMinimum;
        case 1: return FreeMaximum;
#if defined(VX_SIMD_SSE)
        case 2: return PointerMinimum;
        case 3: return PointerMaximum;
        case 4: return RegisterMinimum;
        case 5: return RegisterMaximum;
#endif
        default: return nullptr;
        }
    }
    void ExpectVector(const VxVector &result, const VxVector &a, const VxVector &b) {
        for (int i = 0; i < 3; ++i) {
            const float expected = Maximum() ? (a[i] > b[i] ? a[i] : b[i]) : (a[i] < b[i] ? a[i] : b[i]);
            EXPECT_EQ(ExtremaBits(result[i]), ExtremaBits(expected));
        }
    }
};

TEST_P(VectorExtremaReference, ComponentSelectionUsesBothOperandsWithoutNormalization) {
    VxVector a(-1, 3, 9), b(4, -4, 2), result;
    Function()(&result, &a, &b);
    const VxVector expected = Maximum() ? VxVector(4, 3, 9) : VxVector(-1, -4, 2);
    EXPECT_FLOAT_EQ(result.x, expected.x);
    EXPECT_FLOAT_EQ(result.y, expected.y);
    EXPECT_FLOAT_EQ(result.z, expected.z);
}

TEST_P(VectorExtremaReference, EqualAndUnorderedOperandsSelectTheSecondInput) {
    for (unsigned x : ExtremaValues) for (unsigned y : ExtremaValues) {
        VxVector a(ExtremaFloat(x), ExtremaFloat(y), ExtremaFloat(x));
        VxVector b(ExtremaFloat(y), ExtremaFloat(x), ExtremaFloat(y)), result;
        Function()(&result, &a, &b);
        ExpectVector(result, a, b);
    }
}

TEST_P(VectorExtremaReference, ResultCanReplaceEitherInput) {
    VxVector a(-1, 3, 9), b(4, -4, 2), left = a, right = b;
    Function()(&left, &left, &right);
    ExpectVector(left, a, b);
    left = a;
    Function()(&right, &left, &right);
    ExpectVector(right, a, b);
}

TEST_P(VectorExtremaReference, FourByteAlignmentPreservesInputsAndOutputW) {
    for (int offset = 0; offset < 16; offset += 4) {
        alignas(16) unsigned char storage[96], before[96];
        memset(storage, 0x6d, sizeof(storage));
        VxVector4 *a = ::new(storage + offset) VxVector4(-1, 3, 9, ExtremaFloat(0x7fc12345));
        VxVector4 *b = ::new(storage + 32 + offset) VxVector4(4, -4, 2, ExtremaFloat(0x7f800000));
        VxVector4 *result = ::new(storage + 64 + offset) VxVector4(0, 0, 0, 42);
        memcpy(before, storage, sizeof(storage));
        Function()(result, a, b);
        ExpectVector(*result, *a, *b);
        EXPECT_EQ(memcmp(before, storage, 64 + offset), 0);
        EXPECT_EQ(memcmp(before + 76 + offset, storage + 76 + offset, 20 - offset), 0);
    }
}

#if defined(_WIN32)
TEST_P(VectorExtremaReference, ReadonlyXyzInputsAndExactTwelveByteOutput) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    const size_t page = info.dwPageSize;
    unsigned char *memory = static_cast<unsigned char *>(VirtualAlloc(nullptr, 7 * page, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
    ASSERT_NE(memory, nullptr);
    DWORD old;
    ASSERT_TRUE(VirtualProtect(memory + page, page, PAGE_READWRITE, &old));
    ASSERT_TRUE(VirtualProtect(memory + 3 * page, page, PAGE_READWRITE, &old));
    ASSERT_TRUE(VirtualProtect(memory + 5 * page, page, PAGE_READWRITE, &old));
    VxVector *a = ::new(memory + 2 * page - 12) VxVector(-1, 3, 9);
    VxVector *b = ::new(memory + 4 * page - 12) VxVector(4, -4, 2);
    VxVector *result = ::new(memory + 6 * page - 12) VxVector;
    ASSERT_TRUE(VirtualProtect(memory + page, page, PAGE_READONLY, &old));
    ASSERT_TRUE(VirtualProtect(memory + 3 * page, page, PAGE_READONLY, &old));
    const bool called = CallExtrema(Function(), result, a, b);
    EXPECT_TRUE(called);
    if (called) ExpectVector(*result, *a, *b);
    EXPECT_FLOAT_EQ(Min(*a), -1);
    EXPECT_FLOAT_EQ(Max(*b), 4);
    VirtualFree(memory, 0, MEM_RELEASE);
}
#endif

#if defined(VX_SIMD_SSE)
INSTANTIATE_TEST_SUITE_P(FreePointerRegister, VectorExtremaReference, testing::Values(0, 1, 2, 3, 4, 5));
TEST(VectorExtremaRegister, EveryLaneUsesTheSameOperandSelection) {
    const float a[] = {0, -1, 3, ExtremaFloat(0x7fc12345)};
    const float b[] = {-0.0f, 2, -4, 7};
    float minimum[4], maximum[4];
    _mm_storeu_ps(minimum, VxSIMDMin(_mm_loadu_ps(a), _mm_loadu_ps(b)));
    _mm_storeu_ps(maximum, VxSIMDMax(_mm_loadu_ps(a), _mm_loadu_ps(b)));
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(ExtremaBits(minimum[i]), ExtremaBits(a[i] < b[i] ? a[i] : b[i]));
        EXPECT_EQ(ExtremaBits(maximum[i]), ExtremaBits(a[i] > b[i] ? a[i] : b[i]));
    }
}
#else
INSTANTIATE_TEST_SUITE_P(Free, VectorExtremaReference, testing::Values(0, 1));
#endif
#endif
