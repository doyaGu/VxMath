#include <cmath>
#include <cstring>
#include <cfloat>
#include <new>
#ifndef VX_MATRIX_NORM_PROBE
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

typedef float (*MNFn)(const VxMatrix &, bool);
struct MNResult { int calls = 0, errors = 0, inputErrors = 0; };
static void MNObserve(MNResult &result, MNFn fn, const VxMatrix &m, bool one, float expected) {
    unsigned before[16]; std::memcpy(before, &m, 64);
    const float actual = fn(m, one);
    ++result.calls;
    result.errors += actual != expected || (expected == 0 && std::signbit(actual));
    result.inputErrors += std::memcmp(before, &m, 64) != 0;
}
static void MNFill(VxMatrix &m) {
    const float values[3][3] = {{1,-2,3},{4,-5,6},{-7,8,-9}};
    m.Clear();
    for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) m[r][c] = values[r][c];
}
static unsigned MNNext(unsigned &seed) { seed = seed * 1664525u + 1013904223u; return seed; }
static MNResult MNLiteral(MNFn fn) {
    MNResult result; VxMatrix m; MNFill(m);
    MNObserve(result, fn, m, true, 18); MNObserve(result, fn, m, false, 24);
    m.Clear(); MNObserve(result, fn, m, true, 0); MNObserve(result, fn, m, false, 0);
    return result;
}
static MNResult MNElementary(MNFn fn) {
    const int exponents[] = {-149,-126,-100,-24,0,24,64,100,127};
    MNResult result;
    for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) for (int e : exponents) for (int sign = -1; sign <= 1; sign += 2) {
        VxMatrix m; m.Clear(); m[r][c] = std::ldexp(float(sign), e);
        MNObserve(result, fn, m, true, std::ldexp(1.0f, e));
        MNObserve(result, fn, m, false, std::ldexp(1.0f, e));
    }
    return result;
}
static MNResult MNInduced(MNFn fn) {
    MNResult result; unsigned seed = 304;
    for (int sample = 0; sample < 512; ++sample) {
        int a[3][3]; VxMatrix m; m.Clear();
        for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) m[r][c] = float(a[r][c] = int(MNNext(seed) % 127) - 63);
        // The induced infinity norm is attained at a vertex of the unit cube.
        int infinity = 0;
        for (int vertex = 0; vertex < 8; ++vertex) for (int r = 0; r < 3; ++r) {
            int mapped = 0;
            for (int c = 0; c < 3; ++c) mapped += a[r][c] * (vertex & (1 << c) ? 1 : -1);
            const int magnitude = mapped < 0 ? -mapped : mapped;
            if (magnitude > infinity) infinity = magnitude;
        }
        // The induced one norm is attained at a signed coordinate basis vector.
        int one = 0;
        for (int basis = 0; basis < 3; ++basis) {
            int length = 0;
            for (int r = 0; r < 3; ++r) length += a[r][basis] < 0 ? -a[r][basis] : a[r][basis];
            if (length > one) one = length;
        }
        MNObserve(result, fn, m, true, float(one)); MNObserve(result, fn, m, false, float(infinity));
    }
    return result;
}
static MNResult MNSymmetries(MNFn fn) {
    const int permutations[6][3] = {{0,1,2},{1,2,0},{2,0,1},{2,1,0},{1,0,2},{0,2,1}};
    MNResult result; VxMatrix original; MNFill(original);
    for (int p = 0; p < 6; ++p) for (int q = 0; q < 6; ++q) for (int sign = 0; sign < 64; ++sign) {
        VxMatrix m; m.Clear();
        for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c)
            m[r][c] = original[permutations[p][r]][permutations[q][c]] * (sign & (1 << r) ? -1.0f : 1.0f) * (sign & (8 << c) ? -1.0f : 1.0f);
        MNObserve(result, fn, m, true, 18); MNObserve(result, fn, m, false, 24);
    }
    return result;
}
static MNResult MNIgnored(MNFn fn) {
    const unsigned words[] = {0u,0x80000000u,0x7f800000u,0xff800000u,0x7fc12345u,0x7f7fffffu,1u,0xff7fffffu};
    const int indices[] = {3,7,11,12,13,14,15}; MNResult result;
    for (unsigned word : words) for (int index : indices) {
        VxMatrix m; MNFill(m); std::memcpy(reinterpret_cast<unsigned char *>(&m) + 4 * index, &word, 4);
        MNObserve(result, fn, m, true, 18); MNObserve(result, fn, m, false, 24);
    }
    return result;
}
static MNResult MNSmallTerms(MNFn fn) {
    const int exponents[] = {-120,-64,0,60,100}; MNResult result;
    for (int e : exponents) for (int position = 0; position < 3; ++position) for (int axis = 0; axis < 3; ++axis) for (int signs = 0; signs < 8; ++signs) for (int mode = 0; mode < 2; ++mode) {
        VxMatrix m; m.Clear();
        for (int term = 0; term < 3; ++term) {
            const float value = std::ldexp(signs & (1 << term) ? -1.0f : 1.0f, term == position ? e : e - 24);
            if (mode) m[term][axis] = value; else m[axis][term] = value;
        }
        MNObserve(result, fn, m, mode != 0, float(std::ldexp(1.0, e) + std::ldexp(1.0, e - 23)));
    }
    return result;
}
static MNResult MNScales(MNFn fn) {
    const int exponents[] = {-149,-148,-126,-100,0,100,126,127}; MNResult result;
    for (int e : exponents) for (int axis = 0; axis < 3; ++axis) for (int signs = 0; signs < 8; ++signs) for (int mode = 0; mode < 2; ++mode) {
        VxMatrix m; m.Clear();
        for (int term = 0; term < 3; ++term) {
            const float value = std::ldexp(signs & (1 << term) ? -1.0f : 1.0f, e);
            if (mode) m[term][axis] = value; else m[axis][term] = value;
        }
        MNObserve(result, fn, m, mode != 0, float(std::ldexp(3.0, e)));
    }
    return result;
}
static MNResult MNNonfinite(MNFn fn) {
    MNResult result;
    for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) {
        for (int sign = -1; sign <= 1; sign += 2) {
            VxMatrix m; m.Clear(); m[r][c] = sign * HUGE_VALF;
            MNObserve(result, fn, m, true, HUGE_VALF); MNObserve(result, fn, m, false, HUGE_VALF);
        }
        VxMatrix m;
        for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) m[i][j] = 1;
        const unsigned nan = 0x7fc12345u; std::memcpy(&m[r][c], &nan, 4);
        MNObserve(result, fn, m, true, 3); MNObserve(result, fn, m, false, 3);
    }
    VxMatrix m; const unsigned nan = 0x7fc12345u;
    for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) std::memcpy(&m[r][c], &nan, 4);
    MNObserve(result, fn, m, true, 0); MNObserve(result, fn, m, false, 0);
    return result;
}
static MNResult MNFootprint(MNFn fn) {
    MNResult result; VxMatrix original; MNFill(original);
#if defined(_WIN32)
    SYSTEM_INFO info; GetSystemInfo(&info); const unsigned size = info.dwPageSize;
    unsigned char *memory = static_cast<unsigned char *>(VirtualAlloc(nullptr, 3 * size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!memory) { ++result.errors; return result; }
    DWORD old;
    for (int position = 0; position < 2; ++position) {
        VxMatrix *m = new(memory + size + (position ? size - 44 : 0)) VxMatrix(original);
        const bool ok = VirtualProtect(memory, size, PAGE_NOACCESS, &old) && VirtualProtect(memory + size, size, PAGE_READONLY, &old) && VirtualProtect(memory + 2 * size, size, PAGE_NOACCESS, &old);
        if (!ok) { ++result.errors; break; }
        const float one = fn(*m, true), infinity = fn(*m, false);
        result.calls += 2; result.errors += one != 18 || infinity != 24;
        VirtualProtect(memory, 3 * size, PAGE_READWRITE, &old);
        result.inputErrors += std::memcmp(m, &original, 64) != 0;
        m->~VxMatrix();
    }
    VirtualFree(memory, 0, MEM_RELEASE);
#else
    alignas(16) unsigned char bytes[96];
    for (int shift = 0; shift < 16; shift += 4) {
        VxMatrix *m = new(bytes + shift) VxMatrix(original);
        MNObserve(result, fn, *m, true, 18); MNObserve(result, fn, *m, false, 24);
        m->~VxMatrix();
    }
#endif
    return result;
}
#ifndef VX_MATRIX_NORM_PROBE
static void MNExpect(MNResult result, int calls) {
    EXPECT_EQ(result.calls, calls); EXPECT_EQ(result.errors, 0); EXPECT_EQ(result.inputErrors, 0);
}
TEST(MatrixNormReference, SignatureRemainsScalarHelperWithBooleanSelector) {
    static_assert(std::is_same<decltype(&Vx3DMatrixNorm), float (*)(const VxMatrix &, bool)>::value, "norm signature");
}
TEST(MatrixNormReference, DistinctOneAndInfinityNormAndPositiveZero) { MNExpect(MNLiteral(Vx3DMatrixNorm), 4); }
TEST(MatrixNormReference, EveryLinearEntryAndSignAcrossExponentRange) { MNExpect(MNElementary(Vx3DMatrixNorm), 324); }
TEST(MatrixNormReference, IndependentInducedNormsOnCubeVerticesAndBasisVectors) { MNExpect(MNInduced(Vx3DMatrixNorm), 1024); }
TEST(MatrixNormReference, RowAndColumnPermutationsAndIndependentSignChanges) { MNExpect(MNSymmetries(Vx3DMatrixNorm), 4608); }
TEST(MatrixNormReference, AllSevenNonlinearFieldsAreIgnored) { MNExpect(MNIgnored(Vx3DMatrixNorm), 112); }
TEST(MatrixNormReference, SmallAbsoluteTermsSurviveUntilFinalRounding) { MNExpect(MNSmallTerms(Vx3DMatrixNorm), 720); }
TEST(MatrixNormReference, SubnormalAndLargeScalesIncludingTrueOverflow) { MNExpect(MNScales(Vx3DMatrixNorm), 384); }
TEST(MatrixNormReference, OriginalUnorderedSumAndInfinitySelection) { MNExpect(MNNonfinite(Vx3DMatrixNorm), 56); }
TEST(MatrixNormReference, ReadOnlyInputAtLeadingAnd44ByteTrailingGuard) {
#if defined(_WIN32)
    MNExpect(MNFootprint(Vx3DMatrixNorm), 4);
#else
    MNExpect(MNFootprint(Vx3DMatrixNorm), 8);
#endif
}
#endif
