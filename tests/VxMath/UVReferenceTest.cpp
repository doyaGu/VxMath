#include <float.h>
#include <math.h>
#include <new>
#include <stddef.h>
#include <string.h>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifndef VX_UV_PROBE
#include "VxDefines.h"
#include <gtest/gtest.h>
#endif

static unsigned UVBits(float value) {
    unsigned bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}
static float UVFloat(unsigned bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

struct UVChecks {
    int checks = 0;
    int errors = 0;
    void Expect(bool condition) { ++checks; if (!condition) ++errors; }
    void Value(const VxUV &value, float u, float v) {
        Expect(u != u ? value.u != value.u : UVBits(value.u) == UVBits(u));
        Expect(v != v ? value.v != value.v : UVBits(value.v) == UVBits(v));
    }
};

static UVChecks UVLayoutAndConstruction() {
    UVChecks result;
    result.Expect(sizeof(VxUV) == 8 && alignof(VxUV) == 4);
    result.Expect(offsetof(VxUV, u) == 0 && offsetof(VxUV, v) == 4);
    result.Value(VxUV(), 0, 0);
    VxUV implicit = 2.5f;
    result.Value(implicit, 2.5f, 0);
    result.Value(VxUV(-3, 7), -3, 7);
    result.Value(VxUV(-0.0f, UVFloat(0x7fc12345u)), -0.0f, UVFloat(0x7fc12345u));
    return result;
}

static UVChecks UVCompoundAndSelfAlias() {
    UVChecks result;
    VxUV value(1.5f, -2.25f);
    result.Expect(&(value += value) == &value);
    result.Value(value, 3, -4.5f);
    result.Expect(&(value -= value) == &value);
    result.Value(value, 0, 0);
    value = VxUV(3, -5);
    result.Expect(&(value *= -2) == &value);
    result.Value(value, -6, 10);
    result.Expect(&(value /= -4) == &value);
    result.Value(value, 1.5f, -2.5f);
    value = VxUV(1, 2);
    (value += VxUV(2, -1)) *= 3;
    result.Value(value, 9, 3);
    return result;
}

static UVChecks UVUnaryAndCopy() {
    UVChecks result;
    const VxUV original(2.5f, -1.5f);
    result.Value(+original, 2.5f, -1.5f);
    result.Value(-original, -2.5f, 1.5f);
    VxUV copied(original), assigned;
    assigned = original;
    copied.u = 17;
    assigned.v = 23;
    result.Value(original, 2.5f, -1.5f);
    result.Value(copied, 17, -1.5f);
    result.Value(assigned, 2.5f, 23);
    const VxUV special(-0.0f, UVFloat(0x7fc12345u));
    const VxUV plus = +special;
    result.Expect(UVBits(plus.u) == 0x80000000u && UVBits(plus.v) == 0x7fc12345u);
    return result;
}

static UVChecks UVBinaryValues() {
    UVChecks result;
    const VxUV a(2.5f, -1.5f), b(-0.5f, 3.25f);
    result.Value(a + b, 2, 1.75f);
    result.Value(a - b, 3, -4.75f);
    result.Value(a * -4, -10, 6);
    result.Value(-4 * a, -10, 6);
    result.Value(a / 2, 1.25f, -0.75f);
    result.Value(a, 2.5f, -1.5f);
    result.Value(b, -0.5f, 3.25f);
    return result;
}

static UVChecks UVDivisionRangeAndSigns() {
    UVChecks result;
    const float tiny = UVFloat(1);
    result.Value(VxUV(FLT_MAX, -FLT_MAX) / FLT_MAX, 1, -1);
    result.Value(VxUV(tiny, -tiny) / tiny, 1, -1);
    VxUV largeValue(FLT_MAX, -FLT_MAX), tinyValue(tiny, -tiny);
    largeValue /= FLT_MAX; tinyValue /= tiny;
    result.Value(largeValue, 1, -1);
    result.Value(tinyValue, 1, -1);
    result.Value(VxUV(1, -2) / 0.0f, HUGE_VALF, -HUGE_VALF);
    result.Value(VxUV(1, -2) / -0.0f, -HUGE_VALF, HUGE_VALF);
    const float nan = UVFloat(0x7fc00000u);
    result.Value(VxUV(0, -0.0f) / 0.0f, nan, nan);
    result.Value(VxUV(0, -0.0f) / -2, -0.0f, 0);
    return result;
}

static UVChecks UVNonfiniteArithmetic() {
    UVChecks result;
    const float nan = UVFloat(0x7fc00000u);
    result.Value(VxUV(nan, 2) + VxUV(1, 3), nan, 5);
    result.Value(VxUV(HUGE_VALF, -HUGE_VALF) * 0, nan, nan);
    result.Value(VxUV(3, -5) / HUGE_VALF, 0, -0.0f);
    result.Value(VxUV(0, -0.0f) * -2, -0.0f, 0);
    VxUV self(HUGE_VALF, nan);
    self -= self;
    result.Value(self, nan, nan);
    return result;
}

static UVChecks UVTilingAndMirroring() {
    UVChecks result;
    const VxUV coordinate(0.25f, 0.75f), offset(-2, 3);
    result.Value(coordinate * 4 + offset, -1, 6);
    result.Value(VxUV(1, 1) - coordinate, 0.75f, 0.25f);
    result.Value((coordinate * 8 - VxUV(4, 4)) / 2, -1, 1);
    result.Value(coordinate, 0.25f, 0.75f);
    return result;
}

static UVChecks UVObjectFootprint() {
    UVChecks result;
    alignas(VxUV) unsigned char storage[24];
    memset(storage, 0xa5, sizeof(storage));
    VxUV *value = new(storage + 8) VxUV(1, 2);
    *value += VxUV(3, -4);
    *value *= -2;
    *value /= 4;
    result.Value(*value, -2, 1);
    value->~VxUV();
    for (unsigned k = 0; k < sizeof(storage); ++k) {
        if (k < 8 || k >= 16) result.Expect(storage[k] == 0xa5);
    }
    return result;
}

#if defined(_WIN32)
static void UVWrite(int operation, VxUV *out, const VxUV &a, const VxUV &b) {
    switch (operation) {
    case 0: *out = +a; break;
    case 1: *out = -a; break;
    case 2: *out = a + b; break;
    case 3: *out = a - b; break;
    case 4: *out = a * -2; break;
    case 5: *out = -2 * a; break;
    case 6: *out = a / 2; break;
    case 7: *out = a; *out += b; break;
    case 8: *out = a; *out -= b; break;
    case 9: *out = a; *out *= -2; break;
    case 10: *out = a; *out /= 2; break;
    }
}
static bool UVProtectedWrite(int operation, VxUV *out, const VxUV *a, const VxUV *b) {
    __try { UVWrite(operation, out, *a, *b); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static UVChecks UVGuardPages() {
    UVChecks result;
    SYSTEM_INFO info; GetSystemInfo(&info);
    unsigned char *pages[3] = {};
    bool ready = true;
    for (int k = 0; k < 3; ++k) {
        pages[k] = static_cast<unsigned char *>(VirtualAlloc(nullptr, info.dwPageSize * 2, MEM_RESERVE, PAGE_NOACCESS));
        ready = ready && pages[k] != nullptr;
        if (pages[k]) ready = VirtualAlloc(pages[k], info.dwPageSize, MEM_COMMIT, PAGE_READWRITE) != nullptr && ready;
    }
    result.Expect(ready);
    if (ready) {
        VxUV *a = new(pages[0] + info.dwPageSize - 8) VxUV(2, -4);
        VxUV *b = new(pages[1] + info.dwPageSize - 8) VxUV(1, 3);
        VxUV *out = new(pages[2] + info.dwPageSize - 8) VxUV;
        const float expected[11][2] = {{2,-4},{-2,4},{3,-1},{1,-7},{-4,8},{-4,8},{1,-2},{3,-1},{1,-7},{-4,8},{1,-2}};
        for (int operation = 0; operation < 11; ++operation) {
            const bool success = UVProtectedWrite(operation, out, a, b);
            result.Expect(success);
            if (success) result.Value(*out, expected[operation][0], expected[operation][1]);
            result.Value(*a, 2, -4);
            result.Value(*b, 1, 3);
        }
    }
    for (unsigned char *page : pages) if (page) VirtualFree(page, 0, MEM_RELEASE);
    return result;
}
#endif

#ifndef VX_UV_PROBE
TEST(UVReferenceTest, LayoutAndDefaultArguments) { EXPECT_EQ(UVLayoutAndConstruction().errors, 0); }
TEST(UVReferenceTest, CompoundOperatorsReturnSelfAndAllowSelfAlias) { EXPECT_EQ(UVCompoundAndSelfAlias().errors, 0); }
TEST(UVReferenceTest, UnaryAndImplicitCopiesPreserveIndependentValues) { EXPECT_EQ(UVUnaryAndCopy().errors, 0); }
TEST(UVReferenceTest, BinaryOperatorsPreserveOperands) { EXPECT_EQ(UVBinaryValues().errors, 0); }
TEST(UVReferenceTest, DirectDivisionPreservesRangeAndZeroSigns) { EXPECT_EQ(UVDivisionRangeAndSigns().errors, 0); }
TEST(UVReferenceTest, NonfiniteValuesFollowFloatArithmetic) { EXPECT_EQ(UVNonfiniteArithmetic().errors, 0); }
TEST(UVReferenceTest, TextureCoordinatesRemainUnclamped) { EXPECT_EQ(UVTilingAndMirroring().errors, 0); }
TEST(UVReferenceTest, ConstructorsAndOperatorsOnlyWriteEightBytes) { EXPECT_EQ(UVObjectFootprint().errors, 0); }
#if defined(_WIN32)
TEST(UVReferenceTest, OperandsAndResultsFitAtGuardPageBoundary) { EXPECT_EQ(UVGuardPages().errors, 0); }
#endif
#endif
