#include <cmath>
#include <cstring>
#include <new>
#include <initializer_list>
#ifndef VX_MATRIX_VECTOR4_PROBE
#include "VxMatrix.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef void (*HVFn)(VxVector4 *, const VxMatrix &, const VxVector *);
struct HVResult { int samples = 0, errors = 0; };
static void HVFull(VxVector4 *out, const VxMatrix &m, const VxVector *p) { Vx3DMultiplyMatrixVector4(out,m,static_cast<const VxVector4 *>(p)); }
static void HVPoint(VxVector4 *out, const VxMatrix &m, const VxVector *p) { Vx3DMultiplyMatrixVector4(out,m,p); }
#if defined(VX_SIMD_SSE)
static void HVPackedFull(VxVector4 *out, const VxMatrix &m, const VxVector *p) { VxSIMDMultiplyMatrixVector4(out,&m,static_cast<const VxVector4 *>(p)); }
static void HVPackedPoint(VxVector4 *out, const VxMatrix &m, const VxVector *p) { VxSIMDMultiplyMatrixVector4FromVector3(out,&m,p); }
#endif
static unsigned HVNext(unsigned &seed) { seed = seed*1664525u+1013904223u; return seed; }
static bool HVEqual(const VxVector4 &a, const VxVector4 &b) {
    for (int i = 0; i < 4; ++i) if (!(a[i] == b[i] || (std::isnan(a[i]) && std::isnan(b[i])))) return false;
    return true;
}
static VxVector4 HVExpected(const VxMatrix &m, const VxVector4 &p, bool point) {
    VxVector4 out;
    for (int c = 0; c < 4; ++c) {
        long long value = 0;
        for (int r = 0; r < 4; ++r) value += static_cast<long long>(m[r][c])*static_cast<long long>(point && r == 3 ? 1 : p[r]);
        out[c] = float(value);
    }
    return out;
}
static bool HVExecute(VxVector4 &out, HVFn fn, const VxMatrix &m, const VxVector4 &p, bool alias) {
    VxMatrix matrix = m; VxVector4 input = p;
    if (alias) { fn(&input,matrix,&input); out = input; }
    else { fn(&out,matrix,&input); if (std::memcmp(&input,&p,16) != 0) return false; }
    return std::memcmp(&matrix,&m,64) == 0;
}
static void HVLiteralInput(VxMatrix &m, VxVector4 &p) {
    const float values[4][4] = {{2,1,-1,7},{3,-2,4,-3},{-1,5,2,9},{7,11,-13,2}};
    std::memcpy(&m,values,64); p = VxVector4(2,-3,5,-2);
}
static HVResult HVLiteral(HVFn fn, bool point) {
    VxMatrix m; VxVector4 p; HVLiteralInput(m,p); HVResult result;
    const VxVector4 expected = point ? VxVector4(-3,44,-17,70) : VxVector4(-24,11,22,64);
    for (bool alias : {false,true}) { VxVector4 out; ++result.samples; result.errors += !(HVExecute(out,fn,m,p,alias) && HVEqual(out,expected)); }
    return result;
}
static HVResult HVIntegers(HVFn fn, bool point) {
    HVResult result; unsigned seed = 295;
    for (int sample = 0; sample < 512; ++sample) {
        VxMatrix m; VxVector4 p;
        for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) m[r][c] = float(int(HVNext(seed)%17)-8);
        for (int c = 0; c < 4; ++c) p[c] = float(int(HVNext(seed)%17)-8);
        const VxVector4 expected = HVExpected(m,p,point);
        for (bool alias : {false,true}) { VxVector4 out; ++result.samples; result.errors += !(HVExecute(out,fn,m,p,alias) && HVEqual(out,expected)); }
    }
    return result;
}
static HVResult HVFields(HVFn fn, bool point) {
    HVResult result; const VxVector4 p(2,3,5,-2);
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) {
        VxMatrix m; m.Clear(); m[r][c] = 7; VxVector4 expected(0,0,0,0); expected[c] = 7*(point && r == 3 ? 1 : p[r]);
        for (bool alias : {false,true}) { VxVector4 out; ++result.samples; result.errors += !(HVExecute(out,fn,m,p,alias) && HVEqual(out,expected)); }
    }
    return result;
}
static HVResult HVWeights(HVFn fn, bool point) {
    HVResult result;
    for (float weight : {0.0f,-2.0f,0.5f,1.0f,2.0f}) {
        VxMatrix m; m.SetIdentity(); m[3][0] = 7; m[3][1] = 11; m[3][2] = -13; m[3][3] = 2;
        const VxVector4 p(2,-3,5,weight); const float w = point ? 1 : weight;
        const VxVector4 expected(2+7*w,-3+11*w,5-13*w,2*w);
        for (bool alias : {false,true}) { VxVector4 out; ++result.samples; result.errors += !(HVExecute(out,fn,m,p,alias) && HVEqual(out,expected)); }
    }
    return result;
}
static HVResult HVIgnoredInputW(HVFn fn) {
    HVResult result;
    for (unsigned bits : {0u,0x3f800000u,0x40000000u,0xc0400000u,0x7f800000u,0xff800000u,0x7fc12345u,0x80000000u}) {
        VxMatrix m; VxVector4 p; HVLiteralInput(m,p); std::memcpy(&p.w,&bits,4);
        for (bool alias : {false,true}) { VxVector4 out; ++result.samples; result.errors += !(HVExecute(out,fn,m,p,alias) && HVEqual(out,VxVector4(-3,44,-17,70))); }
    }
    return result;
}
static HVResult HVMatrixRows(HVFn fn, bool point) {
    HVResult result;
    for (int inRow = -1; inRow < 4; ++inRow) for (int outRow = 0; outRow < 4; ++outRow) {
        VxMatrix m; VxVector4 p; HVLiteralInput(m,p); const VxVector4 *input = inRow < 0 ? &p : &m[inRow];
        const VxVector4 expected = HVExpected(m,*input,point); VxMatrix before = m; fn(&m[outRow],m,input);
        bool ok = HVEqual(m[outRow],expected);
        for (int r = 0; r < 4; ++r) if (r != outRow) ok = ok && std::memcmp(&m[r],&before[r],16) == 0;
        ++result.samples; result.errors += !ok;
    }
    return result;
}
static HVResult HVNumerics(HVFn fn, bool point, int kind) {
    HVResult result; const int rotations = point && kind != 0 ? 3 : 4;
    for (int col = 0; col < 4; ++col) for (int rotate = 0; rotate < rotations; ++rotate) {
        VxMatrix m; m.Clear(); VxVector4 p(1,1,1,1); float x[4], weights[4], expected;
        if (kind == 0) { x[0] = std::ldexp(1.0f,80); x[1] = 1; x[2] = -x[0]; x[3] = 1; for (float &w : weights) w = 1; expected = 2; }
        else if (kind == 1) { x[0] = std::ldexp(1.0f,100); x[1] = -x[0]; x[2] = 1; x[3] = 2; weights[0] = weights[1] = std::ldexp(1.0f,40); weights[2] = weights[3] = 1; expected = 3; }
        else { for (int i = 0; i < 4; ++i) { x[i] = std::ldexp(1.0f,-100); weights[i] = std::ldexp(1.0f,-51); } if (point) { x[3] = 0; weights[3] = 1; } expected = std::ldexp(1.0f,-149); }
        const int terms = point && kind != 0 ? 3 : 4;
        for (int i = 0; i < terms; ++i) { const int at = (i+rotate)%terms; m[at][col] = x[i]; p[at] = weights[i]; }
        if (terms == 3) m[3][col] = x[3];
        for (bool alias : {false,true}) { VxVector4 out; ++result.samples; result.errors += !(HVExecute(out,fn,m,p,alias) && out[col] == expected); }
    }
    return result;
}
static HVResult HVNonfinite(HVFn fn) {
    HVResult result; const VxVector4 p(1,1,1,1);
    for (int col = 0; col < 4; ++col) for (int kind = 0; kind < 3; ++kind) {
        VxMatrix m; m.Clear(); m[0][col] = kind == 1 ? -HUGE_VALF : HUGE_VALF; if (kind == 2) m[1][col] = -HUGE_VALF;
        for (bool alias : {false,true}) {
            VxVector4 out; const bool inputs = HVExecute(out,fn,m,p,alias);
            const bool ok = kind == 2 ? std::isnan(out[col]) : std::isinf(out[col]) && std::signbit(out[col]) == (kind == 1);
            ++result.samples; result.errors += !(inputs && ok);
        }
    }
    return result;
}
static HVResult HVZeroWeightIEEE(HVFn fn, bool point) {
    HVResult result;
    for (float zero : {0.0f,-0.0f}) {
        VxMatrix m; m.Clear(); for (int c = 0; c < 4; ++c) m[3][c] = HUGE_VALF;
        const VxVector4 p(0,0,0,zero);
        for (bool alias : {false,true}) {
            VxVector4 out; bool ok = HVExecute(out,fn,m,p,alias);
            for (int c = 0; c < 4; ++c) ok = ok && (point ? std::isinf(out[c]) && out[c] > 0 : std::isnan(out[c]));
            ++result.samples; result.errors += !ok;
        }
    }
    return result;
}
static HVResult HVFootprint(HVFn fn, bool point) {
    HVResult result; VxMatrix m; VxVector4 p; HVLiteralInput(m,p); const VxVector4 expected = point ? VxVector4(-3,44,-17,70) : VxVector4(-24,11,22,64);
    for (int shift = 0; shift < 16; shift += 4) {
        alignas(16) unsigned char bytes[240], before[240]; std::memset(bytes,0xa5,sizeof(bytes));
        VxMatrix *pm = new(bytes+16+shift) VxMatrix(m); VxVector4 *pv = new(bytes+112+shift) VxVector4(p), *pr = new(bytes+176+shift) VxVector4;
        std::memcpy(before,bytes,sizeof(bytes)); fn(pr,*pm,pv); bool ok = HVEqual(*pr,expected);
        for (int i = 0; i < 240; ++i) if (i < 176+shift || i >= 192+shift) ok = ok && bytes[i] == before[i];
        ++result.samples; result.errors += !ok; pm->~VxMatrix(); pv->~VxVector4(); pr->~VxVector4();
    }
#if defined(_WIN32)
    SYSTEM_INFO info; GetSystemInfo(&info); const unsigned size = info.dwPageSize;
    unsigned char *memory = static_cast<unsigned char *>(VirtualAlloc(nullptr,9*size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if (!memory) { ++result.errors; return result; } DWORD old; bool ok = true;
    for (unsigned page : {0u,2u,3u,5u,6u,8u}) ok = ok && VirtualProtect(memory+page*size,size,PAGE_NOACCESS,&old);
    if (!ok) { VirtualFree(memory,0,MEM_RELEASE); ++result.errors; return result; }
    for (int i = 0; i < 8; ++i) {
        VxMatrix *pm = new(memory+size+(i&1 ? size-64 : 0)) VxMatrix(m);
        unsigned char *input = memory+4*size+(i&2 ? size-(point ? 12 : 16) : 0);
        VxVector *pv = point ? new(input) VxVector(p.x,p.y,p.z) : new(input) VxVector4(p);
        VxVector4 *pr = new(memory+7*size+(i&4 ? size-16 : 0)) VxVector4;
        ok = VirtualProtect(memory+size,size,PAGE_READONLY,&old) && VirtualProtect(memory+4*size,size,PAGE_READONLY,&old);
        if (!ok) { ++result.errors; break; } fn(pr,*pm,pv); ++result.samples; result.errors += !HVEqual(*pr,expected);
        VirtualProtect(memory+size,size,PAGE_READWRITE,&old); VirtualProtect(memory+4*size,size,PAGE_READWRITE,&old);
        pm->~VxMatrix(); if (point) pv->~VxVector(); else static_cast<VxVector4 *>(pv)->~VxVector4(); pr->~VxVector4();
    }
    VirtualFree(memory,0,MEM_RELEASE);
#endif
    return result;
}
#ifndef VX_MATRIX_VECTOR4_PROBE
static void HVExpect(HVResult r, int count) { EXPECT_EQ(r.samples,count); EXPECT_EQ(r.errors,0); }
TEST(MatrixVector4Reference, ActualWAndFullOutputLiteral) { HVExpect(HVLiteral(HVFull,false),2); }
TEST(MatrixVector4Reference, ImplicitOneAndFullOutputLiteral) { HVExpect(HVLiteral(HVPoint,true),2); }
TEST(MatrixVector4Reference, IntegerHomogeneousMapsAndAliases) { HVExpect(HVIntegers(HVFull,false),1024); HVExpect(HVIntegers(HVPoint,true),1024); }
TEST(MatrixVector4Reference, AllSixteenFieldsInfluenceXYZW) { HVExpect(HVFields(HVFull,false),32); HVExpect(HVFields(HVPoint,true),32); }
TEST(MatrixVector4Reference, ActualAndImplicitWeightsHaveDistinctContracts) { HVExpect(HVWeights(HVFull,false),10); HVExpect(HVWeights(HVPoint,true),10); HVExpect(HVIgnoredInputW(HVPoint),16); }
TEST(MatrixVector4Reference, MatrixRowViewsAndWideningBaseAlias) { HVExpect(HVMatrixRows(HVFull,false),20); HVExpect(HVMatrixRows(HVPoint,true),20); }
TEST(MatrixVector4Reference, FourTermCancellationIncludesOutputW) { HVExpect(HVNumerics(HVFull,false,0),32); HVExpect(HVNumerics(HVPoint,true,0),32); }
TEST(MatrixVector4Reference, FiniteXYZWSurviveProductOverflow) { HVExpect(HVNumerics(HVFull,false,1),32); HVExpect(HVNumerics(HVPoint,true,1),24); }
TEST(MatrixVector4Reference, ProductTailsFormSubnormalXYZW) { HVExpect(HVNumerics(HVFull,false,2),32); HVExpect(HVNumerics(HVPoint,true,2),24); }
TEST(MatrixVector4Reference, NonfiniteAndZeroWeightIEEEClassification) { for (HVFn fn : {HVFull,HVPoint}) HVExpect(HVNonfinite(fn),24); HVExpect(HVZeroWeightIEEE(HVFull,false),4); HVExpect(HVZeroWeightIEEE(HVPoint,true),4); }
TEST(MatrixVector4Reference, ExactInputWidthsAndFullOutputFootprints) {
#if defined(_WIN32)
    HVExpect(HVFootprint(HVFull,false),12); HVExpect(HVFootprint(HVPoint,true),12);
#else
    HVExpect(HVFootprint(HVFull,false),4); HVExpect(HVFootprint(HVPoint,true),4);
#endif
}
#if defined(VX_SIMD_SSE)
TEST(MatrixVector4Reference, PackedIntegerHomogeneousMaps) { HVExpect(HVIntegers(HVPackedFull,false),1024); HVExpect(HVIntegers(HVPackedPoint,true),1024); }
TEST(MatrixVector4Reference, PackedWeightsFieldsAndRowAliases) {
    HVExpect(HVFields(HVPackedFull,false),32); HVExpect(HVFields(HVPackedPoint,true),32);
    HVExpect(HVWeights(HVPackedFull,false),10); HVExpect(HVWeights(HVPackedPoint,true),10); HVExpect(HVIgnoredInputW(HVPackedPoint),16);
    HVExpect(HVMatrixRows(HVPackedFull,false),20); HVExpect(HVMatrixRows(HVPackedPoint,true),20);
}
TEST(MatrixVector4Reference, PackedCancellationScaleAndSubnormal) { for (int k = 0; k < 3; ++k) { HVExpect(HVNumerics(HVPackedFull,false,k),32); HVExpect(HVNumerics(HVPackedPoint,true,k),k == 0 ? 32 : 24); } }
TEST(MatrixVector4Reference, PackedNonfiniteAndZeroWeightIEEE) { for (HVFn fn : {HVPackedFull,HVPackedPoint}) HVExpect(HVNonfinite(fn),24); HVExpect(HVZeroWeightIEEE(HVPackedFull,false),4); HVExpect(HVZeroWeightIEEE(HVPackedPoint,true),4); }
TEST(MatrixVector4Reference, PackedInputAndOutputFootprints) {
#if defined(_WIN32)
    HVExpect(HVFootprint(HVPackedFull,false),12); HVExpect(HVFootprint(HVPackedPoint,true),12);
#else
    HVExpect(HVFootprint(HVPackedFull,false),4); HVExpect(HVFootprint(HVPackedPoint,true),4);
#endif
}
#endif
#endif
