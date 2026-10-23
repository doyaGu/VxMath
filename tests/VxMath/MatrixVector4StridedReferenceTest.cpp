#include <cmath>
#include <cstring>
#include <new>
#include <climits>
#include <initializer_list>
#ifndef VX_MATRIX_VECTOR4_STRIDED_PROBE
#include "VxMatrix.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef void (*QSFn)(VxStridedData *, VxStridedData *, const VxMatrix &, int);
struct QSResult { int samples = 0, errors = 0, accessViolations = 0; };
static void QSFree(VxStridedData *d, VxStridedData *s, const VxMatrix &m, int n) { Vx3DMultiplyMatrixVector4Strided(d,s,m,n); }
#if defined(VX_SIMD_SSE)
static void QSPacked(VxStridedData *d, VxStridedData *s, const VxMatrix &m, int n) { VxSIMDMultiplyMatrixVector4Strided(d,s,&m,n); }
#endif
static unsigned QSNext(unsigned &seed) { seed = seed*1664525u+1013904223u; return seed; }
static bool QSEqual(const VxVector4 &a, const VxVector4 &b) {
    for (int c = 0; c < 4; ++c) if (!(a[c] == b[c] || (std::isnan(a[c]) && std::isnan(b[c])))) return false;
    return true;
}
static VxVector4 QSInteger(const VxMatrix &m, const VxVector &p) {
    VxVector4 out;
    for (int c = 0; c < 4; ++c) {
        long long value = static_cast<long long>(m[3][c]);
        for (int r = 0; r < 3; ++r) value += static_cast<long long>(m[r][c])*static_cast<long long>(p[r]);
        out[c] = float(value);
    }
    return out;
}
static VxMatrix QSMatrix() {
    VxMatrix m; m.Clear(); m[0][1] = 1; m[1][0] = -1; m[2][2] = 1;
    m[3][0] = 7; m[3][1] = -2; m[3][2] = 5; m[0][3] = 19; m[3][3] = 3; return m;
}
static bool QSCall(QSFn fn, void *out, void *input, const VxMatrix &m, int count, unsigned ss, unsigned ds, bool sameDescriptor = false) {
    VxStridedData src(input,ss), dest(out,ds); unsigned char beforeSrc[sizeof(src)], beforeDest[sizeof(dest)];
    std::memcpy(beforeSrc,&src,sizeof(src)); std::memcpy(beforeDest,&dest,sizeof(dest));
    fn(sameDescriptor ? &src : &dest,&src,m,count);
    return std::memcmp(beforeSrc,&src,sizeof(src)) == 0 && std::memcmp(beforeDest,&dest,sizeof(dest)) == 0;
}
static bool QSGuardedCall(QSFn fn, VxStridedData *d, VxStridedData *s, const VxMatrix *m, int count) {
#if defined(_MSC_VER) && defined(_WIN32)
    // Keep a bad 16-byte source load observable without losing the test process or page cleanup.
    __try { fn(d,s,*m,count); return true; }
    __except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) { return false; }
#else
    fn(d,s,*m,count); return true;
#endif
}
static bool QSData(QSFn fn, const VxMatrix &m, const VxVector *points, const VxVector4 *expected, int count, unsigned ss, unsigned ds, bool alias, bool wideSource = false, unsigned wBits = 0xc0000000u) {
    alignas(16) unsigned char src[2048], dest[2048], beforeSrc[2048], beforeDest[2048];
    std::memset(src,0xa5,sizeof(src)); std::memset(dest,0xa5,sizeof(dest));
    for (int i = 0; i < count; ++i) {
        if (alias || wideSource) { VxVector4 *v = new(src+64+i*ss) VxVector4(points[i].x,points[i].y,points[i].z,0); std::memcpy(&v->w,&wBits,4); }
        else new(src+64+i*ss) VxVector(points[i]);
        if (!alias) new(dest+64+i*ds) VxVector4;
    }
    std::memcpy(beforeSrc,src,sizeof(src)); std::memcpy(beforeDest,dest,sizeof(dest));
    unsigned char *out = alias ? src : dest; VxMatrix matrix = m;
    bool ok = QSCall(fn,out+64,src+64,matrix,count,ss,ds) && std::memcmp(&matrix,&m,64) == 0;
    for (int i = 0; i < count; ++i) ok = ok && QSEqual(*reinterpret_cast<const VxVector4 *>(out+64+i*ds),expected[i]);
    for (int b = 0; b < 2048; ++b) {
        bool written = false;
        for (int i = 0; i < count; ++i) written = written || (unsigned(b) >= 64+i*ds && unsigned(b) < 80+i*ds);
        if (!written) ok = ok && out[b] == (alias ? beforeSrc[b] : beforeDest[b]);
    }
    if (!alias) ok = ok && std::memcmp(src,beforeSrc,sizeof(src)) == 0;
    return ok;
}
static bool QSFlow(QSFn fn, int count, unsigned ss, unsigned ds, bool separate, bool sameDescriptor = false) {
    alignas(16) unsigned char src[2048], dest[2048], expectedSrc[2048], expectedDest[2048];
    std::memset(src,0xa5,sizeof(src)); std::memset(dest,0xa5,sizeof(dest));
    for (int i = 0; i < count; ++i) {
        if (separate) { new(src+64+i*ss) VxVector(float(i+1),float(2*i-3),float(4-i)); new(dest+64+i*ds) VxVector4; }
        else new(src+64+i*ss) VxVector4(float(i+1),float(2*i-3),float(4-i),-2);
    }
    std::memcpy(expectedSrc,src,sizeof(src)); std::memcpy(expectedDest,dest,sizeof(dest));
    unsigned char *expected = separate ? expectedDest : expectedSrc, *out = separate ? dest : src;
    const VxMatrix m = QSMatrix();
    for (int i = 0; i < count; ++i) { VxVector p; std::memcpy(&p,expectedSrc+64+i*ss,12); const VxVector4 r = QSInteger(m,p); std::memcpy(expected+64+i*ds,&r,16); }
    return QSCall(fn,out+64,src+64,m,count,ss,ds,sameDescriptor) && std::memcmp(src,expectedSrc,sizeof(src)) == 0 && std::memcmp(dest,expectedDest,sizeof(dest)) == 0;
}
static QSResult QSLayouts(QSFn fn) {
    QSResult r;
    for (int n : {1,2,3,4,5,7,8,9,16,17}) {
        for (unsigned ss : {0u,12u,16u,20u,28u}) for (unsigned ds : {0u,16u,20u,24u,32u}) { ++r.samples; r.errors += !QSFlow(fn,n,ss,ds,true); }
        for (unsigned stride : {0u,16u,20u,24u,32u}) { ++r.samples; r.errors += !QSFlow(fn,n,stride,stride,false); }
    }
    return r;
}
static QSResult QSOverlaps(QSFn fn) {
    QSResult r; const VxMatrix m = QSMatrix();
    for (int n : {4,5,8,9}) for (int ss : {0,1,2,3}) for (int ds : {0,1,2,3}) for (int delta : {-2,-1,0,1,2}) {
        VxVector4 slots[128], expected[128];
        for (int i = 0; i < 128; ++i) slots[i] = VxVector4(float(i+1),float(2*i-3),float(4-i),-2);
        std::memcpy(expected,slots,sizeof(slots));
        for (int i = 0; i < n; ++i) expected[16+delta+i*ds] = QSInteger(m,expected[16+i*ss]);
        ++r.samples; r.errors += !(QSCall(fn,slots+16+delta,slots+16,m,n,unsigned(ss*16),unsigned(ds*16)) && std::memcmp(slots,expected,sizeof(slots)) == 0);
    }
    return r;
}
static QSResult QSSharedDescriptor(QSFn fn) {
    QSResult r;
    for (int n : {1,4,5}) for (unsigned stride : {0u,16u,20u,32u}) { ++r.samples; r.errors += !QSFlow(fn,n,stride,stride,false,true); }
    return r;
}
static QSResult QSLiteral(QSFn fn) {
    const float values[4][4] = {{2,1,-1,7},{3,-2,4,-3},{-1,5,2,9},{7,11,-13,2}};
    VxMatrix m; std::memcpy(&m,values,64); VxVector points[5]; VxVector4 expected[5];
    for (int i = 0; i < 5; ++i) { points[i] = VxVector(2,-3,5); expected[i] = VxVector4(-3,44,-17,70); }
    QSResult r; ++r.samples; r.errors += !QSData(fn,m,points,expected,5,20,32,false);
    ++r.samples; r.errors += !QSData(fn,m,points,expected,5,20,20,true); return r;
}
static QSResult QSFields(QSFn fn) {
    QSResult r; const unsigned pairs[4][2] = {{12,16},{16,24},{28,0},{0,20}}; const VxVector p(2,3,5);
    for (int row = 0; row < 4; ++row) for (int col = 0; col < 4; ++col) {
        VxMatrix m; m.Clear(); m[row][col] = 7; VxVector points[5]; VxVector4 expected[5];
        for (int i = 0; i < 5; ++i) { points[i] = p; expected[i] = QSInteger(m,p); }
        for (int n : {1,5}) {
            for (const auto &pair : pairs) { ++r.samples; r.errors += !QSData(fn,m,points,expected,n,pair[0],pair[1],false); }
            for (unsigned stride : {16u,20u}) { ++r.samples; r.errors += !QSData(fn,m,points,expected,n,stride,stride,true); }
        }
    }
    return r;
}
static QSResult QSIgnored(QSFn fn) {
    QSResult r; VxMatrix m; const float values[4][4] = {{2,1,-1,7},{3,-2,4,-3},{-1,5,2,9},{7,11,-13,2}}; std::memcpy(&m,values,64);
    VxVector points[5]; VxVector4 expected[5];
    for (int i = 0; i < 5; ++i) { points[i] = VxVector(2,-3,5); expected[i] = VxVector4(-3,44,-17,70); }
    const unsigned pairs[3][2] = {{16,16},{16,24},{32,16}};
    for (unsigned bits : {0u,0x3f800000u,0x40000000u,0xc0000000u,0x7f800000u,0xff800000u,0x7fc12345u,0x80000000u}) {
        for (const auto &pair : pairs) { ++r.samples; r.errors += !QSData(fn,m,points,expected,5,pair[0],pair[1],false,true,bits); }
        ++r.samples; r.errors += !QSData(fn,m,points,expected,5,16,16,true,true,bits);
    }
    return r;
}
static QSResult QSNumerics(QSFn fn, int kind) {
    QSResult r; const int terms = kind == 0 ? 4 : 3;
    const unsigned pairs[5][2] = {{12,16},{16,24},{28,16},{0,20},{20,0}};
    for (int col = 0; col < 4; ++col) for (int rotate = 0; rotate < terms; ++rotate) {
        VxMatrix m; m.Clear(); VxVector p(1,1,1); VxVector4 e(0,0,0,0); float x[4], weights[4];
        if (kind == 0) { x[0] = std::ldexp(1.0f,80); x[1] = 1; x[2] = -x[0]; x[3] = 1; for (float &v : weights) v = 1; e[col] = 2; }
        else if (kind == 1) { x[0] = std::ldexp(1.0f,100); x[1] = -x[0]; x[2] = 1; x[3] = 2; weights[0] = weights[1] = std::ldexp(1.0f,40); weights[2] = weights[3] = 1; e[col] = 3; }
        else { for (int i = 0; i < 3; ++i) { x[i] = std::ldexp(1.0f,-100); weights[i] = std::ldexp(1.0f,-51); } x[3] = 0; weights[3] = 1; e[col] = std::ldexp(1.0f,-149); }
        for (int i = 0; i < terms; ++i) { const int at = (i+rotate)%terms; m[at][col] = x[i]; if (at < 3) p[at] = weights[i]; }
        if (terms == 3) m[3][col] = x[3];
        VxVector points[9]; VxVector4 expected[9]; for (int i = 0; i < 9; ++i) { points[i] = p; expected[i] = e; }
        for (int n : {1,3,4,5,8,9}) {
            for (const auto &pair : pairs) { ++r.samples; r.errors += !QSData(fn,m,points,expected,n,pair[0],pair[1],false); }
            ++r.samples; r.errors += !QSData(fn,m,points,expected,n,20,20,true);
        }
    }
    return r;
}
static QSResult QSNonfinite(QSFn fn) {
    QSResult r; VxVector points[5]; VxVector4 expected[5]; const unsigned pairs[3][2] = {{12,16},{16,24},{0,20}};
    for (int col = 0; col < 4; ++col) for (int kind = 0; kind < 3; ++kind) {
        VxMatrix m; m.Clear(); m[0][col] = kind == 1 ? -HUGE_VALF : HUGE_VALF; if (kind == 2) m[1][col] = -HUGE_VALF;
        for (int i = 0; i < 5; ++i) { points[i] = VxVector(1,1,1); expected[i] = VxVector4(0,0,0,0); expected[i][col] = kind == 2 ? NAN : m[0][col]; }
        for (int n : {1,5}) {
            for (const auto &pair : pairs) { ++r.samples; r.errors += !QSData(fn,m,points,expected,n,pair[0],pair[1],false); }
            ++r.samples; r.errors += !QSData(fn,m,points,expected,n,16,16,true);
        }
    }
    return r;
}
static QSResult QSEmpty(QSFn fn) {
    QSResult r; const VxMatrix m = QSMatrix(); VxVector p(1,2,3); VxVector4 out(7,8,9,10);
    for (int n : {0,-1,INT_MIN}) {
        ++r.samples; r.errors += !QSCall(fn,&out,&p,m,n,16,24) || !(p.x == 1 && p.y == 2 && p.z == 3) || !QSEqual(out,VxVector4(7,8,9,10));
        fn(nullptr,nullptr,m,n); ++r.samples;
    }
    return r;
}
static QSResult QSFootprint(QSFn fn) {
    QSResult r; const VxMatrix matrix = QSMatrix(); const VxVector p(2,-3,5); const VxVector4 expected(10,0,10,41);
    for (int shift = 0; shift < 16; shift += 4) {
        alignas(16) unsigned char source[256], dest[256], before[256]; std::memset(source,0xa5,sizeof(source)); std::memset(dest,0xa5,sizeof(dest));
        for (int i = 0; i < 5; ++i) { new(source+shift+i*20) VxVector(p); new(dest+shift+i*28) VxVector4; }
        std::memcpy(before,dest,sizeof(dest)); bool ok = QSCall(fn,dest+shift,source+shift,matrix,5,20,28);
        for (int i = 0; i < 5; ++i) ok = ok && QSEqual(*reinterpret_cast<const VxVector4 *>(dest+shift+i*28),expected);
        for (int b = 0; b < 256; ++b) { bool written = false; for (int i = 0; i < 5; ++i) written = written || (b >= shift+i*28 && b < shift+i*28+16); if (!written) ok = ok && before[b] == dest[b]; }
        ++r.samples; r.errors += !ok;
    }
#if defined(_WIN32)
    SYSTEM_INFO info; GetSystemInfo(&info); const unsigned size = info.dwPageSize;
    unsigned char *memory = static_cast<unsigned char *>(VirtualAlloc(nullptr,15*size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if (!memory) { ++r.errors; return r; } DWORD old;
    for (unsigned page : {0u,2u,3u,5u,6u,8u,9u,11u,12u,14u}) {
        if (!VirtualProtect(memory+page*size,size,PAGE_NOACCESS,&old)) { VirtualFree(memory,0,MEM_RELEASE); ++r.errors; return r; }
    }
    const unsigned pairs[5][2] = {{12,16},{16,24},{0,20},{20,0},{0,0}};
    for (int n : {1,4,5}) for (const auto &pair : pairs) for (int edge = 0; edge < 32; ++edge) {
        const unsigned ss = pair[0], ds = pair[1], srcSpan = (n-1)*ss+12, destSpan = (n-1)*ds+16;
        VxMatrix *m = new(memory+size+(edge&1 ? size-64 : 0)) VxMatrix(matrix);
        unsigned char *source = memory+4*size+(edge&2 ? size-srcSpan : 0), *dest = memory+7*size+(edge&4 ? size-destSpan : 0);
        std::memset(source,0xa5,srcSpan); std::memset(dest,0xa5,destSpan);
        for (int i = 0; i < n; ++i) { new(source+i*ss) VxVector(p); new(dest+i*ds) VxVector4; }
        VxStridedData *s = new(memory+10*size+(edge&8 ? size-sizeof(VxStridedData) : 0)) VxStridedData(source,ss);
        VxStridedData *d = new(memory+13*size+(edge&16 ? size-sizeof(VxStridedData) : 0)) VxStridedData(dest,ds);
        for (unsigned page : {1u,4u,10u,13u}) if (!VirtualProtect(memory+page*size,size,PAGE_READONLY,&old)) { VirtualFree(memory,0,MEM_RELEASE); ++r.errors; return r; }
        bool ok = QSGuardedCall(fn,d,s,m,n); r.accessViolations += !ok;
        for (int i = 0; i < n; ++i) ok = ok && QSEqual(*reinterpret_cast<const VxVector4 *>(dest+i*ds),expected);
        for (unsigned b = 0; b < destSpan; ++b) if (ds && b%ds >= 16) ok = ok && dest[b] == 0xa5;
        ++r.samples; r.errors += !ok;
        for (unsigned page : {1u,4u,10u,13u}) if (!VirtualProtect(memory+page*size,size,PAGE_READWRITE,&old)) { VirtualFree(memory,0,MEM_RELEASE); ++r.errors; return r; }
    }
    VirtualFree(memory,0,MEM_RELEASE);
#endif
    return r;
}
#ifndef VX_MATRIX_VECTOR4_STRIDED_PROBE
static void QSExpect(QSResult r, int count) { EXPECT_EQ(r.samples,count); EXPECT_EQ(r.errors,0); EXPECT_EQ(r.accessViolations,0); }
TEST(MatrixVector4StridedReference, LiteralPointToXYZWWithoutPerspectiveDivision) { QSExpect(QSLiteral(QSFree),2); }
TEST(MatrixVector4StridedReference, IndependentUnsignedByteStridesAndCounts) { QSExpect(QSLayouts(QSFree),300); }
TEST(MatrixVector4StridedReference, AllSixteenFieldsInfluenceXYZW) { QSExpect(QSFields(QSFree),192); }
TEST(MatrixVector4StridedReference, InputFourthComponentIsIgnored) { QSExpect(QSIgnored(QSFree),32); }
TEST(MatrixVector4StridedReference, OverlappingElementStreamsUseSequentialReads) { QSExpect(QSOverlaps(QSFree),320); }
TEST(MatrixVector4StridedReference, SameDescriptorIsUnchanged) { QSExpect(QSSharedDescriptor(QSFree),12); }
TEST(MatrixVector4StridedReference, CancellationAcrossFastAndGeneralLayouts) { QSExpect(QSNumerics(QSFree,0),576); }
TEST(MatrixVector4StridedReference, FiniteResultsSurviveProductOverflow) { QSExpect(QSNumerics(QSFree,1),432); }
TEST(MatrixVector4StridedReference, ProductTailsFormSubnormalResults) { QSExpect(QSNumerics(QSFree,2),432); }
TEST(MatrixVector4StridedReference, NonfiniteXYZClassification) { QSExpect(QSNonfinite(QSFree),96); }
TEST(MatrixVector4StridedReference, NonpositiveCountsDoNotAccessArrays) { QSExpect(QSEmpty(QSFree),6); }
TEST(MatrixVector4StridedReference, ExactXYZInputAndXYZWOutputPageEdges) {
#if defined(_WIN32)
    QSExpect(QSFootprint(QSFree),484);
#else
    QSExpect(QSFootprint(QSFree),4);
#endif
}
#if defined(VX_SIMD_SSE)
TEST(MatrixVector4StridedReference, PackedIndependentStridesAndCounts) { QSExpect(QSLayouts(QSPacked),300); }
TEST(MatrixVector4StridedReference, PackedFieldsIgnoredColumnAndSharedDescriptor) { QSExpect(QSFields(QSPacked),192); QSExpect(QSIgnored(QSPacked),32); QSExpect(QSSharedDescriptor(QSPacked),12); }
TEST(MatrixVector4StridedReference, PackedOverlapOrder) { QSExpect(QSOverlaps(QSPacked),320); }
TEST(MatrixVector4StridedReference, PackedPointToXYZWNumericalPaths) { QSExpect(QSNumerics(QSPacked,0),576); QSExpect(QSNumerics(QSPacked,1),432); QSExpect(QSNumerics(QSPacked,2),432); }
TEST(MatrixVector4StridedReference, PackedNonfiniteAndExactBoundaries) {
    QSExpect(QSNonfinite(QSPacked),96); QSExpect(QSEmpty(QSPacked),6);
#if defined(_WIN32)
    QSExpect(QSFootprint(QSPacked),484);
#else
    QSExpect(QSFootprint(QSPacked),4);
#endif
}
#endif
#endif
