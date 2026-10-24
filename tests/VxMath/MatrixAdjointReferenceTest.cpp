#include <cmath>
#include <cstring>
#include <cstdint>
#include <new>
#ifndef VX_MATRIX_ADJOINT_PROBE
#include "VxMatrix.h"
#include <gtest/gtest.h>
#include <type_traits>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef void (*MAFn)(const VxMatrix &, VxMatrix &);
struct MAResult { int calls = 0, errors = 0, inputErrors = 0, untouchedErrors = 0; };
static bool MAEqual(float a, float b) { return a == b || (std::isnan(a) && std::isnan(b)); }
static unsigned MANext(unsigned &seed) { seed = seed * 1664525u + 1013904223u; return seed; }
static void MASeed(VxMatrix &m) {
    const unsigned words[16] = {0x80000000u,0x7fc12345u,0x7f800001u,0xff800000u,0xdeadbeefu,0x3f800000u,1u,0x80000000u,0,0xffffffffu,0x3f000000u,0x7fc54321u,1u,0x80000000u,0x7f800001u,0xdeadbeefu};
    std::memcpy(&m, words, 64);
}
static void MALinear(VxMatrix &m) {
    MASeed(m);
    for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) m[r][c] = 0;
}
static void MAFill(VxMatrix &m) {
    const float values[3][3] = {{1,2,3},{0,4,5},{1,0,6}}; MALinear(m);
    for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) m[r][c] = values[r][c];
}
static void MAExpected(const VxMatrix &m, VxMatrix &out) {
    // Independently form each signed minor by deleting its row and column.
    for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) {
        int rows[2], columns[2], nr = 0, nc = 0;
        for (int i = 0; i < 3; ++i) { if (i != r) rows[nr++] = i; if (i != c) columns[nc++] = i; }
        const double positive = double(m[rows[0]][columns[0]]) * m[rows[1]][columns[1]];
        const double negative = double(m[rows[0]][columns[1]]) * m[rows[1]][columns[0]];
        out[r][c] = float((r + c) & 1 ? negative - positive : positive - negative);
    }
}
static void MAObserve(MAResult &result, MAFn fn, const VxMatrix &m, const VxMatrix &expected, bool alias) {
    VxMatrix input = m, separate; MASeed(separate);
    VxMatrix &out = alias ? input : separate;
    unsigned beforeInput[16], beforeOutput[16];
    std::memcpy(beforeInput, &input, 64); std::memcpy(beforeOutput, &out, 64);
    fn(input, out); ++result.calls;
    bool numeric = true, untouched = true;
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) {
        if (r < 3 && c < 3) numeric = numeric && MAEqual(out[r][c], expected[r][c]);
        else untouched = untouched && std::memcmp(&out[r][c], beforeOutput + 4 * r + c, 4) == 0;
    }
    result.errors += !numeric; result.untouchedErrors += !untouched;
    result.inputErrors += !alias && std::memcmp(beforeInput, &input, 64) != 0;
}
static MAResult MALiteral(MAFn fn) {
    const float values[3][3] = {{24,5,-4},{-12,3,2},{-2,-5,4}};
    MAResult result; VxMatrix m, expected; MAFill(m);
    for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) expected[r][c] = values[r][c];
    MAObserve(result, fn, m, expected, false); MAObserve(result, fn, m, expected, true); return result;
}
static MAResult MAIntegers(MAFn fn) {
    MAResult result; unsigned seed = 305;
    for (int sample = 0; sample < 512; ++sample) {
        VxMatrix m, expected; MALinear(m);
        for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) m[r][c] = float(int(MANext(seed) % 7) - 3);
        MAExpected(m, expected); MAObserve(result, fn, m, expected, false); MAObserve(result, fn, m, expected, true);
    }
    return result;
}
static MAResult MARanks(MAFn fn) {
    MAResult result;
    for (int rank = 0; rank < 3; ++rank) {
        VxMatrix m, expected; MALinear(m);
        const int u[] = {1,2,-3}, v[] = {2,-1,3};
        for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c)
            m[r][c] = rank == 0 ? 0 : rank == 1 ? float(u[r] * v[c]) : float(r == 2 ? (c + 1) + (c + 4) : c + 1 + 3 * r);
        MAExpected(m, expected); MAObserve(result, fn, m, expected, false); MAObserve(result, fn, m, expected, true);
    }
    return result;
}
static MAResult MAIgnored(MAFn fn) {
    const unsigned words[] = {0,0x80000000u,0x7f800000u,0xff800000u,0x7fc12345u,0x7f800001u,1u,0xdeadbeefu};
    const int fields[] = {3,7,11,12,13,14,15}; MAResult result;
    for (unsigned word : words) for (int field : fields) {
        VxMatrix m, expected; MAFill(m); MAExpected(m, expected);
        std::memcpy(reinterpret_cast<unsigned char *>(&m) + 4 * field, &word, 4);
        MAObserve(result, fn, m, expected, false); MAObserve(result, fn, m, expected, true);
    }
    return result;
}
static void MAMinor(VxMatrix &m, int r, int c, float a, float b, float d, float e) {
    MALinear(m); int rows[2], columns[2], nr = 0, nc = 0;
    for (int i = 0; i < 3; ++i) { if (i != r) rows[nr++] = i; if (i != c) columns[nc++] = i; }
    m[rows[0]][columns[0]] = a; m[rows[0]][columns[1]] = b;
    m[rows[1]][columns[0]] = d; m[rows[1]][columns[1]] = e;
}
static void MATarget(MAResult &result, MAFn fn, const VxMatrix &m, int r, int c, float value) {
    VxMatrix expected; MALinear(expected); expected[r][c] = (r + c) & 1 ? -value : value;
    MAObserve(result, fn, m, expected, false); MAObserve(result, fn, m, expected, true);
}
static MAResult MACancellation(MAFn fn) {
    MAResult result;
    for (int e = 1; e <= 23; ++e) for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) {
        const float n = std::ldexp(1.0f, e); VxMatrix m;
        MAMinor(m, r, c, n, n - 1, n + 1, n); MATarget(result, fn, m, r, c, 1);
    }
    return result;
}
static MAResult MAOverflow(MAFn fn) {
    MAResult result;
    for (int kind = 0; kind < 2; ++kind) for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) for (int sign = 0; sign < 4; ++sign) {
        const float a = std::ldexp(1.0f, 80), d = std::ldexp(1.0f, 60), b = a + (kind ? std::ldexp(1.0f, 57) : 0);
        const float s = sign & 1 ? -1.0f : 1.0f, t = sign & 2 ? -1.0f : 1.0f;
        VxMatrix m; MAMinor(m, r, c, s * a, s * b, t * d, t * d);
        MATarget(result, fn, m, r, c, kind ? -s * t * std::ldexp(1.0f, 117) : 0);
    }
    return result;
}
static MAResult MASubnormal(MAFn fn) {
    MAResult result; const float a = std::ldexp(1.0f, -75);
    for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) for (int sign = 0; sign < 4; ++sign) {
        const float s = sign & 1 ? -1.0f : 1.0f, t = sign & 2 ? -1.0f : 1.0f;
        VxMatrix m; MAMinor(m, r, c, s * a, -s * a, t * a, t * a);
        MATarget(result, fn, m, r, c, s * t * std::ldexp(1.0f, -149));
    }
    return result;
}
static MAResult MAScales(MAFn fn) {
    const int exponents[] = {-149,-100,-60,0,60,100,127}; MAResult result;
    for (int x : exponents) for (int y : exponents) for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) for (int sign = -1; sign <= 1; sign += 2) {
        VxMatrix m; MAMinor(m, r, c, std::ldexp(float(sign), x), 0, 0, std::ldexp(1.0f, y));
        MATarget(result, fn, m, r, c, float(std::ldexp(double(sign), x + y)));
    }
    return result;
}
static MAResult MANonfinite(MAFn fn) {
    const unsigned words[] = {0x7f800000u,0xff800000u,0x7fc12345u}; MAResult result;
    for (unsigned word : words) for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) {
        VxMatrix m, expected; MAFill(m); std::memcpy(&m[r][c], &word, 4); MAExpected(m, expected);
        MAObserve(result, fn, m, expected, false); MAObserve(result, fn, m, expected, true);
    }
    VxMatrix m, expected; MALinear(m); const unsigned nan = 0x7fc12345u;
    for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) std::memcpy(&m[r][c], &nan, 4);
    MAExpected(m, expected); MAObserve(result, fn, m, expected, false); MAObserve(result, fn, m, expected, true);
    return result;
}
static MAResult MAAlignment(MAFn fn) {
    MAResult result; VxMatrix m, expected; MAFill(m); MAExpected(m, expected);
    for (int source = 0; source < 4; ++source) for (int target = 0; target < 5; ++target) {
        alignas(16) unsigned char bytes[320], before[320]; std::memset(bytes, 0xa5, sizeof(bytes));
        const int at = target == 4 ? 16 + 4 * source : 144 + 4 * target;
        VxMatrix *in = new(bytes + 16 + 4 * source) VxMatrix(m), *out = in;
        if (target != 4) { out = new(bytes + at) VxMatrix; MASeed(*out); }
        std::memcpy(before, bytes, sizeof(bytes)); fn(*in, *out); ++result.calls; bool numeric = true, untouched = true;
        for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) numeric = numeric && MAEqual((*out)[r][c], expected[r][c]);
        for (int i = 0; i < 320; ++i) {
            const int offset = i - at;
            const bool written = offset >= 0 && offset < 44 && offset % 16 < 12;
            if (!written) untouched = untouched && bytes[i] == before[i];
        }
        result.errors += !numeric; result.untouchedErrors += !untouched;
        if (target != 4) out->~VxMatrix(); in->~VxMatrix();
    }
    return result;
}
static MAResult MAFootprint(MAFn fn) {
    MAResult result; VxMatrix m, expected; MAFill(m); MAExpected(m, expected);
#if defined(_WIN32)
    SYSTEM_INFO info; GetSystemInfo(&info); const unsigned size = info.dwPageSize;
    unsigned char *memory = static_cast<unsigned char *>(VirtualAlloc(nullptr, 6 * size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!memory) { ++result.errors; return result; } DWORD old;
    for (int layout = 0; layout < 6; ++layout) {
        const bool alias = layout >= 4; const int sp = alias ? layout - 4 : layout & 1, dp = layout >> 1;
        VxMatrix *in = new(memory + size + (sp ? size - 44 : 0)) VxMatrix(m), *out = in;
        if (!alias) { out = new(memory + 4 * size + (dp ? size - 44 : 0)) VxMatrix; MASeed(*out); }
        unsigned beforeInput[16], beforeOutput[16]; std::memcpy(beforeInput, in, 64); std::memcpy(beforeOutput, out, 64);
        bool ok = true;
        const unsigned guards[] = {0,2,3,5}; for (unsigned page : guards) ok = ok && VirtualProtect(memory + page * size, size, PAGE_NOACCESS, &old);
        if (!alias) ok = ok && VirtualProtect(memory + size, size, PAGE_READONLY, &old);
        if (!ok) { ++result.errors; break; }
        fn(*in, *out); ++result.calls; VirtualProtect(memory, 6 * size, PAGE_READWRITE, &old);
        bool numeric = true, untouched = true;
        for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c)
            if (r < 3 && c < 3) numeric = numeric && MAEqual((*out)[r][c], expected[r][c]);
            else untouched = untouched && std::memcmp(&(*out)[r][c], beforeOutput + 4 * r + c, 4) == 0;
        result.errors += !numeric; result.untouchedErrors += !untouched;
        result.inputErrors += !alias && std::memcmp(beforeInput, in, 64) != 0;
        if (!alias) out->~VxMatrix(); in->~VxMatrix();
    }
    VirtualFree(memory, 0, MEM_RELEASE);
#else
    MAObserve(result, fn, m, expected, false); MAObserve(result, fn, m, expected, true);
#endif
    return result;
}
#ifndef VX_MATRIX_ADJOINT_PROBE
static void MAExpect(MAResult result, int calls) { EXPECT_EQ(result.calls, calls); EXPECT_EQ(result.errors, 0); EXPECT_EQ(result.inputErrors, 0); EXPECT_EQ(result.untouchedErrors, 0); }
TEST(MatrixAdjointReference, SignatureRetainsInputFirstOutputSecond) { static_assert(std::is_same<decltype(&Vx3DMatrixAdjoint), void (*)(const VxMatrix &, VxMatrix &)>::value, "adjoint signature"); }
TEST(MatrixAdjointReference, LiteralCofactorDirectionAndSnapshotAlias) { MAExpect(MALiteral(Vx3DMatrixAdjoint), 2); }
TEST(MatrixAdjointReference, IndependentSignedMinorsAndIntegerAliases) { MAExpect(MAIntegers(Vx3DMatrixAdjoint), 1024); }
TEST(MatrixAdjointReference, ZeroRankOneAndRankTwoRequireNoInverse) { MAExpect(MARanks(Vx3DMatrixAdjoint), 6); }
TEST(MatrixAdjointReference, SevenIgnoredInputAndPreservedOutputFields) { MAExpect(MAIgnored(Vx3DMatrixAdjoint), 112); }
TEST(MatrixAdjointReference, SmallCofactorsSurviveProductCancellation) { MAExpect(MACancellation(Vx3DMatrixAdjoint), 414); }
TEST(MatrixAdjointReference, FiniteCofactorsSurviveOverflowingFloatProducts) { MAExpect(MAOverflow(Vx3DMatrixAdjoint), 144); }
TEST(MatrixAdjointReference, TwoHalfSubnormalProductsRetainTheirDifference) { MAExpect(MASubnormal(Vx3DMatrixAdjoint), 72); }
TEST(MatrixAdjointReference, SignedPowersOfTwoCoverTrueUnderflowAndOverflow) { MAExpect(MAScales(Vx3DMatrixAdjoint), 1764); }
TEST(MatrixAdjointReference, InfinityAndNaNRetainMinorArithmeticClassification) { MAExpect(MANonfinite(Vx3DMatrixAdjoint), 56); }
TEST(MatrixAdjointReference, IndependentFourByteAlignmentsAndCanaries) { MAExpect(MAAlignment(Vx3DMatrixAdjoint), 20); }
TEST(MatrixAdjointReference, ReadOnlySourceAnd44ByteGuardedSourceDestinationAndAlias) {
#if defined(_WIN32)
    MAExpect(MAFootprint(Vx3DMatrixAdjoint), 6);
#else
    MAExpect(MAFootprint(Vx3DMatrixAdjoint), 2);
#endif
}
#endif
