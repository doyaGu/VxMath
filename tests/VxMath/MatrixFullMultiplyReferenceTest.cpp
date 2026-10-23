#include <cmath>
#include <cstring>
#include <new>
#include <initializer_list>
#ifndef VX_MATRIX_FULL_PROBE
#include "VxMatrix.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef void (*M4Fn)(VxMatrix &, const VxMatrix &, const VxMatrix &);
struct M4Result { int samples = 0, errors = 0; };
static void M4Free(VxMatrix &r, const VxMatrix &a, const VxMatrix &b) { Vx3DMultiplyMatrix4(r, a, b); }
#if defined(VX_SIMD_SSE)
static void M4Packed(VxMatrix &r, const VxMatrix &a, const VxMatrix &b) { VxSIMDMultiplyMatrixM4(&r, &a, &b); }
#endif
static unsigned M4Next(unsigned &seed) { seed = seed * 1664525u + 1013904223u; return seed; }
static bool M4Equal(const VxMatrix &a, const VxMatrix &b) {
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c)
        if (!(a[r][c] == b[r][c] || (std::isnan(a[r][c]) && std::isnan(b[r][c])))) return false;
    return true;
}
static void M4Point(const VxMatrix &m, const double *point, double *out) {
    for (int c = 0; c < 4; ++c) {
        out[c] = 0;
        for (int r = 0; r < 4; ++r) out[c] += point[r] * m[r][c];
    }
}
static void M4Expected(VxMatrix &out, const VxMatrix &a, const VxMatrix &b) {
    // Recover the full homogeneous map by composing its four basis images.
    for (int axis = 0; axis < 4; ++axis) {
        double basis[4] = {}, middle[4], mapped[4]; basis[axis] = 1;
        M4Point(b, basis, middle); M4Point(a, middle, mapped);
        for (int c = 0; c < 4; ++c) out[axis][c] = float(mapped[c]);
    }
}
static bool M4Execute(VxMatrix &out, M4Fn fn, const VxMatrix &a, const VxMatrix &b, int mode) {
    VxMatrix left = a, right = b;
    switch (mode) {
    case 0: fn(out, left, right); break;
    case 1: fn(left, left, right); out = left; break;
    case 2: fn(right, left, right); out = right; break;
    case 3: fn(out, left, left); break;
    case 4: fn(left, left, left); out = left; break;
    }
    return (mode != 0 && mode != 2 && mode != 3 || std::memcmp(&left, &a, 64) == 0)
        && (mode == 2 || std::memcmp(&right, &b, 64) == 0);
}
static M4Result M4Literal(M4Fn fn) {
    const float aa[4][4] = {{2,0,0,1},{0,3,0,-2},{0,0,4,3},{5,7,11,2}};
    const float bb[4][4] = {{1,2,0,2},{0,1,0,3},{0,0,1,-1},{13,17,19,4}};
    const float rr[4][4] = {{12,20,22,1},{15,24,33,4},{-5,-7,-7,1},{46,79,120,44}};
    VxMatrix a, b, expected; std::memcpy(&a, aa, 64); std::memcpy(&b, bb, 64); std::memcpy(&expected, rr, 64);
    M4Result result;
    for (int mode = 0; mode < 3; ++mode) {
        VxMatrix out; ++result.samples; result.errors += !(M4Execute(out, fn, a, b, mode) && M4Equal(out, expected));
    }
    return result;
}
static M4Result M4Integers(M4Fn fn) {
    M4Result result; unsigned seed = 291;
    for (int sample = 0; sample < 256; ++sample) {
        VxMatrix a, b;
        for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) {
            a[r][c] = float(int(M4Next(seed) % 17) - 8); b[r][c] = float(int(M4Next(seed) % 17) - 8);
        }
        for (int mode = 0; mode < 5; ++mode) {
            VxMatrix expected, out; M4Expected(expected, a, mode >= 3 ? a : b);
            ++result.samples; result.errors += !(M4Execute(out, fn, a, b, mode) && M4Equal(out, expected));
        }
    }
    return result;
}
static M4Result M4BasisWeights(M4Fn fn) {
    M4Result result;
    // Every entry of both operands must influence the appropriate full row/column, including W.
    for (int side = 0; side < 2; ++side) for (int row = 0; row < 4; ++row) for (int col = 0; col < 4; ++col) {
        VxMatrix a, b, expected;
        for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) a[r][c] = b[r][c] = float(1 + r * 4 + c);
        if (side == 0) { a.Clear(); a[row][col] = 3; } else { b.Clear(); b[row][col] = 3; }
        M4Expected(expected, a, b);
        for (int mode = 0; mode < 3; ++mode) {
            VxMatrix out; ++result.samples; result.errors += !(M4Execute(out, fn, a, b, mode) && M4Equal(out, expected));
        }
    }
    return result;
}
static M4Result M4Projective(M4Fn fn) {
    VxMatrix p, view, out; p.Clear(); view.SetIdentity();
    p[0][0] = 2; p[1][1] = 3; p[2][2] = 2; p[2][3] = 1; p[3][2] = -2;
    view[3][0] = 5; view[3][1] = -7; view[3][2] = 11;
    const float coordinates[][4] = {{0,0,0,1},{1,2,3,1},{-3,4,2,1},{1,2,3,0}};
    M4Result result;
    for (int mode = 0; mode < 3; ++mode) {
        const bool inputs = M4Execute(out, fn, p, view, mode);
        for (const auto &point : coordinates) {
            double q[4] = {point[0],point[1],point[2],point[3]}, mid[4], expected[4], actual[4];
            M4Point(view, q, mid); M4Point(p, mid, expected); M4Point(out, q, actual);
            bool ok = inputs; for (int c = 0; c < 4; ++c) ok = ok && expected[c] == actual[c];
            ++result.samples; result.errors += !ok;
        }
    }
    return result;
}
static M4Result M4Numerics(M4Fn fn, int kind) {
    M4Result result;
    for (int row = 0; row < 4; ++row) for (int col = 0; col < 4; ++col) for (int rotate = 0; rotate < 4; ++rotate) {
        VxMatrix a, b; a.Clear(); b.Clear(); float x[4], y[4], expected;
        if (kind == 0) { x[0] = std::ldexp(1.0f,80); x[1] = 1; x[2] = -x[0]; x[3] = 1; for (float &v : y) v = 1; expected = 2; }
        else if (kind == 1) { x[0] = std::ldexp(1.0f,100); x[1] = -x[0]; x[2] = 1; x[3] = 2; y[0] = y[1] = std::ldexp(1.0f,40); y[2] = y[3] = 1; expected = 3; }
        else { for (int k = 0; k < 4; ++k) { x[k] = std::ldexp(1.0f,-100); y[k] = std::ldexp(1.0f,-51); } expected = std::ldexp(1.0f,-149); }
        for (int k = 0; k < 4; ++k) { const int index = (k + rotate) % 4; a[index][col] = x[k]; b[row][index] = y[k]; }
        for (int mode = 0; mode < 3; ++mode) {
            VxMatrix out; ++result.samples; result.errors += !(M4Execute(out, fn, a, b, mode) && out[row][col] == expected);
        }
    }
    return result;
}
static M4Result M4Nonfinite(M4Fn fn) {
    M4Result result; const float infinity = HUGE_VALF;
    for (int col = 0; col < 4; ++col) for (int mode = 0; mode < 3; ++mode) for (int kind = 0; kind < 3; ++kind) {
        VxMatrix a, b, out; a.Clear(); b.Clear();
        for (int k = 0; k < 4; ++k) { a[k][col] = k == 0 ? infinity : kind == 2 && k == 1 ? -infinity : 1; b[0][k] = 1; }
        if (kind == 1) b[0][0] = -1;
        const bool inputs = M4Execute(out, fn, a, b, mode); const float value = out[0][col];
        const bool ok = kind == 2 ? std::isnan(value) : std::isinf(value) && std::signbit(value) == (kind == 1);
        ++result.samples; result.errors += !(inputs && ok);
    }
    return result;
}
static M4Result M4Footprint(M4Fn fn) {
    M4Result result; VxMatrix a, b, expected;
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) { a[r][c] = float(r + c + 1); b[r][c] = float(4 * r - c); }
    M4Expected(expected, a, b);
    for (int shift = 0; shift < 16; shift += 4) {
        alignas(16) unsigned char bytes[320], before[320]; std::memset(bytes, 0xa5, sizeof(bytes));
        VxMatrix *pa = new(bytes + 16 + shift) VxMatrix(a), *pb = new(bytes + 112 + shift) VxMatrix(b), *pr = new(bytes + 208 + shift) VxMatrix;
        std::memcpy(before, bytes, sizeof(bytes)); fn(*pr, *pa, *pb); bool ok = M4Equal(*pr, expected);
        for (int i = 0; i < 320; ++i) if (i < 208 + shift || i >= 272 + shift) ok = ok && bytes[i] == before[i];
        ++result.samples; result.errors += !ok; pa->~VxMatrix(); pb->~VxMatrix(); pr->~VxMatrix();
    }
#if defined(_WIN32)
    SYSTEM_INFO info; GetSystemInfo(&info); const unsigned size = info.dwPageSize;
    unsigned char *memory = static_cast<unsigned char *>(VirtualAlloc(nullptr, 9 * size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!memory) { ++result.errors; return result; } DWORD old; bool ok = true;
    for (unsigned page : {0u,2u,3u,5u,6u,8u}) ok = ok && VirtualProtect(memory + page * size, size, PAGE_NOACCESS, &old);
    if (!ok) { VirtualFree(memory, 0, MEM_RELEASE); ++result.errors; return result; }
    for (int i = 0; i < 8; ++i) {
        VxMatrix *pa = new(memory + size + (i & 1 ? size - 64 : 0)) VxMatrix(a), *pb = new(memory + 4 * size + (i & 2 ? size - 64 : 0)) VxMatrix(b), *pr = new(memory + 7 * size + (i & 4 ? size - 64 : 0)) VxMatrix;
        ok = VirtualProtect(memory + size, size, PAGE_READONLY, &old) && VirtualProtect(memory + 4 * size, size, PAGE_READONLY, &old);
        if (!ok) { ++result.errors; break; } fn(*pr, *pa, *pb); ++result.samples; result.errors += !M4Equal(*pr, expected);
        VirtualProtect(memory + size, size, PAGE_READWRITE, &old); VirtualProtect(memory + 4 * size, size, PAGE_READWRITE, &old);
        pa->~VxMatrix(); pb->~VxMatrix(); pr->~VxMatrix();
    }
    VirtualFree(memory, 0, MEM_RELEASE);
#endif
    return result;
}
#ifndef VX_MATRIX_FULL_PROBE
static void M4Expect(M4Result r, int samples) { EXPECT_EQ(r.samples, samples); EXPECT_EQ(r.errors, 0); }
TEST(MatrixFullMultiplyReference, NonCommutingHomogeneousLiteralAndAliases) { M4Expect(M4Literal(M4Free), 3); }
TEST(MatrixFullMultiplyReference, IntegerBasisCompositionAndAllAliases) { M4Expect(M4Integers(M4Free), 1280); }
TEST(MatrixFullMultiplyReference, EveryInputFieldIncludesProjectiveWeights) { M4Expect(M4BasisWeights(M4Free), 96); }
TEST(MatrixFullMultiplyReference, ProjectionCompositionPreservesHomogeneousPointsAndDirections) { M4Expect(M4Projective(M4Free), 12); }
TEST(MatrixFullMultiplyReference, CancellationAcrossAllFourTermsAndOutputLanes) { M4Expect(M4Numerics(M4Free, 0), 192); }
TEST(MatrixFullMultiplyReference, FiniteResultsSurviveProductOverflow) { M4Expect(M4Numerics(M4Free, 1), 192); }
TEST(MatrixFullMultiplyReference, FourProductTailsRecoverSubnormalResult) { M4Expect(M4Numerics(M4Free, 2), 192); }
TEST(MatrixFullMultiplyReference, NonfiniteArithmeticRetainsSignsAndNaN) { M4Expect(M4Nonfinite(M4Free), 36); }
TEST(MatrixFullMultiplyReference, UnalignedAndGuardedReadWriteFootprints) {
#if defined(_WIN32)
    M4Expect(M4Footprint(M4Free), 12);
#else
    M4Expect(M4Footprint(M4Free), 4);
#endif
}
#if defined(VX_SIMD_SSE)
TEST(MatrixFullMultiplyReference, PackedIntegerBasisAndAllAliases) { M4Expect(M4Integers(M4Packed), 1280); }
TEST(MatrixFullMultiplyReference, PackedAllSixteenInputWeights) { M4Expect(M4BasisWeights(M4Packed), 96); }
TEST(MatrixFullMultiplyReference, PackedFourTermCancellationOverflowAndSubnormal) { for (int k = 0; k < 3; ++k) M4Expect(M4Numerics(M4Packed, k), 192); }
TEST(MatrixFullMultiplyReference, PackedNonfiniteArithmetic) { M4Expect(M4Nonfinite(M4Packed), 36); }
TEST(MatrixFullMultiplyReference, PackedReadWriteFootprint) {
#if defined(_WIN32)
    M4Expect(M4Footprint(M4Packed), 12);
#else
    M4Expect(M4Footprint(M4Packed), 4);
#endif
}
#endif
#endif
