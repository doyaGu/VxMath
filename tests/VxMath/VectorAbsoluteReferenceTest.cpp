#include <new>
#include <string.h>
#ifndef VX_VECTOR_ABSOLUTE_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

static const unsigned AbsoluteValues[] = {
    0u, 0x80000000u, 0x3f800000u, 0xbf800000u,
    0x422a0000u, 0xc22a0000u, 0x00800000u, 0x80800000u,
    0x00000001u, 0x80000001u, 0x7f7fffffu, 0xff7fffffu,
    0x7f800000u, 0xff800000u, 0x7fc12345u, 0xffc54321u
};

static float AbsoluteFloat(unsigned bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static unsigned AbsoluteBits(float value) {
    unsigned bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

typedef void (*AbsoluteFunction)(VxVector *, const VxVector *);

static void FreeAbsolute(VxVector *result, const VxVector *source) {
    *result = Absolute(*source);
}

static void MemberAbsolute(VxVector *result, const VxVector *source) {
    *result = *source;
    result->Absolute();
}

static bool CallAbsolute(AbsoluteFunction function, VxVector *result, const VxVector *source) {
#if defined(_WIN32)
    __try {
        function(result, source);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    function(result, source);
    return true;
#endif
}

#ifndef VX_VECTOR_ABSOLUTE_PROBE
class VectorAbsoluteReference : public testing::TestWithParam<int> {
protected:
    AbsoluteFunction Function() {
        return GetParam() == 0 ? FreeAbsolute : MemberAbsolute;
    }

    void ExpectAbsolute(const VxVector &result, const VxVector &source) {
        EXPECT_EQ(AbsoluteBits(result.x), AbsoluteBits(source.x) & 0x7fffffffu);
        EXPECT_EQ(AbsoluteBits(result.y), AbsoluteBits(source.y) & 0x7fffffffu);
        EXPECT_EQ(AbsoluteBits(result.z), AbsoluteBits(source.z) & 0x7fffffffu);
    }
};

TEST_P(VectorAbsoluteReference, NegativeAndPositiveComponentsHaveTheExpectedMagnitudes) {
    VxVector source(-3, 4, -12), result;
    Function()(&result, &source);
    EXPECT_FLOAT_EQ(result.x, 3);
    EXPECT_FLOAT_EQ(result.y, 4);
    EXPECT_FLOAT_EQ(result.z, 12);
}

TEST_P(VectorAbsoluteReference, FiniteZeroInfinityAndQuietNanClearOnlyTheSign) {
    for (unsigned bits : AbsoluteValues) {
        VxVector source(AbsoluteFloat(bits), AbsoluteFloat(bits ^ 0x80000000u), -2), result;
        Function()(&result, &source);
        ExpectAbsolute(result, source);
    }
}

TEST_P(VectorAbsoluteReference, ResultCanReplaceTheSource) {
    for (unsigned bits : AbsoluteValues) {
        const VxVector source(AbsoluteFloat(bits), -1, -2);
        VxVector result = source;
        Function()(&result, &result);
        ExpectAbsolute(result, source);
    }
}

TEST_P(VectorAbsoluteReference, ApplyingAbsoluteTwiceDoesNotChangeTheResult) {
    for (unsigned bits : AbsoluteValues) {
        const VxVector source(AbsoluteFloat(bits), -1, -2);
        VxVector result;
        Function()(&result, &source);
        const VxVector once = result;
        Function()(&result, &result);
        EXPECT_EQ(memcmp(&result, &once, sizeof(result)), 0);
    }
}

TEST_P(VectorAbsoluteReference, SeparateSourceRemainsUnchanged) {
    VxVector source(AbsoluteFloat(0xffc54321u), -0.0f, -3), result;
    unsigned char before[sizeof(source)];
    memcpy(before, &source, sizeof(source));
    Function()(&result, &source);
    ExpectAbsolute(result, source);
    EXPECT_EQ(memcmp(before, &source, sizeof(source)), 0);
}

TEST_P(VectorAbsoluteReference, FourByteAlignmentPreservesInputAndDerivedW) {
    for (int offset = 0; offset < 16; offset += 4) {
        alignas(16) unsigned char storage[64], before[64];
        memset(storage, 0x6d, sizeof(storage));
        VxVector4 *source = ::new(storage + offset) VxVector4(-1, -2, -3, -42);
        VxVector4 *result = ::new(storage + 32 + offset) VxVector4(0, 0, 0, -17);
        memcpy(before, storage, sizeof(storage));
        Function()(result, source);
        ExpectAbsolute(*result, *source);
        EXPECT_EQ(memcmp(before, storage, 32 + offset), 0);
        EXPECT_EQ(memcmp(before + 44 + offset, storage + 44 + offset, 20 - offset), 0);
        EXPECT_FLOAT_EQ(result->w, -17);
        Function()(source, source);
        EXPECT_FLOAT_EQ(source->w, -42);
    }
}

#if defined(_WIN32)
TEST_P(VectorAbsoluteReference, ReadonlyXyzInputAndExactTwelveByteOutput) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    const size_t page = info.dwPageSize;
    unsigned char *memory = static_cast<unsigned char *>(VirtualAlloc(nullptr, 5 * page, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
    ASSERT_NE(memory, nullptr);
    DWORD old;
    ASSERT_TRUE(VirtualProtect(memory + page, page, PAGE_READWRITE, &old));
    ASSERT_TRUE(VirtualProtect(memory + 3 * page, page, PAGE_READWRITE, &old));
    VxVector *source = ::new(memory + 2 * page - 12) VxVector(-1, -2, -3);
    VxVector *result = ::new(memory + 4 * page - 12) VxVector;
    ASSERT_TRUE(VirtualProtect(memory + page, page, PAGE_READONLY, &old));
    const bool called = CallAbsolute(Function(), result, source);
    EXPECT_TRUE(called);
    if (called) {
        ExpectAbsolute(*result, *source);
        Function()(result, result);
        EXPECT_FLOAT_EQ(result->z, 3);
    }
    VirtualFree(memory, 0, MEM_RELEASE);
}
#endif

INSTANTIATE_TEST_SUITE_P(FreeAndMember, VectorAbsoluteReference, testing::Values(0, 1));
#endif
