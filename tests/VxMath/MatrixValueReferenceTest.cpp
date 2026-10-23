#include <cstdint>
#include <cstring>
#include <new>
#include <initializer_list>
#include <type_traits>
#ifndef VX_MATRIX_VALUE_PROBE
#include "VxMatrix.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

struct MVResult { int samples = 0; int errors = 0; };
#if defined(_MSC_VER)
#define MV_NOINLINE __declspec(noinline)
#else
#define MV_NOINLINE __attribute__((noinline))
#endif
typedef void (*MVAction)(VxMatrix &);
static const std::uint32_t MVIdentityBits[16] = {
    0x3f800000, 0, 0, 0, 0, 0x3f800000, 0, 0,
    0, 0, 0x3f800000, 0, 0, 0, 0, 0x3f800000
};
static unsigned MVNext(unsigned &seed) { seed = seed * 1664525u + 1013904223u; return seed; }
static bool MVGuards(const unsigned char *buffer, int offset, int size) {
    for (int i = 0; i < size; ++i) {
        if ((i < offset || i >= offset + 64) && buffer[i] != 0xa5) return false;
    }
    return true;
}
MV_NOINLINE static VxMatrix *MVConstruct(void *buffer) { return new (buffer) VxMatrix; }
MV_NOINLINE static void MVClear(VxMatrix &m) { m.Clear(); }
MV_NOINLINE static void MVSetIdentity(VxMatrix &m) { m.SetIdentity(); }
MV_NOINLINE static void MVFreeIdentity(VxMatrix &m) { Vx3DMatrixIdentity(m); }
#if defined(VX_SIMD_SSE)
MV_NOINLINE static void MVSimdIdentity(VxMatrix &m) { VxSIMDMatrixIdentity(&m); }
#endif

static MVResult MVDefault() {
    MVResult result;
    for (int offset = 16; offset < 32; offset += 4) {
        for (unsigned char fill : {0x00, 0x55, 0xaa, 0xff}) {
            alignas(16) unsigned char buffer[128];
            std::memset(buffer, 0xa5, sizeof(buffer));
            std::memset(buffer + offset, fill, 64);
            VxMatrix *m = MVConstruct(buffer + offset);
            // Inspect object bytes only: never read the original SDK's uninitialized floats.
            unsigned char bytes[64]; std::memcpy(bytes, m, 64);
            bool ok = MVGuards(buffer, offset, sizeof(buffer));
            for (unsigned char byte : bytes) {
#ifdef VX_MATRIX_VALUE_PROBE
                ok = ok && byte == fill;
#else
                ok = ok && byte == 0;
#endif
            }
            ++result.samples; result.errors += !ok;
            m->~VxMatrix();
        }
    }
    return result;
}

static MVResult MVArrayCopy() {
    MVResult result; unsigned seed = 288;
    for (int sample = 0; sample < 256; ++sample) {
        std::uint32_t words[16]; for (unsigned &word : words) word = MVNext(seed);
        float source[4][4]; std::memcpy(source, words, 64);
        alignas(16) unsigned char buffer[128]; std::memset(buffer, 0xa5, sizeof(buffer));
        const int offset = 16 + 4 * (sample % 4);
        VxMatrix *m = new (buffer + offset) VxMatrix(source);
        bool ok = std::memcmp(m, words, 64) == 0 && std::memcmp(source, words, 64) == 0;
        ok = ok && MVGuards(buffer, offset, sizeof(buffer));
        reinterpret_cast<unsigned char *>(m)[sample % 64] ^= 0xff;
        ok = ok && std::memcmp(source, words, 64) == 0;
        VxMatrix copied(*m), assigned; assigned = *m;
        ok = ok && std::memcmp(&copied, m, 64) == 0 && std::memcmp(&assigned, m, 64) == 0;
        ++result.samples; result.errors += !ok; m->~VxMatrix();
    }
    return result;
}

static MVResult MVWrite(MVAction action, bool identity) {
    MVResult result; unsigned seed = 983;
    const std::uint32_t zeros[16] = {};
    for (int sample = 0; sample < 256; ++sample) {
        for (int offset = 16; offset < 32; offset += 4) {
            alignas(16) unsigned char buffer[128]; std::memset(buffer, 0xa5, sizeof(buffer));
            VxMatrix *m = MVConstruct(buffer + offset);
            std::uint32_t words[16]; for (unsigned &word : words) word = MVNext(seed);
            std::memcpy(m, words, 64); action(*m);
            const bool ok = std::memcmp(m, identity ? MVIdentityBits : zeros, 64) == 0 && MVGuards(buffer, offset, sizeof(buffer));
            ++result.samples; result.errors += !ok; m->~VxMatrix();
        }
    }
    return result;
}

static MVResult MVIdentityCopies() {
    MVResult result;
    for (int i = 0; i < 64; ++i) {
        VxMatrix first = VxMatrix::Identity();
        bool ok = std::memcmp(&first, MVIdentityBits, 64) == 0;
        first[i / 16][(i / 4) % 4] = -float(i + 1);
        VxMatrix second = VxMatrix::Identity();
        ok = ok && std::memcmp(&second, MVIdentityBits, 64) == 0;
        ++result.samples; result.errors += !ok;
    }
    return result;
}

static MVResult MVAccess() {
    MVResult result;
    for (int index = 0; index < 16; ++index) {
        float values[4][4]; for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) values[r][c] = float(4 * r + c + 1);
        VxMatrix m(values); const VxMatrix &constant = m;
        void *mutableData = m; const void *constData = constant;
        bool ok = mutableData == static_cast<void *>(&m) && constData == mutableData;
        for (int r = 0; r < 4; ++r) {
            ok = ok && static_cast<const void *>(&constant[r]) == static_cast<const unsigned char *>(constData) + 16 * r;
            ok = ok && static_cast<void *>(&m[r]) == static_cast<unsigned char *>(mutableData) + 16 * r;
            for (int c = 0; c < 4; ++c) ok = ok && constant[r][c] == values[r][c];
        }
        m[index / 4][index % 4] = -123.0f;
        ok = ok && static_cast<const float *>(constData)[index] == -123.0f;
        static_cast<float *>(mutableData)[index] = 456.0f;
        ok = ok && constant[index / 4][index % 4] == 456.0f;
        ++result.samples; result.errors += !ok;
    }
    return result;
}

static MVResult MVCompare() {
    MVResult result;
    const std::uint32_t patterns[] = {0, 0x80000000, 0x3f800000, 0x3f800001, 0x7f800000, 0xff800000, 0x7fc12345, 0xffc54321};
    for (int index = 0; index < 16; ++index) for (unsigned a : patterns) for (unsigned b : patterns) {
        VxMatrix left, right; left.SetIdentity(); right.SetIdentity();
        std::memcpy(&left[index / 4][index % 4], &a, 4); std::memcpy(&right[index / 4][index % 4], &b, 4);
        // IEEE numerical equality: signed zeros equal; any quiet NaN unequal.
        const bool nan = (a & 0x7fffffff) > 0x7f800000 || (b & 0x7fffffff) > 0x7f800000;
        const bool equal = !nan && (a == b || ((a | b) & 0x7fffffff) == 0);
        const int eq = left == right, ne = left != right;
        const bool ok = eq == int(equal) && ne == int(!equal) && (right == left) == eq && (right != left) == ne;
        ++result.samples; result.errors += !ok;
    }
    return result;
}

#if defined(_WIN32)
static MVResult MVBoundary(MVAction action, bool identity) {
    MVResult result; SYSTEM_INFO info; GetSystemInfo(&info); const unsigned size = info.dwPageSize;
    unsigned char *memory = static_cast<unsigned char *>(VirtualAlloc(nullptr, 3 * size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!memory) { result.errors = 1; return result; }
    DWORD old; const bool protectedPages = VirtualProtect(memory, size, PAGE_NOACCESS, &old) && VirtualProtect(memory + 2 * size, size, PAGE_NOACCESS, &old);
    if (!protectedPages) { VirtualFree(memory, 0, MEM_RELEASE); result.errors = 1; return result; }
    for (unsigned offset : {size, 2 * size - 64}) {
        VxMatrix *m = MVConstruct(memory + offset); std::memset(m, 0xff, 64); action(*m);
        const std::uint32_t zeros[16] = {}; ++result.samples;
        result.errors += std::memcmp(m, identity ? MVIdentityBits : zeros, 64) != 0;
        m->~VxMatrix();
    }
    VirtualFree(memory, 0, MEM_RELEASE); return result;
}
#endif

#ifndef VX_MATRIX_VALUE_PROBE
static_assert(sizeof(VxMatrix) == 64 && alignof(VxMatrix) == 4, "matrix64 layout");
static_assert(std::is_standard_layout<VxMatrix>::value && std::is_trivially_copyable<VxMatrix>::value, "matrix value storage");
static_assert(!std::is_polymorphic<VxMatrix>::value && std::is_trivially_destructible<VxMatrix>::value, "no vptr or owned lifetime");
static_assert(std::is_same<decltype(VxMatrix::Identity()), VxMatrix>::value, "SDL returns an independent matrix value");
static_assert(std::is_convertible<float (*)[4], VxMatrix>::value, "SDL preserves the existing implicit array constructor");
static void MVExpect(MVResult r, int samples) { EXPECT_EQ(r.samples, samples); EXPECT_EQ(r.errors, 0); }
TEST(MatrixValueReference, DefaultZeroInitialization) { MVExpect(MVDefault(), 16); }
TEST(MatrixValueReference, ArrayAndImplicitCopiesPreserveBitsAndOwnership) { MVExpect(MVArrayCopy(), 256); }
TEST(MatrixValueReference, ClearWritesAllPositiveZeros) { MVExpect(MVWrite(MVClear, false), 1024); }
TEST(MatrixValueReference, SetIdentityWritesAllSixteenEntries) { MVExpect(MVWrite(MVSetIdentity, true), 1024); }
TEST(MatrixValueReference, FreeIdentityWritesAllSixteenEntries) { MVExpect(MVWrite(MVFreeIdentity, true), 1024); }
TEST(MatrixValueReference, IdentityCopiesAreIndependent) { MVExpect(MVIdentityCopies(), 64); }
TEST(MatrixValueReference, RowAndPointerAccessShareRowMajorStorage) { MVExpect(MVAccess(), 16); }
TEST(MatrixValueReference, EqualityUsesNumericalValuesAcrossEveryLane) { MVExpect(MVCompare(), 1024); }
#if defined(_WIN32)
TEST(MatrixValueReference, ClearAndIdentityRespectPageBoundaries) {
    MVExpect(MVBoundary(MVClear, false), 2); MVExpect(MVBoundary(MVSetIdentity, true), 2); MVExpect(MVBoundary(MVFreeIdentity, true), 2);
}
#endif
#if defined(VX_SIMD_SSE)
TEST(MatrixValueReference, PackedIdentitySupportsFourByteAlignment) { MVExpect(MVWrite(MVSimdIdentity, true), 1024); }
#if defined(_WIN32)
TEST(MatrixValueReference, PackedIdentityRespectsPageBoundaries) { MVExpect(MVBoundary(MVSimdIdentity, true), 2); }
#endif
#endif
#endif
#undef MV_NOINLINE
