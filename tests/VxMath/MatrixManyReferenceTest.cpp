#include <cmath>
#include <cstring>
#include <new>
#include <climits>
#include <initializer_list>
#ifndef VX_MATRIX_MANY_PROBE
#include "VxMatrix.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef void (*MBFn)(VxVector *, const VxMatrix &, const VxVector *, int, int);
struct MBResult { int samples = 0, errors = 0; };
static void MBFree(VxVector *r, const VxMatrix &m, const VxVector *p, int n, int stride) { Vx3DMultiplyMatrixVectorMany(r,m,p,n,stride); }
#if defined(VX_SIMD_SSE)
static void MBPacked(VxVector *r, const VxMatrix &m, const VxVector *p, int n, int stride) { VxSIMDMultiplyMatrixVectorMany(r,&m,p,n,stride); }
#endif
static unsigned MBNext(unsigned &seed) { seed = seed*1664525u+1013904223u; return seed; }
static bool MBEqual(const VxVector &a, const VxVector &b) {
    for (int c = 0; c < 3; ++c) if (!(a[c] == b[c] || (std::isnan(a[c]) && std::isnan(b[c])))) return false;
    return true;
}
static VxVector MBInteger(const VxMatrix &m, const VxVector &p) {
    VxVector out;
    for (int c = 0; c < 3; ++c) {
        long long value = static_cast<long long>(m[3][c]);
        for (int r = 0; r < 3; ++r) value += static_cast<long long>(m[r][c])*static_cast<long long>(p[r]);
        out[c] = float(value);
    }
    return out;
}
static VxMatrix MBMatrix() {
    VxMatrix m; m.Clear(); m[0][1] = 1; m[1][0] = -1; m[2][2] = 1;
    m[3][0] = 7; m[3][1] = -2; m[3][2] = 5; m[0][3] = 19; m[3][3] = 3;
    return m;
}
static bool MBData(MBFn fn, const VxMatrix &m, const VxVector *points, const VxVector *expected, int count, int stride, bool alias) {
    alignas(16) unsigned char src[2048], dst[2048], beforeSrc[2048], beforeDst[2048];
    std::memset(src,0xa5,sizeof(src)); std::memset(dst,0xa5,sizeof(dst));
    const int start = 64+(stride < 0 ? (count-1)*(-stride) : 0);
    for (int i = 0; i < count; ++i) {
        new(src+start+i*stride) VxVector(points[i]);
        if (!alias) new(dst+start+i*stride) VxVector;
    }
    std::memcpy(beforeSrc,src,sizeof(src)); std::memcpy(beforeDst,dst,sizeof(dst));
    unsigned char *output = alias ? src : dst; VxMatrix matrix = m;
    fn(reinterpret_cast<VxVector *>(output+start),matrix,reinterpret_cast<const VxVector *>(src+start),count,stride);
    bool ok = std::memcmp(&m,&matrix,64) == 0;
    for (int i = 0; i < count; ++i) ok = ok && MBEqual(*reinterpret_cast<const VxVector *>(output+start+i*stride),expected[i]);
    for (int b = 0; b < 2048; ++b) {
        bool written = false;
        for (int i = 0; i < count; ++i) written = written || (b >= start+i*stride && b < start+i*stride+12);
        if (!written) ok = ok && output[b] == (alias ? beforeSrc[b] : beforeDst[b]);
    }
    if (!alias) ok = ok && std::memcmp(src,beforeSrc,sizeof(src)) == 0;
    return ok;
}
static bool MBSequentialCase(MBFn fn, int count, int stride, int delta, bool separate) {
    alignas(16) unsigned char src[2048], dst[2048], expectedSrc[2048], expectedDst[2048];
    std::memset(src,0xa5,sizeof(src)); std::memset(dst,0xa5,sizeof(dst));
    const int step = stride < 0 ? -stride : stride;
    for (int i = -2; i < count+2; ++i) new(src+64+(i+2)*step) VxVector(float(i+1),float(2*i-3),float(4-i));
    const int start = 64+2*step+(stride < 0 ? (count-1)*step : 0), target = start+delta*step;
    if (separate) for (int i = 0; i < count; ++i) new(dst+target+i*stride) VxVector;
    std::memcpy(expectedSrc,src,sizeof(src)); std::memcpy(expectedDst,dst,sizeof(dst));
    unsigned char *expected = separate ? expectedDst : expectedSrc, *output = separate ? dst : src;
    VxMatrix m = MBMatrix(), before = m;
    for (int i = 0; i < count; ++i) {
        VxVector p; std::memcpy(&p,expectedSrc+start+i*stride,12); const VxVector r = MBInteger(m,p);
        std::memcpy(expected+target+i*stride,&r,12);
    }
    fn(reinterpret_cast<VxVector *>(output+target),m,reinterpret_cast<const VxVector *>(src+start),count,stride);
    return std::memcmp(src,expectedSrc,sizeof(src)) == 0 && std::memcmp(dst,expectedDst,sizeof(dst)) == 0 && std::memcmp(&m,&before,64) == 0;
}
static MBResult MBLayouts(MBFn fn) {
    MBResult r;
    for (int n : {1,2,3,4,5,7,8,9,16,17}) for (int stride : {12,16,20,28,-12,-16,-28,0}) for (bool separate : {false,true}) {
        ++r.samples; r.errors += !MBSequentialCase(fn,n,stride,0,separate);
    }
    return r;
}
static MBResult MBOverlaps(MBFn fn) {
    MBResult r;
    for (int n : {4,5,8,9}) for (int stride : {12,20,-12,-20}) for (int delta : {-2,-1,0,1,2}) {
        ++r.samples; r.errors += !MBSequentialCase(fn,n,stride,delta,false);
    }
    return r;
}
static MBResult MBLiteral(MBFn fn) {
    const float values[4][4] = {{2,1,-1,7},{3,-2,4,-3},{-1,5,2,9},{7,11,-13,2}};
    VxMatrix m; std::memcpy(&m,values,64); VxVector points[5], expected[5];
    for (int i = 0; i < 5; ++i) { points[i] = VxVector(2,-3,5); expected[i] = VxVector(-3,44,-17); }
    MBResult r; for (bool alias : {false,true}) { ++r.samples; r.errors += !MBData(fn,m,points,expected,5,20,alias); } return r;
}
static MBResult MBFields(MBFn fn) {
    MBResult r; const VxVector point(2,3,5); VxVector points[5], expected[5];
    for (int row = 0; row < 4; ++row) for (int col = 0; col < 4; ++col) {
        VxMatrix m; m.Clear(); m[row][col] = 7;
        for (int i = 0; i < 5; ++i) { points[i] = point; expected[i] = MBInteger(m,point); }
        for (int n : {1,5}) for (int stride : {12,20}) for (bool alias : {false,true}) { ++r.samples; r.errors += !MBData(fn,m,points,expected,n,stride,alias); }
    }
    return r;
}
static MBResult MBIgnored(MBFn fn) {
    MBResult r; VxVector points[5], expected[5]; const VxMatrix base = MBMatrix();
    for (int i = 0; i < 5; ++i) { points[i] = VxVector(2,-3,5); expected[i] = VxVector(10,0,10); }
    for (int row = 0; row < 4; ++row) for (unsigned bits : {0u,0x3f800000u,0x7f800000u,0xff800000u,0x7fc12345u,0x80000000u,0x00800000u,0x7f7fffffu}) {
        VxMatrix m = base; std::memcpy(&m[row][3],&bits,4);
        for (bool alias : {false,true}) { ++r.samples; r.errors += !MBData(fn,m,points,expected,5,16,alias); }
    }
    return r;
}
static MBResult MBNumerics(MBFn fn, int kind) {
    MBResult r; const int terms = kind == 0 ? 4 : 3;
    for (int col = 0; col < 3; ++col) for (int rotate = 0; rotate < terms; ++rotate) {
        VxMatrix m; m.Clear(); VxVector p(1,1,1), e(0,0,0); float x[4], weights[4];
        if (kind == 0) { x[0] = std::ldexp(1.0f,30); x[1] = 1; x[2] = -x[0]; x[3] = 1; for (float &v : weights) v = 1; e[col] = 2; }
        else if (kind == 1) { x[0] = std::ldexp(1.0f,100); x[1] = -x[0]; x[2] = x[0]; x[3] = 2; weights[0] = weights[1] = std::ldexp(1.0f,40); weights[2] = std::ldexp(1.0f,20); weights[3] = 1; e[col] = std::ldexp(1.0f,120); }
        else { for (int i = 0; i < 3; ++i) { x[i] = std::ldexp(1.0f,-100); weights[i] = std::ldexp(1.0f,-51); } x[3] = 0; weights[3] = 1; e[col] = std::ldexp(1.0f,-149); }
        for (int i = 0; i < terms; ++i) { const int at = (i+rotate)%terms; m[at][col] = x[i]; if (at < 3) p[at] = weights[i]; }
        if (terms == 3) m[3][col] = x[3];
        VxVector points[9], expected[9]; for (int i = 0; i < 9; ++i) { points[i] = p; expected[i] = e; }
        for (int n : {1,3,4,5,8,9}) for (int stride : {12,20,-20}) for (bool alias : {false,true}) { ++r.samples; r.errors += !MBData(fn,m,points,expected,n,stride,alias); }
    }
    return r;
}
static MBResult MBNonfinite(MBFn fn) {
    MBResult r; VxVector points[5], expected[5];
    for (int col = 0; col < 3; ++col) for (int kind = 0; kind < 3; ++kind) {
        VxMatrix m; m.Clear(); m[0][col] = kind == 1 ? -HUGE_VALF : HUGE_VALF; if (kind == 2) m[1][col] = -HUGE_VALF;
        for (int i = 0; i < 5; ++i) { points[i] = VxVector(1,1,1); expected[i] = VxVector(0,0,0); expected[i][col] = kind == 2 ? NAN : m[0][col]; }
        for (int n : {1,5}) for (bool alias : {false,true}) { ++r.samples; r.errors += !MBData(fn,m,points,expected,n,16,alias); }
    }
    return r;
}
static MBResult MBEmpty(MBFn fn) {
    MBResult r; VxMatrix m = MBMatrix(), before = m; VxVector p(1,2,3), out(7,8,9);
    for (int n : {0,-1,INT_MIN}) {
        fn(&out,m,&p,n,12); ++r.samples; r.errors += !MBEqual(out,VxVector(7,8,9)) || !MBEqual(p,VxVector(1,2,3)) || std::memcmp(&m,&before,64) != 0;
        fn(nullptr,m,nullptr,n,-20); ++r.samples; r.errors += std::memcmp(&m,&before,64) != 0;
    }
    return r;
}
static MBResult MBFootprint(MBFn fn) {
    MBResult r; const VxMatrix matrix = MBMatrix(); const VxVector p(2,-3,5), expected(10,0,10);
    for (int shift = 0; shift < 16; shift += 4) {
        alignas(16) unsigned char source[128], dest[128], before[128]; std::memset(dest,0xa5,sizeof(dest));
        for (int i = 0; i < 5; ++i) { new(source+shift+i*20) VxVector(p); new(dest+shift+i*20) VxVector; }
        std::memcpy(before,dest,sizeof(dest)); fn(reinterpret_cast<VxVector *>(dest+shift),matrix,reinterpret_cast<const VxVector *>(source+shift),5,20);
        bool ok = true; for (int i = 0; i < 5; ++i) ok = ok && MBEqual(*reinterpret_cast<const VxVector *>(dest+shift+i*20),expected);
        for (int b = 0; b < 128; ++b) { bool written = false; for (int i = 0; i < 5; ++i) written = written || (b >= shift+i*20 && b < shift+i*20+12); if (!written) ok = ok && before[b] == dest[b]; }
        ++r.samples; r.errors += !ok;
    }
#if defined(_WIN32)
    SYSTEM_INFO info; GetSystemInfo(&info); const unsigned size = info.dwPageSize;
    unsigned char *memory = static_cast<unsigned char *>(VirtualAlloc(nullptr,9*size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if (!memory) { ++r.errors; return r; } DWORD old; bool ok = true;
    for (unsigned page : {0u,2u,3u,5u,6u,8u}) ok = ok && VirtualProtect(memory+page*size,size,PAGE_NOACCESS,&old);
    if (!ok) { VirtualFree(memory,0,MEM_RELEASE); ++r.errors; return r; }
    for (int n : {1,4,5}) for (int stride : {12,20,-12,-20,0}) for (int edge = 0; edge < 8; ++edge) {
        const int step = stride < 0 ? -stride : stride, span = (n-1)*step+12;
        VxMatrix *m = new(memory+size+(edge&1 ? size-64 : 0)) VxMatrix(matrix);
        unsigned char *source = memory+4*size+(edge&2 ? size-span : 0), *dest = memory+7*size+(edge&4 ? size-span : 0);
        std::memset(source,0xa5,span); std::memset(dest,0xa5,span);
        for (int i = 0; i < n; ++i) { new(source+i*step) VxVector(p); new(dest+i*step) VxVector; }
        ok = VirtualProtect(memory+size,size,PAGE_READONLY,&old) && VirtualProtect(memory+4*size,size,PAGE_READONLY,&old);
        if (!ok) { ++r.errors; break; }
        fn(reinterpret_cast<VxVector *>(dest+(stride < 0 ? span-12 : 0)),*m,reinterpret_cast<const VxVector *>(source+(stride < 0 ? span-12 : 0)),n,stride);
        ok = true; for (int i = 0; i < n; ++i) ok = ok && MBEqual(*reinterpret_cast<const VxVector *>(dest+i*step),expected);
        for (int b = 0; b < span; ++b) if (step && b%step >= 12) ok = ok && dest[b] == 0xa5;
        ++r.samples; r.errors += !ok;
        VirtualProtect(memory+size,size,PAGE_READWRITE,&old); VirtualProtect(memory+4*size,size,PAGE_READWRITE,&old);
    }
    VirtualFree(memory,0,MEM_RELEASE);
#endif
    return r;
}
#ifndef VX_MATRIX_MANY_PROBE
static void MBExpect(MBResult r, int count) { EXPECT_EQ(r.samples,count); EXPECT_EQ(r.errors,0); }
TEST(MatrixManyReference, LiteralAffineXYZWithoutPerspectiveDivision) { MBExpect(MBLiteral(MBFree),2); }
TEST(MatrixManyReference, CountsAndSignedSharedByteStride) { MBExpect(MBLayouts(MBFree),160); }
TEST(MatrixManyReference, AllFieldsAndRecordPadding) { MBExpect(MBFields(MBFree),128); }
TEST(MatrixManyReference, FourthColumnIsIgnored) { MBExpect(MBIgnored(MBFree),64); }
TEST(MatrixManyReference, ElementOverlapUsesForwardSequentialReads) { MBExpect(MBOverlaps(MBFree),80); }
TEST(MatrixManyReference, CancellationAcrossBatchAndTail) { MBExpect(MBNumerics(MBFree,0),432); }
TEST(MatrixManyReference, FiniteResultsSurviveProductOverflow) { MBExpect(MBNumerics(MBFree,1),324); }
TEST(MatrixManyReference, ProductTailsFormSubnormalResults) { MBExpect(MBNumerics(MBFree,2),324); }
TEST(MatrixManyReference, NonfiniteXYZClassification) { MBExpect(MBNonfinite(MBFree),36); }
TEST(MatrixManyReference, NonpositiveCountsDoNotAccessArrays) { MBExpect(MBEmpty(MBFree),6); }
TEST(MatrixManyReference, ExactXYZWidthsAtProtectedPageEdges) {
#if defined(_WIN32)
    MBExpect(MBFootprint(MBFree),124);
#else
    MBExpect(MBFootprint(MBFree),4);
#endif
}
#if defined(VX_SIMD_SSE)
TEST(MatrixManyReference, PackedCountsAndSignedStride) { MBExpect(MBLayouts(MBPacked),160); }
TEST(MatrixManyReference, PackedFieldRolesAndIgnoredColumn) { MBExpect(MBFields(MBPacked),128); MBExpect(MBIgnored(MBPacked),64); }
TEST(MatrixManyReference, PackedElementOverlapHasSequentialOrder) { MBExpect(MBOverlaps(MBPacked),80); }
TEST(MatrixManyReference, PackedNumericalBatchAndTail) { MBExpect(MBNumerics(MBPacked,0),432); MBExpect(MBNumerics(MBPacked,1),324); MBExpect(MBNumerics(MBPacked,2),324); }
TEST(MatrixManyReference, PackedNonfiniteAndExactBoundaries) {
    MBExpect(MBNonfinite(MBPacked),36); MBExpect(MBEmpty(MBPacked),6);
#if defined(_WIN32)
    MBExpect(MBFootprint(MBPacked),124);
#else
    MBExpect(MBFootprint(MBPacked),4);
#endif
}
#endif
#endif
