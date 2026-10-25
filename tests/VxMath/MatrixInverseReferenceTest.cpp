#include <cmath>
#include <cstring>
#include <new>
#include <initializer_list>
#ifndef VX_MATRIX_INVERSE_PROBE
#include "VxMatrix.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef void (*MIFn)(VxMatrix &, const VxMatrix &);
struct MIResult { int samples = 0, errors = 0; };
static void MIFree(VxMatrix &out, const VxMatrix &in) { Vx3DInverseMatrix(out, in); }
#if defined(VX_SIMD_SSE)
static void MIPacked(VxMatrix &out, const VxMatrix &in) { VxSIMDInverseAffineMatrix(&out, &in); }
#endif
static unsigned MINext(unsigned &seed) { seed = seed * 1664525u + 1013904223u; return seed; }
static bool MIEqual(const VxMatrix &a, const VxMatrix &b) {
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) if (a[r][c] != b[r][c]) return false;
    return true;
}
static bool MIExecute(VxMatrix &out, MIFn fn, const VxMatrix &in, bool alias) {
    VxMatrix input = in; std::memset(&out, 0xa5, 64);
    if (alias) { fn(input, input); out = input; return true; }
    fn(out, input); return std::memcmp(&input, &in, 64) == 0;
}
static void MIKnown(VxMatrix &in, VxMatrix &expected, unsigned &seed) {
    int v[9]; for (int &value : v) value = int(MINext(seed) % 9) - 4;
    const double a = v[0], b = v[1], c = v[2], d = v[3], e = v[4], f = v[5];
    in.SetIdentity(); expected.SetIdentity();
    in[0][0] = 1; in[0][1] = float(d); in[0][2] = float(e);
    in[1][0] = float(a); in[1][1] = float(a*d+1); in[1][2] = float(a*e+f);
    in[2][0] = float(b); in[2][1] = float(b*d+c); in[2][2] = float(b*e+c*f+1);
    // Undo two elementary shear maps on basis points, rather than repeating a cofactor inverse.
    for (int axis = 0; axis < 3; ++axis) {
        double p[3] = {}; p[axis] = 1;
        const double u0 = p[0], u1 = p[1]-d*p[0], u2 = p[2]-f*p[1]+(d*f-e)*p[0];
        expected[axis][0] = float(u0-a*u1+(a*c-b)*u2);
        expected[axis][1] = float(u1-c*u2); expected[axis][2] = float(u2);
    }
    if (seed & 0x10000) {
        for (int j = 0; j < 3; ++j) in[0][j] = -in[0][j];
        for (int j = 0; j < 3; ++j) expected[j][0] = -expected[j][0];
    }
    for (int c0 = 0; c0 < 3; ++c0) in[3][c0] = float(v[6+c0]);
    for (int c0 = 0; c0 < 3; ++c0) {
        double value = 0; for (int r = 0; r < 3; ++r) value -= double(in[3][r])*expected[r][c0];
        expected[3][c0] = float(value);
    }
}
static MIResult MIIntegers(MIFn fn, bool alias) {
    MIResult result; unsigned seed = 292;
    for (int i = 0; i < 256; ++i) {
        VxMatrix in, expected, actual; MIKnown(in, expected, seed);
        ++result.samples; result.errors += !(MIExecute(actual, fn, in, alias) && MIEqual(actual, expected));
    }
    return result;
}
static MIResult MILiteral(MIFn fn) {
    const float input[4][4] = {{2,1,0,0},{0,4,1,0},{0,0,8,0},{3,-5,7,1}};
    const float inverse[4][4] = {{0.5f,-0.125f,0.015625f,0},{0,0.25f,-0.03125f,0},{0,0,0.125f,0},{-1.5f,1.625f,-1.078125f,1}};
    VxMatrix in, expected; std::memcpy(&in, input, 64); std::memcpy(&expected, inverse, 64); MIResult result;
    for (bool alias : {false,true}) { VxMatrix actual; ++result.samples; result.errors += !(MIExecute(actual, fn, in, alias) && MIEqual(actual, expected)); }
    return result;
}
static MIResult MIIgnored(MIFn fn) {
    const unsigned patterns[] = {0,0x3f800000,0x40000000,0xc0400000,0x7f800000,0xff800000,0x7fc12345,0x80000000};
    MIResult result;
    for (unsigned pattern : patterns) for (int row = 0; row < 4; ++row) {
        unsigned seed = 292; VxMatrix in, expected; MIKnown(in, expected, seed); std::memcpy(&in[row][3], &pattern, 4);
        for (bool alias : {false,true}) { VxMatrix actual; ++result.samples; result.errors += !(MIExecute(actual, fn, in, alias) && MIEqual(actual, expected)); }
    }
    return result;
}
static MIResult MISingular(MIFn fn, bool cancellation = false) {
    MIResult result;
    for (int kind = cancellation ? 4 : 0; kind < (cancellation ? 5 : 4); ++kind) {
        VxMatrix in; in.SetIdentity(); in[3][0] = 3; in[3][1] = 5; in[3][2] = 7;
        if (kind == 0) for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) in[r][c] = 0;
        if (kind == 1) for (int c = 0; c < 3; ++c) in[1][c] = 0;
        if (kind == 2) for (int c = 0; c < 3; ++c) in[1][c] = in[0][c];
        if (kind == 3) for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) in[r][c] = float((r+1)*(c+1));
        if (kind == 4) { const float m[3][3] = {{4096,4095,4095},{4097,4096,4096},{8193,8191,8191}}; for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) in[r][c] = m[r][c]; }
        for (bool alias : {false,true}) {
            VxMatrix actual, expected; std::memset(&expected, 0xa5, 64); if (alias) expected = in;
            const bool ok = MIExecute(actual, fn, in, alias) && std::memcmp(&actual, &expected, 64) == 0;
            ++result.samples; result.errors += !ok;
        }
    }
    return result;
}
static MIResult MIScales(MIFn fn) {
    MIResult result;
    for (int exponent : {-120,-80,-30,0,30,80,120}) for (bool mixed : {false,true}) {
        VxMatrix in, expected; in.SetIdentity(); expected.SetIdentity();
        for (int axis = 0; axis < 3; ++axis) {
            const int e = mixed ? (axis == 0 ? exponent : axis == 1 ? -exponent : 0) : exponent;
            in[axis][axis] = std::ldexp(1.0f,e); in[3][axis] = std::ldexp(float(axis+1),e);
            expected[axis][axis] = std::ldexp(1.0f,-e); expected[3][axis] = -float(axis+1);
        }
        for (bool alias : {false,true}) { VxMatrix actual; ++result.samples; result.errors += !(MIExecute(actual, fn, in, alias) && MIEqual(actual, expected)); }
    }
    return result;
}
static MIResult MITinyAxis(MIFn fn) {
    MIResult result;
    for (int axis = 0; axis < 3; ++axis) {
        VxMatrix in, expected; in.SetIdentity(); expected.SetIdentity();
        in[axis][axis] = in[3][axis] = std::ldexp(1.0f,-149); expected[axis][axis] = HUGE_VALF; expected[3][axis] = -1;
        for (bool alias : {false,true}) { VxMatrix actual; ++result.samples; result.errors += !(MIExecute(actual, fn, in, alias) && MIEqual(actual, expected)); }
    }
    return result;
}
static MIResult MICancellation(MIFn fn) {
    MIResult result;
    for (int n : {4096,1048576}) {
        const float m[3][3] = {{float(n),float(n-1),float(n-1)},{float(n+1),float(n),float(n)},{float(n+1),float(n),float(n+1)}};
        const float v[3][3] = {{float(n),float(1-n),0},{float(-n-1),float(n+1),-1},{0,-1,1}};
        VxMatrix in, expected; in.SetIdentity(); expected.SetIdentity();
        for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) { in[r][c] = m[r][c]; expected[r][c] = v[r][c]; }
        in[3][0] = 1; in[3][1] = 2; in[3][2] = 3;
        expected[3][0] = float(n+2); expected[3][1] = float(-n); expected[3][2] = -1;
        for (bool alias : {false,true}) { VxMatrix actual; ++result.samples; result.errors += !(MIExecute(actual, fn, in, alias) && MIEqual(actual, expected)); }
    }
    return result;
}
static MIResult MITranslation(MIFn fn) {
    MIResult result;
    for (int kind = 0; kind < 2; ++kind) for (int axis = 0; axis < 3; ++axis) {
        VxMatrix base, inverse; base.SetIdentity(); inverse.SetIdentity();
        const int e = kind == 0 ? 0 : -20, t = 30;
        base[0][0] = std::ldexp(1.0f,e); base[1][0] = base[2][0] = -1;
        for (int r = 0; r < 3; ++r) inverse[r][0] = std::ldexp(1.0f,-e);
        base[3][0] = std::ldexp(1.0f,t); base[3][1] = std::ldexp(1.0f,e); base[3][2] = -base[3][0];
        inverse[3][0] = -1; inverse[3][1] = -base[3][1]; inverse[3][2] = -base[3][2];
        VxMatrix in, expected; in.SetIdentity(); expected.SetIdentity();
        for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) { in[(r+axis)%3][(c+axis)%3] = base[r][c]; expected[(r+axis)%3][(c+axis)%3] = inverse[r][c]; }
        for (int c = 0; c < 3; ++c) { in[3][(c+axis)%3] = base[3][c]; expected[3][(c+axis)%3] = inverse[3][c]; }
        for (bool alias : {false,true}) { VxMatrix actual; ++result.samples; result.errors += !(MIExecute(actual, fn, in, alias) && MIEqual(actual, expected)); }
    }
    return result;
}
static MIResult MIFootprint(MIFn fn) {
    unsigned seed = 292; VxMatrix in, expected; MIKnown(in, expected, seed); MIResult result;
    for (int shift = 0; shift < 16; shift += 4) {
        alignas(16) unsigned char bytes[224], before[224]; std::memset(bytes,0xa5,sizeof(bytes));
        VxMatrix *pm = new(bytes+16+shift) VxMatrix(in), *pr = new(bytes+128+shift) VxMatrix;
        std::memcpy(before,bytes,sizeof(bytes)); fn(*pr,*pm); bool ok = MIEqual(*pr,expected);
        for (int i = 0; i < 224; ++i) if (i < 128+shift || i >= 192+shift) ok = ok && bytes[i] == before[i];
        ++result.samples; result.errors += !ok; pm->~VxMatrix(); pr->~VxMatrix();
    }
#if defined(_WIN32)
    SYSTEM_INFO info; GetSystemInfo(&info); const unsigned size = info.dwPageSize;
    unsigned char *memory = static_cast<unsigned char *>(VirtualAlloc(nullptr,6*size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if (!memory) { ++result.errors; return result; } DWORD old; bool ok = true;
    for (unsigned page : {0u,2u,3u,5u}) ok = ok && VirtualProtect(memory+page*size,size,PAGE_NOACCESS,&old);
    if (!ok) { VirtualFree(memory,0,MEM_RELEASE); ++result.errors; return result; }
    for (int i = 0; i < 4; ++i) {
        VxMatrix *pm = new(memory+size+(i&1 ? size-64 : 0)) VxMatrix(in), *pr = new(memory+4*size+(i&2 ? size-64 : 0)) VxMatrix;
        if (!VirtualProtect(memory+size,size,PAGE_READONLY,&old)) { ++result.errors; break; }
        fn(*pr,*pm); ++result.samples; result.errors += !MIEqual(*pr,expected);
        VirtualProtect(memory+size,size,PAGE_READWRITE,&old); pm->~VxMatrix(); pr->~VxMatrix();
    }
    VirtualFree(memory,0,MEM_RELEASE);
#endif
    return result;
}
#ifndef VX_MATRIX_INVERSE_PROBE
static void MIExpect(MIResult result, int count) { EXPECT_EQ(result.samples,count); EXPECT_EQ(result.errors,0); }
TEST(MatrixInverseReference, NonOrthogonalLiteralAndInPlaceResult) { MIExpect(MILiteral(MIFree),2); }
TEST(MatrixInverseReference, KnownShearMapsWithReflections) { MIExpect(MIIntegers(MIFree,false),256); }
TEST(MatrixInverseReference, InPlaceKnownShearMaps) { MIExpect(MIIntegers(MIFree,true),256); }
TEST(MatrixInverseReference, FourthInputColumnIsIgnored) { MIExpect(MIIgnored(MIFree),64); }
TEST(MatrixInverseReference, SingularInputLeavesOutputBytesUnchanged) { MIExpect(MISingular(MIFree),8); }
TEST(MatrixInverseReference, ExactlyDependentLargeRowsLeaveOutputUnchanged) { MIExpect(MISingular(MIFree,true),2); }
TEST(MatrixInverseReference, UniformAndMixedScalesRemainInvertible) { MIExpect(MIScales(MIFree),28); }
TEST(MatrixInverseReference, OverflowingInverseScaleDoesNotDestroyFiniteTranslation) { MIExpect(MITinyAxis(MIFree),6); }
TEST(MatrixInverseReference, ExactNonzeroDeterminantSurvivesCancellation) { MIExpect(MICancellation(MIFree),4); }
TEST(MatrixInverseReference, TranslationUsesUnroundedCofactors) { MIExpect(MITranslation(MIFree),12); }
TEST(MatrixInverseReference, UnalignedAndReadonlyFootprints) {
#if defined(_WIN32)
    MIExpect(MIFootprint(MIFree),8);
#else
    MIExpect(MIFootprint(MIFree),4);
#endif
}
#if defined(VX_SIMD_SSE)
TEST(MatrixInverseReference, PackedShearMapsAndInPlaceResults) { for (bool alias : {false,true}) MIExpect(MIIntegers(MIPacked,alias),256); }
TEST(MatrixInverseReference, PackedFourthColumnAndSingularNoWrite) { MIExpect(MIIgnored(MIPacked),64); MIExpect(MISingular(MIPacked),8); MIExpect(MISingular(MIPacked,true),2); }
TEST(MatrixInverseReference, PackedScalesAndExactDeterminant) { MIExpect(MIScales(MIPacked),28); MIExpect(MITinyAxis(MIPacked),6); MIExpect(MICancellation(MIPacked),4); }
TEST(MatrixInverseReference, PackedTranslationCancellation) { MIExpect(MITranslation(MIPacked),12); }
TEST(MatrixInverseReference, PackedReadWriteFootprints) {
#if defined(_WIN32)
    MIExpect(MIFootprint(MIPacked),8);
#else
    MIExpect(MIFootprint(MIPacked),4);
#endif
}
#endif
#endif
