#include <cmath>
#include <cstring>
#include <new>
#include <climits>
#include <initializer_list>
#ifndef VX_MATRIX_ROTATE_MANY_PROBE
#include "VxMatrix.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef void (*RBFn)(VxVector *, const VxMatrix &, const VxVector *, int, int);
struct RBResult { int samples = 0, errors = 0; };
static void RBFree(VxVector *r, const VxMatrix &m, const VxVector *p, int n, int stride) { Vx3DRotateVectorMany(r,m,p,n,stride); }
#if defined(VX_SIMD_SSE)
static void RBPacked(VxVector *r, const VxMatrix &m, const VxVector *p, int n, int stride) { VxSIMDRotateVectorMany(r,&m,p,n,stride); }
#endif
static unsigned RBNext(unsigned &seed) { seed = seed*1664525u+1013904223u; return seed; }
static bool RBEqual(const VxVector &a, const VxVector &b) {
    for (int c = 0; c < 3; ++c) if (!(a[c] == b[c] || (std::isnan(a[c]) && std::isnan(b[c])))) return false;
    return true;
}
static VxVector RBInteger(const VxMatrix &m, const VxVector &p) {
    VxVector out;
    for (int c = 0; c < 3; ++c) {
        long long value = 0;
        for (int r = 0; r < 3; ++r) value += static_cast<long long>(m[r][c])*static_cast<long long>(p[r]);
        out[c] = float(value);
    }
    return out;
}
static VxMatrix RBMatrix() {
    VxMatrix m; m.Clear(); m[0][1] = 1; m[1][0] = -1; m[2][2] = 1;
    m[3][0] = 7; m[3][1] = -2; m[3][2] = 5; m[0][3] = 19; m[3][3] = 3;
    return m;
}
static bool RBData(RBFn fn, const VxMatrix &m, const VxVector *points, const VxVector *expected, int count, int stride, bool alias) {
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
    for (int i = 0; i < count; ++i) ok = ok && RBEqual(*reinterpret_cast<const VxVector *>(output+start+i*stride),expected[i]);
    for (int b = 0; b < 2048; ++b) {
        bool written = false;
        for (int i = 0; i < count; ++i) written = written || (b >= start+i*stride && b < start+i*stride+12);
        if (!written) ok = ok && output[b] == (alias ? beforeSrc[b] : beforeDst[b]);
    }
    if (!alias) ok = ok && std::memcmp(src,beforeSrc,sizeof(src)) == 0;
    return ok;
}
static bool RBSequentialCase(RBFn fn, int count, int stride, int delta, bool separate) {
    alignas(16) unsigned char src[2048], dst[2048], expectedSrc[2048], expectedDst[2048];
    std::memset(src,0xa5,sizeof(src)); std::memset(dst,0xa5,sizeof(dst));
    const int step = stride < 0 ? -stride : stride;
    for (int i = -2; i < count+2; ++i) new(src+64+(i+2)*step) VxVector(float(i+1),float(2*i-3),float(4-i));
    const int start = 64+2*step+(stride < 0 ? (count-1)*step : 0), target = start+delta*step;
    if (separate) for (int i = 0; i < count; ++i) new(dst+target+i*stride) VxVector;
    std::memcpy(expectedSrc,src,sizeof(src)); std::memcpy(expectedDst,dst,sizeof(dst));
    unsigned char *expected = separate ? expectedDst : expectedSrc, *output = separate ? dst : src;
    VxMatrix m = RBMatrix(), before = m;
    for (int i = 0; i < count; ++i) {
        VxVector p; std::memcpy(&p,expectedSrc+start+i*stride,12); const VxVector r = RBInteger(m,p);
        std::memcpy(expected+target+i*stride,&r,12);
    }
    fn(reinterpret_cast<VxVector *>(output+target),m,reinterpret_cast<const VxVector *>(src+start),count,stride);
    return std::memcmp(src,expectedSrc,sizeof(src)) == 0 && std::memcmp(dst,expectedDst,sizeof(dst)) == 0 && std::memcmp(&m,&before,64) == 0;
}
static RBResult RBLayouts(RBFn fn) {
    RBResult r;
    for (int n : {1,2,3,4,5,7,8,9,16,17}) for (int stride : {12,16,20,28,-12,-16,-28,0}) for (bool separate : {false,true}) {
        ++r.samples; r.errors += !RBSequentialCase(fn,n,stride,0,separate);
    }
    return r;
}
static RBResult RBOverlaps(RBFn fn) {
    RBResult r;
    for (int n : {4,5,8,9}) for (int stride : {12,20,-12,-20}) for (int delta : {-2,-1,0,1,2}) {
        ++r.samples; r.errors += !RBSequentialCase(fn,n,stride,delta,false);
    }
    return r;
}
static RBResult RBLiteral(RBFn fn) {
    const float values[4][4] = {{2,1,-1,7},{3,-2,4,-3},{-1,5,2,9},{7,11,-13,2}};
    VxMatrix m; std::memcpy(&m,values,64); VxVector points[5], expected[5];
    for (int i = 0; i < 5; ++i) { points[i] = VxVector(2,-3,5); expected[i] = VxVector(-10,33,-4); }
    RBResult r; for (bool alias : {false,true}) { ++r.samples; r.errors += !RBData(fn,m,points,expected,5,20,alias); } return r;
}
static RBResult RBFields(RBFn fn) {
    RBResult r; const VxVector point(2,3,5); VxVector points[5], expected[5];
    for (int row = 0; row < 4; ++row) for (int col = 0; col < 4; ++col) {
        VxMatrix m; m.Clear(); m[row][col] = 7;
        for (int i = 0; i < 5; ++i) { points[i] = point; expected[i] = RBInteger(m,point); }
        for (int n : {1,5}) for (int stride : {12,20}) for (bool alias : {false,true}) { ++r.samples; r.errors += !RBData(fn,m,points,expected,n,stride,alias); }
    }
    return r;
}
static RBResult RBIgnored(RBFn fn) {
    RBResult r; VxVector points[5], expected[5]; const VxMatrix base = RBMatrix();
    for (int i = 0; i < 5; ++i) { points[i] = VxVector(2,-3,5); expected[i] = VxVector(3,2,5); }
    for (int field = 0; field < 7; ++field) for (unsigned bits : {0u,0x3f800000u,0x7f800000u,0xff800000u,0x7fc12345u,0x80000000u,0x00800000u,0x7f7fffffu}) {
        VxMatrix m = base; const int row = field < 3 ? field : 3, col = field < 3 ? 3 : field-3;
        std::memcpy(&m[row][col],&bits,4);
        for (bool alias : {false,true}) { ++r.samples; r.errors += !RBData(fn,m,points,expected,5,16,alias); }
    }
    return r;
}
static RBResult RBNumerics(RBFn fn, int kind) {
    RBResult r;
    for (int col = 0; col < 3; ++col) for (int rotate = 0; rotate < 3; ++rotate) {
        VxMatrix m; m.Clear(); VxVector p(1,1,1), e(0,0,0); float x[3], weights[3];
        if (kind == 0) { x[0] = std::ldexp(1.0f,30); x[1] = 1; x[2] = -x[0]; for (float &v : weights) v = 1; e[col] = 1; }
        else if (kind == 1) { x[0] = std::ldexp(1.0f,100); x[1] = -x[0]; x[2] = x[0]; weights[0] = weights[1] = std::ldexp(1.0f,40); weights[2] = std::ldexp(1.0f,20); e[col] = std::ldexp(1.0f,120); }
        else { for (int i = 0; i < 3; ++i) { x[i] = std::ldexp(1.0f,-100); weights[i] = std::ldexp(1.0f,-51); } e[col] = std::ldexp(1.0f,-149); }
        for (int i = 0; i < 3; ++i) { const int at = (i+rotate)%3; m[at][col] = x[i]; p[at] = weights[i]; }
        VxVector points[9], expected[9]; for (int i = 0; i < 9; ++i) { points[i] = p; expected[i] = e; }
        for (int n : {1,3,4,5,8,9}) for (int stride : {12,20,-20}) for (bool alias : {false,true}) { ++r.samples; r.errors += !RBData(fn,m,points,expected,n,stride,alias); }
    }
    return r;
}
static RBResult RBNonfinite(RBFn fn) {
    RBResult r; VxVector points[5], expected[5];
    for (int col = 0; col < 3; ++col) for (int kind = 0; kind < 3; ++kind) {
        VxMatrix m; m.Clear(); m[0][col] = kind == 1 ? -HUGE_VALF : HUGE_VALF; if (kind == 2) m[1][col] = -HUGE_VALF;
        for (int i = 0; i < 5; ++i) { points[i] = VxVector(1,1,1); expected[i] = VxVector(0,0,0); expected[i][col] = kind == 2 ? NAN : m[0][col]; }
        for (int n : {1,5}) for (bool alias : {false,true}) { ++r.samples; r.errors += !RBData(fn,m,points,expected,n,16,alias); }
    }
    return r;
}
static RBResult RBEmpty(RBFn fn) {
    RBResult r; VxMatrix m = RBMatrix(), before = m; VxVector p(1,2,3), out(7,8,9);
    for (int n : {0,-1,INT_MIN}) {
        fn(&out,m,&p,n,12); ++r.samples; r.errors += !RBEqual(out,VxVector(7,8,9)) || !RBEqual(p,VxVector(1,2,3)) || std::memcmp(&m,&before,64) != 0;
        fn(nullptr,m,nullptr,n,-20); ++r.samples; r.errors += std::memcmp(&m,&before,64) != 0;
    }
    return r;
}
static RBResult RBFootprint(RBFn fn) {
    RBResult r; const VxMatrix matrix = RBMatrix(); const VxVector p(2,-3,5), expected(3,2,5);
    for (int shift = 0; shift < 16; shift += 4) {
        alignas(16) unsigned char source[128], dest[128], before[128]; std::memset(dest,0xa5,sizeof(dest));
        for (int i = 0; i < 5; ++i) { new(source+shift+i*20) VxVector(p); new(dest+shift+i*20) VxVector; }
        std::memcpy(before,dest,sizeof(dest)); fn(reinterpret_cast<VxVector *>(dest+shift),matrix,reinterpret_cast<const VxVector *>(source+shift),5,20);
        bool ok = true; for (int i = 0; i < 5; ++i) ok = ok && RBEqual(*reinterpret_cast<const VxVector *>(dest+shift+i*20),expected);
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
        ok = true; for (int i = 0; i < n; ++i) ok = ok && RBEqual(*reinterpret_cast<const VxVector *>(dest+i*step),expected);
        for (int b = 0; b < span; ++b) if (step && b%step >= 12) ok = ok && dest[b] == 0xa5;
        ++r.samples; r.errors += !ok;
        VirtualProtect(memory+size,size,PAGE_READWRITE,&old); VirtualProtect(memory+4*size,size,PAGE_READWRITE,&old);
    }
    VirtualFree(memory,0,MEM_RELEASE);
#endif
    return r;
}
static RBResult RBLinearRows(RBFn fn) {
    RBResult r;
#if defined(_WIN32)
    SYSTEM_INFO info; GetSystemInfo(&info); const unsigned size = info.dwPageSize;
    unsigned char *memory = static_cast<unsigned char *>(VirtualAlloc(nullptr,2*size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if (!memory) { ++r.errors; return r; }
    const VxMatrix prototype = RBMatrix(); VxMatrix *m = new(memory+size-48) VxMatrix(prototype); DWORD old;
    if (!VirtualProtect(memory,size,PAGE_READONLY,&old) || !VirtualProtect(memory+size,size,PAGE_NOACCESS,&old)) { VirtualFree(memory,0,MEM_RELEASE); ++r.errors; return r; }
    for (int n : {1,4,5}) for (int stride : {16,-16,0}) for (bool alias : {false,true}) {
        VxVector4 src[5], dst[5], expectedSrc[5], expectedDst[5];
        for (int i = 0; i < 5; ++i) {
            src[i] = VxVector4(float(i+1),float(2*i-3),float(4-i),0); dst[i] = VxVector4(-99,-98,-97,123.5f);
            const unsigned bits = 0x7fc12345u+i; std::memcpy(&src[i].w,&bits,4);
        }
        std::memcpy(expectedSrc,src,sizeof(src)); std::memcpy(expectedDst,dst,sizeof(dst));
        VxVector4 *expected = alias ? expectedSrc : expectedDst; const int start = stride < 0 ? n-1 : 0;
        for (int i = 0; i < n; ++i) {
            const int at = start+i*(stride/16); const VxVector value = RBInteger(prototype,expectedSrc[at]);
            static_cast<VxVector &>(expected[at]) = value;
        }
        fn(alias ? static_cast<VxVector *>(&src[start]) : static_cast<VxVector *>(&dst[start]),*m,&src[start],n,stride);
        ++r.samples; r.errors += std::memcmp(src,expectedSrc,sizeof(src)) != 0 || std::memcmp(dst,expectedDst,sizeof(dst)) != 0;
    }
    VirtualFree(memory,0,MEM_RELEASE);
#endif
    return r;
}
#ifndef VX_MATRIX_ROTATE_MANY_PROBE
static void RBExpect(RBResult r, int count) { EXPECT_EQ(r.samples,count); EXPECT_EQ(r.errors,0); }
TEST(MatrixRotateManyReference, LiteralLinearXYZWithoutNormalization) { RBExpect(RBLiteral(RBFree),2); }
TEST(MatrixRotateManyReference, CountsAndSignedSharedByteStride) { RBExpect(RBLayouts(RBFree),160); }
TEST(MatrixRotateManyReference, AllFieldsAndRecordPadding) { RBExpect(RBFields(RBFree),128); }
TEST(MatrixRotateManyReference, TranslationAndFourthColumnAreIgnored) { RBExpect(RBIgnored(RBFree),112); }
TEST(MatrixRotateManyReference, ElementOverlapUsesForwardSequentialReads) { RBExpect(RBOverlaps(RBFree),80); }
TEST(MatrixRotateManyReference, CancellationAcrossBatchAndTail) { RBExpect(RBNumerics(RBFree,0),324); }
TEST(MatrixRotateManyReference, FiniteResultsSurviveProductOverflow) { RBExpect(RBNumerics(RBFree,1),324); }
TEST(MatrixRotateManyReference, ProductTailsFormSubnormalResults) { RBExpect(RBNumerics(RBFree,2),324); }
TEST(MatrixRotateManyReference, NonfiniteXYZClassification) { RBExpect(RBNonfinite(RBFree),36); }
TEST(MatrixRotateManyReference, NonpositiveCountsDoNotAccessArrays) { RBExpect(RBEmpty(RBFree),6); }
TEST(MatrixRotateManyReference, ExactXYZWidthsAtProtectedPageEdges) {
#if defined(_WIN32)
    RBExpect(RBFootprint(RBFree),124);
#else
    RBExpect(RBFootprint(RBFree),4);
#endif
}
TEST(MatrixRotateManyReference, TranslationRowIsNeverReadAndVectorWIsPreserved) {
#if defined(_WIN32)
    RBExpect(RBLinearRows(RBFree),18);
#else
    RBExpect(RBLinearRows(RBFree),0);
#endif
}
#if defined(VX_SIMD_SSE)
TEST(MatrixRotateManyReference, PackedCountsAndSignedStride) { RBExpect(RBLayouts(RBPacked),160); }
TEST(MatrixRotateManyReference, PackedFieldRolesAndIgnoredColumn) { RBExpect(RBFields(RBPacked),128); RBExpect(RBIgnored(RBPacked),112); }
TEST(MatrixRotateManyReference, PackedElementOverlapHasSequentialOrder) { RBExpect(RBOverlaps(RBPacked),80); }
TEST(MatrixRotateManyReference, PackedNumericalBatchAndTail) { RBExpect(RBNumerics(RBPacked,0),324); RBExpect(RBNumerics(RBPacked,1),324); RBExpect(RBNumerics(RBPacked,2),324); }
TEST(MatrixRotateManyReference, PackedNonfiniteAndExactBoundaries) {
    RBExpect(RBNonfinite(RBPacked),36); RBExpect(RBEmpty(RBPacked),6);
#if defined(_WIN32)
    RBExpect(RBLinearRows(RBPacked),18);
#endif
#if defined(_WIN32)
    RBExpect(RBFootprint(RBPacked),124);
#else
    RBExpect(RBFootprint(RBPacked),4);
#endif
}
#endif
#endif
