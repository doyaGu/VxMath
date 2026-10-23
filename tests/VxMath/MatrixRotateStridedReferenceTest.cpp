#include <cmath>
#include <cstring>
#include <new>
#include <climits>
#include <initializer_list>
#ifndef VX_MATRIX_ROTATE_STRIDED_PROBE
#include "VxMatrix.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef void (*RSFn)(VxStridedData *, VxStridedData *, const VxMatrix &, int);
struct RSResult { int samples = 0, errors = 0; };
static void RSFree(VxStridedData *d, VxStridedData *s, const VxMatrix &m, int n) { Vx3DRotateVectorStrided(d,s,m,n); }
#if defined(VX_SIMD_SSE)
static void RSPacked(VxStridedData *d, VxStridedData *s, const VxMatrix &m, int n) { VxSIMDRotateVectorStrided(d,s,&m,n); }
#endif
static unsigned RSNext(unsigned &seed) { seed = seed*1664525u+1013904223u; return seed; }
static bool RSEqual(const VxVector &a, const VxVector &b) {
    for (int c = 0; c < 3; ++c) if (!(a[c] == b[c] || (std::isnan(a[c]) && std::isnan(b[c])))) return false;
    return true;
}
static VxVector RSInteger(const VxMatrix &m, const VxVector &p) {
    VxVector out;
    for (int c = 0; c < 3; ++c) {
        long long value = 0;
        for (int r = 0; r < 3; ++r) value += static_cast<long long>(m[r][c])*static_cast<long long>(p[r]);
        out[c] = float(value);
    }
    return out;
}
static VxMatrix RSMatrix() {
    VxMatrix m; m.Clear(); m[0][1] = 1; m[1][0] = -1; m[2][2] = 1;
    m[3][0] = 7; m[3][1] = -2; m[3][2] = 5; m[0][3] = 19; m[3][3] = 3; return m;
}
static bool RSCall(RSFn fn, void *out, void *input, const VxMatrix &m, int count, unsigned ss, unsigned ds, bool sameDescriptor = false) {
    VxStridedData src(input,ss), dest(out,ds); unsigned char beforeSrc[sizeof(src)], beforeDest[sizeof(dest)];
    std::memcpy(beforeSrc,&src,sizeof(src)); std::memcpy(beforeDest,&dest,sizeof(dest));
    fn(sameDescriptor ? &src : &dest,&src,m,count);
    return std::memcmp(beforeSrc,&src,sizeof(src)) == 0 && std::memcmp(beforeDest,&dest,sizeof(dest)) == 0;
}
static bool RSData(RSFn fn, const VxMatrix &m, const VxVector *points, const VxVector *expected, int count, unsigned ss, unsigned ds, bool alias) {
    alignas(16) unsigned char src[2048], dest[2048], beforeSrc[2048], beforeDest[2048];
    std::memset(src,0xa5,sizeof(src)); std::memset(dest,0xa5,sizeof(dest));
    for (int i = 0; i < count; ++i) { new(src+64+i*ss) VxVector(points[i]); if (!alias) new(dest+64+i*ds) VxVector; }
    std::memcpy(beforeSrc,src,sizeof(src)); std::memcpy(beforeDest,dest,sizeof(dest));
    unsigned char *out = alias ? src : dest; VxMatrix matrix = m;
    bool ok = RSCall(fn,out+64,src+64,matrix,count,ss,ds) && std::memcmp(&matrix,&m,64) == 0;
    for (int i = 0; i < count; ++i) ok = ok && RSEqual(*reinterpret_cast<const VxVector *>(out+64+i*ds),expected[i]);
    for (int b = 0; b < 2048; ++b) {
        bool written = false;
        for (int i = 0; i < count; ++i) written = written || (unsigned(b) >= 64+i*ds && unsigned(b) < 76+i*ds);
        if (!written) ok = ok && out[b] == (alias ? beforeSrc[b] : beforeDest[b]);
    }
    if (!alias) ok = ok && std::memcmp(src,beforeSrc,sizeof(src)) == 0;
    return ok;
}
static bool RSFlow(RSFn fn, int count, unsigned ss, unsigned ds, bool separate, bool sameDescriptor = false) {
    alignas(16) unsigned char src[2048], dest[2048], expectedSrc[2048], expectedDest[2048];
    std::memset(src,0xa5,sizeof(src)); std::memset(dest,0xa5,sizeof(dest));
    for (int i = 0; i < count; ++i) { new(src+64+i*ss) VxVector(float(i+1),float(2*i-3),float(4-i)); if (separate) new(dest+64+i*ds) VxVector; }
    std::memcpy(expectedSrc,src,sizeof(src)); std::memcpy(expectedDest,dest,sizeof(dest));
    unsigned char *expected = separate ? expectedDest : expectedSrc, *out = separate ? dest : src;
    const VxMatrix m = RSMatrix();
    for (int i = 0; i < count; ++i) { VxVector p; std::memcpy(&p,expectedSrc+64+i*ss,12); const VxVector r = RSInteger(m,p); std::memcpy(expected+64+i*ds,&r,12); }
    return RSCall(fn,out+64,src+64,m,count,ss,ds,sameDescriptor) && std::memcmp(src,expectedSrc,sizeof(src)) == 0 && std::memcmp(dest,expectedDest,sizeof(dest)) == 0;
}
static RSResult RSLayouts(RSFn fn) {
    RSResult r;
    for (int n : {1,2,3,4,5,7,8,9,16,17}) {
        for (unsigned ss : {0u,12u,16u,20u,28u}) for (unsigned ds : {0u,12u,16u,24u,32u}) { ++r.samples; r.errors += !RSFlow(fn,n,ss,ds,true); }
        for (unsigned stride : {0u,12u,16u,20u,28u}) { ++r.samples; r.errors += !RSFlow(fn,n,stride,stride,false); }
    }
    return r;
}
static RSResult RSOverlaps(RSFn fn) {
    RSResult r; const VxMatrix m = RSMatrix();
    for (int n : {4,5,8,9}) for (int ss : {0,1,2,3}) for (int ds : {0,1,2,3}) for (int delta : {-2,-1,0,1,2}) {
        VxVector slots[128], expected[128];
        for (int i = 0; i < 128; ++i) slots[i] = VxVector(float(i+1),float(2*i-3),float(4-i));
        std::memcpy(expected,slots,sizeof(slots));
        for (int i = 0; i < n; ++i) expected[16+delta+i*ds] = RSInteger(m,expected[16+i*ss]);
        ++r.samples; r.errors += !(RSCall(fn,slots+16+delta,slots+16,m,n,unsigned(ss*12),unsigned(ds*12)) && std::memcmp(slots,expected,sizeof(slots)) == 0);
    }
    return r;
}
static RSResult RSSharedDescriptor(RSFn fn) {
    RSResult r;
    for (int n : {1,4,5}) for (unsigned stride : {0u,12u,16u,20u}) { ++r.samples; r.errors += !RSFlow(fn,n,stride,stride,false,true); }
    return r;
}
static RSResult RSLiteral(RSFn fn) {
    const float values[4][4] = {{2,1,-1,7},{3,-2,4,-3},{-1,5,2,9},{7,11,-13,2}};
    VxMatrix m; std::memcpy(&m,values,64); VxVector points[5], expected[5];
    for (int i = 0; i < 5; ++i) { points[i] = VxVector(2,-3,5); expected[i] = VxVector(-10,33,-4); }
    RSResult r; ++r.samples; r.errors += !RSData(fn,m,points,expected,5,20,32,false);
    ++r.samples; r.errors += !RSData(fn,m,points,expected,5,20,20,true); return r;
}
static RSResult RSFields(RSFn fn) {
    RSResult r; const unsigned pairs[4][2] = {{12,12},{16,24},{28,0},{0,20}}; const VxVector p(2,3,5);
    for (int row = 0; row < 4; ++row) for (int col = 0; col < 4; ++col) {
        VxMatrix m; m.Clear(); m[row][col] = 7; VxVector points[5], expected[5];
        for (int i = 0; i < 5; ++i) { points[i] = p; expected[i] = RSInteger(m,p); }
        for (int n : {1,5}) {
            for (const auto &pair : pairs) { ++r.samples; r.errors += !RSData(fn,m,points,expected,n,pair[0],pair[1],false); }
            for (unsigned stride : {12u,20u}) { ++r.samples; r.errors += !RSData(fn,m,points,expected,n,stride,stride,true); }
        }
    }
    return r;
}
static RSResult RSIgnored(RSFn fn) {
    RSResult r; VxVector points[5], expected[5]; const VxMatrix base = RSMatrix(); const unsigned pairs[3][2] = {{12,12},{16,24},{0,32}};
    for (int i = 0; i < 5; ++i) { points[i] = VxVector(2,-3,5); expected[i] = VxVector(3,2,5); }
    for (int field = 0; field < 7; ++field) for (unsigned bits : {0u,0x3f800000u,0x7f800000u,0xff800000u,0x7fc12345u,0x80000000u,0x00800000u,0x7f7fffffu}) {
        VxMatrix m = base; std::memcpy(&m[field < 3 ? field : 3][field < 3 ? 3 : field-3],&bits,4);
        for (const auto &pair : pairs) { ++r.samples; r.errors += !RSData(fn,m,points,expected,5,pair[0],pair[1],false); }
        ++r.samples; r.errors += !RSData(fn,m,points,expected,5,16,16,true);
    }
    return r;
}
static RSResult RSNumerics(RSFn fn, int kind) {
    RSResult r; const int terms = 3;
    const unsigned pairs[5][2] = {{12,12},{16,24},{28,16},{0,20},{20,0}};
    for (int col = 0; col < 3; ++col) for (int rotate = 0; rotate < terms; ++rotate) {
        VxMatrix m; m.Clear(); VxVector p(1,1,1), e(0,0,0); float x[4], weights[4];
        if (kind == 0) { x[0] = std::ldexp(1.0f,80); x[1] = 1; x[2] = -x[0]; x[3] = 0; for (float &v : weights) v = 1; e[col] = 1; }
        else if (kind == 1) { x[0] = std::ldexp(1.0f,100); x[1] = -x[0]; x[2] = 1; x[3] = 0; weights[0] = weights[1] = std::ldexp(1.0f,40); weights[2] = weights[3] = 1; e[col] = 1; }
        else { for (int i = 0; i < 3; ++i) { x[i] = std::ldexp(1.0f,-100); weights[i] = std::ldexp(1.0f,-51); } x[3] = 0; weights[3] = 1; e[col] = std::ldexp(1.0f,-149); }
        for (int i = 0; i < terms; ++i) { const int at = (i+rotate)%terms; m[at][col] = x[i]; if (at < 3) p[at] = weights[i]; }

        VxVector points[9], expected[9]; for (int i = 0; i < 9; ++i) { points[i] = p; expected[i] = e; }
        for (int n : {1,3,4,5,8,9}) {
            for (const auto &pair : pairs) { ++r.samples; r.errors += !RSData(fn,m,points,expected,n,pair[0],pair[1],false); }
            ++r.samples; r.errors += !RSData(fn,m,points,expected,n,20,20,true);
        }
    }
    return r;
}
static RSResult RSNonfinite(RSFn fn) {
    RSResult r; VxVector points[5], expected[5]; const unsigned pairs[3][2] = {{12,12},{16,24},{0,20}};
    for (int col = 0; col < 3; ++col) for (int kind = 0; kind < 3; ++kind) {
        VxMatrix m; m.Clear(); m[0][col] = kind == 1 ? -HUGE_VALF : HUGE_VALF; if (kind == 2) m[1][col] = -HUGE_VALF;
        for (int i = 0; i < 5; ++i) { points[i] = VxVector(1,1,1); expected[i] = VxVector(0,0,0); expected[i][col] = kind == 2 ? NAN : m[0][col]; }
        for (int n : {1,5}) {
            for (const auto &pair : pairs) { ++r.samples; r.errors += !RSData(fn,m,points,expected,n,pair[0],pair[1],false); }
            ++r.samples; r.errors += !RSData(fn,m,points,expected,n,16,16,true);
        }
    }
    return r;
}
static RSResult RSEmpty(RSFn fn) {
    RSResult r; const VxMatrix m = RSMatrix(); VxVector p(1,2,3), out(7,8,9);
    for (int n : {0,-1,INT_MIN}) {
        ++r.samples; r.errors += !RSCall(fn,&out,&p,m,n,16,24) || !RSEqual(p,VxVector(1,2,3)) || !RSEqual(out,VxVector(7,8,9));
        fn(nullptr,nullptr,m,n); ++r.samples;
    }
    return r;
}
static RSResult RSFootprint(RSFn fn) {
    RSResult r; const VxMatrix matrix = RSMatrix(); const VxVector p(2,-3,5), expected(3,2,5);
    for (int shift = 0; shift < 16; shift += 4) {
        alignas(16) unsigned char source[256], dest[256], before[256]; std::memset(dest,0xa5,sizeof(dest));
        for (int i = 0; i < 5; ++i) { new(source+shift+i*20) VxVector(p); new(dest+shift+i*28) VxVector; }
        std::memcpy(before,dest,sizeof(dest)); bool ok = RSCall(fn,dest+shift,source+shift,matrix,5,20,28);
        for (int i = 0; i < 5; ++i) ok = ok && RSEqual(*reinterpret_cast<const VxVector *>(dest+shift+i*28),expected);
        for (int b = 0; b < 256; ++b) { bool written = false; for (int i = 0; i < 5; ++i) written = written || (b >= shift+i*28 && b < shift+i*28+12); if (!written) ok = ok && before[b] == dest[b]; }
        ++r.samples; r.errors += !ok;
    }
#if defined(_WIN32)
    SYSTEM_INFO info; GetSystemInfo(&info); const unsigned size = info.dwPageSize;
    unsigned char *memory = static_cast<unsigned char *>(VirtualAlloc(nullptr,15*size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if (!memory) { ++r.errors; return r; } DWORD old;
    for (unsigned page : {0u,2u,3u,5u,6u,8u,9u,11u,12u,14u}) {
        if (!VirtualProtect(memory+page*size,size,PAGE_NOACCESS,&old)) { VirtualFree(memory,0,MEM_RELEASE); ++r.errors; return r; }
    }
    const unsigned pairs[5][2] = {{12,12},{16,24},{0,20},{20,0},{0,0}};
    for (int n : {1,4,5}) for (const auto &pair : pairs) for (int edge = 0; edge < 32; ++edge) {
        const unsigned ss = pair[0], ds = pair[1], srcSpan = (n-1)*ss+12, destSpan = (n-1)*ds+12;
        VxMatrix *m = new(memory+size+(edge&1 ? size-64 : 0)) VxMatrix(matrix);
        unsigned char *source = memory+4*size+(edge&2 ? size-srcSpan : 0), *dest = memory+7*size+(edge&4 ? size-destSpan : 0);
        std::memset(source,0xa5,srcSpan); std::memset(dest,0xa5,destSpan);
        for (int i = 0; i < n; ++i) { new(source+i*ss) VxVector(p); new(dest+i*ds) VxVector; }
        VxStridedData *s = new(memory+10*size+(edge&8 ? size-sizeof(VxStridedData) : 0)) VxStridedData(source,ss);
        VxStridedData *d = new(memory+13*size+(edge&16 ? size-sizeof(VxStridedData) : 0)) VxStridedData(dest,ds);
        for (unsigned page : {1u,4u,10u,13u}) if (!VirtualProtect(memory+page*size,size,PAGE_READONLY,&old)) { VirtualFree(memory,0,MEM_RELEASE); ++r.errors; return r; }
        fn(d,s,*m,n); bool ok = true;
        for (int i = 0; i < n; ++i) ok = ok && RSEqual(*reinterpret_cast<const VxVector *>(dest+i*ds),expected);
        for (unsigned b = 0; b < destSpan; ++b) if (ds && b%ds >= 12) ok = ok && dest[b] == 0xa5;
        ++r.samples; r.errors += !ok;
        for (unsigned page : {1u,4u,10u,13u}) if (!VirtualProtect(memory+page*size,size,PAGE_READWRITE,&old)) { VirtualFree(memory,0,MEM_RELEASE); ++r.errors; return r; }
    }
    VirtualFree(memory,0,MEM_RELEASE);
#endif
    return r;
}
static RSResult RSLinearRows(RSFn fn) {
    RSResult r;
#if defined(_WIN32)
    SYSTEM_INFO info; GetSystemInfo(&info); const unsigned size = info.dwPageSize;
    unsigned char *memory = static_cast<unsigned char *>(VirtualAlloc(nullptr,2*size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if (!memory) { ++r.errors; return r; }
    const VxMatrix reference = RSMatrix(); VxMatrix *m = new(memory+size-48) VxMatrix(reference); DWORD old;
    if (!VirtualProtect(memory,size,PAGE_READONLY,&old) || !VirtualProtect(memory+size,size,PAGE_NOACCESS,&old)) { VirtualFree(memory,0,MEM_RELEASE); ++r.errors; return r; }
    const unsigned pairs[7][2] = {{16,16},{16,32},{0,16},{16,0},{0,0},{16,16},{0,0}};
    for (int n : {1,4,5}) for (int layout = 0; layout < 7; ++layout) {
        VxVector4 source[16], dest[16], expectedSrc[16], expectedDest[16];
        for (int i=0;i<16;++i) { source[i] = VxVector4(float(i+1),float(2*i-3),float(4-i),123.5f); dest[i]=VxVector4(7,8,9,456.5f); }
        std::memcpy(expectedSrc,source,sizeof(source)); std::memcpy(expectedDest,dest,sizeof(dest));
        const unsigned ss=pairs[layout][0], ds=pairs[layout][1]; const bool alias=layout>=5;
        for (int i=0;i<n;++i) {
            const VxVector p(expectedSrc[i*ss/16].x,expectedSrc[i*ss/16].y,expectedSrc[i*ss/16].z), v=RSInteger(reference,p);
            VxVector4 &out=alias ? expectedSrc[i*ds/16] : expectedDest[i*ds/16]; out.x=v.x;out.y=v.y;out.z=v.z;
        }
        ++r.samples; r.errors += !(RSCall(fn,alias ? source : dest,source,*m,n,ss,ds) && std::memcmp(source,expectedSrc,sizeof(source))==0 && std::memcmp(dest,expectedDest,sizeof(dest))==0);
    }
    VirtualFree(memory,0,MEM_RELEASE);
#endif
    return r;
}
#ifndef VX_MATRIX_ROTATE_STRIDED_PROBE
static void RSExpect(RSResult r, int count) { EXPECT_EQ(r.samples,count); EXPECT_EQ(r.errors,0); }
TEST(MatrixRotateStridedReference, LiteralLinearXYZWithoutTranslation) { RSExpect(RSLiteral(RSFree),2); }
TEST(MatrixRotateStridedReference, IndependentUnsignedByteStridesAndCounts) { RSExpect(RSLayouts(RSFree),300); }
TEST(MatrixRotateStridedReference, AllFieldsAndRecordPadding) { RSExpect(RSFields(RSFree),192); }
TEST(MatrixRotateStridedReference, TranslationRowAndFourthColumnAreIgnored) { RSExpect(RSIgnored(RSFree),224); }
TEST(MatrixRotateStridedReference, OverlappingElementStreamsUseSequentialReads) { RSExpect(RSOverlaps(RSFree),320); }
TEST(MatrixRotateStridedReference, SameDescriptorIsUnchanged) { RSExpect(RSSharedDescriptor(RSFree),12); }
TEST(MatrixRotateStridedReference, CancellationAcrossFastAndGeneralLayouts) { RSExpect(RSNumerics(RSFree,0),324); }
TEST(MatrixRotateStridedReference, FiniteResultsSurviveProductOverflow) { RSExpect(RSNumerics(RSFree,1),324); }
TEST(MatrixRotateStridedReference, ProductTailsFormSubnormalResults) { RSExpect(RSNumerics(RSFree,2),324); }
TEST(MatrixRotateStridedReference, NonfiniteXYZClassification) { RSExpect(RSNonfinite(RSFree),72); }
TEST(MatrixRotateStridedReference, NonpositiveCountsDoNotAccessArrays) { RSExpect(RSEmpty(RSFree),6); }
TEST(MatrixRotateStridedReference, ExactXYZAndReadOnlyDescriptorPageEdges) {
#if defined(_WIN32)
    RSExpect(RSFootprint(RSFree),484);
#else
    RSExpect(RSFootprint(RSFree),4);
#endif
}
TEST(MatrixRotateStridedReference, TranslationRowIsNotRead) {
#if defined(_WIN32)
    RSExpect(RSLinearRows(RSFree),21);
#else
    RSExpect(RSLinearRows(RSFree),0);
#endif
}
#if defined(VX_SIMD_SSE)
TEST(MatrixRotateStridedReference, PackedIndependentStridesAndCounts) { RSExpect(RSLayouts(RSPacked),300); }
TEST(MatrixRotateStridedReference, PackedFieldsIgnoredColumnAndSharedDescriptor) { RSExpect(RSFields(RSPacked),192); RSExpect(RSIgnored(RSPacked),224); RSExpect(RSSharedDescriptor(RSPacked),12); }
TEST(MatrixRotateStridedReference, PackedOverlapOrder) { RSExpect(RSOverlaps(RSPacked),320); }
TEST(MatrixRotateStridedReference, PackedFastAndGeneralNumericalPaths) { RSExpect(RSNumerics(RSPacked,0),324); RSExpect(RSNumerics(RSPacked,1),324); RSExpect(RSNumerics(RSPacked,2),324); }
TEST(MatrixRotateStridedReference, PackedNonfiniteAndExactBoundaries) {
    RSExpect(RSNonfinite(RSPacked),72); RSExpect(RSEmpty(RSPacked),6);
#if defined(_WIN32)
    RSExpect(RSFootprint(RSPacked),484); RSExpect(RSLinearRows(RSPacked),21);
#else
    RSExpect(RSFootprint(RSPacked),4);
#endif
}
#endif
#endif
