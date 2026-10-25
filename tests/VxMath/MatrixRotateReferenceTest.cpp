#include <cmath>
#include <cstring>
#include <new>
#include <initializer_list>
#ifndef VX_MATRIX_ROTATE_PROBE
#include "VxMatrix.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef void (*MRFn)(VxVector *, const VxMatrix &, const VxVector *);
struct MRResult { int samples = 0, errors = 0; };
static void MRFree(VxVector *out, const VxMatrix &m, const VxVector *p) { Vx3DRotateVector(out,m,p); }
#if defined(VX_SIMD_SSE)
static void MRPacked(VxVector *out, const VxMatrix &m, const VxVector *p) { VxSIMDRotateVectorM(out,&m,p); }
#endif
static void MRMember(VxVector *out, const VxMatrix &m, const VxVector *p) { VxVector temp = *p; temp.Rotate(m); *out = temp; }
static void MRValue(VxVector *out, const VxMatrix &m, const VxVector *p) { *out = Rotate(m,*p); }
static unsigned MRNext(unsigned &seed) { seed = seed*1664525u+1013904223u; return seed; }
static bool MREqual(const VxVector &a, const VxVector &b) {
    for (int i = 0; i < 3; ++i) if (!(a[i] == b[i] || (std::isnan(a[i]) && std::isnan(b[i])))) return false;
    return true;
}
static VxVector MRIntegerExpected(const VxMatrix &m, const VxVector &p) {
    VxVector result;
    // Integer-lattice images of the origin and three basis directions are exact.
    for (int c = 0; c < 3; ++c) {
        long long value = 0;
        for (int axis = 0; axis < 3; ++axis) value += static_cast<long long>(p[axis])*static_cast<long long>(m[axis][c]);
        result[c] = float(value);
    }
    return result;
}
static bool MRExecute(VxVector &out, MRFn fn, const VxMatrix &m, const VxVector &point, bool alias) {
    VxMatrix input = m; VxVector p = point;
    if (alias) { fn(&p,input,&p); out = p; }
    else { fn(&out,input,&p); if (std::memcmp(&p,&point,12) != 0) return false; }
    return std::memcmp(&input,&m,64) == 0;
}
static void MRLiteralInput(VxMatrix &m, VxVector &point) {
    const float values[4][4] = {{2,1,-1,7},{3,-2,4,-3},{-1,5,2,9},{7,11,-13,2}};
    std::memcpy(&m,values,64); point = VxVector(2,-3,5);
}
static MRResult MRLiteral(MRFn fn) {
    VxMatrix m; VxVector p; MRLiteralInput(m,p); MRResult result;
    for (bool alias : {false,true}) { VxVector out; ++result.samples; result.errors += !(MRExecute(out,fn,m,p,alias) && MREqual(out,VxVector(-10,33,-4))); }
    return result;
}
static MRResult MRIntegers(MRFn fn) {
    MRResult result; unsigned seed = 293;
    for (int sample = 0; sample < 1024; ++sample) {
        VxMatrix m; VxVector p;
        for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) m[r][c] = float(int(MRNext(seed)%17)-8);
        for (int c = 0; c < 3; ++c) p[c] = float(int(MRNext(seed)%17)-8);
        const VxVector expected = MRIntegerExpected(m,p);
        for (bool alias : {false,true}) { VxVector out; ++result.samples; result.errors += !(MRExecute(out,fn,m,p,alias) && MREqual(out,expected)); }
    }
    return result;
}
static MRResult MRFields(MRFn fn) {
    MRResult result; const VxVector p(2,3,5);
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) {
        VxMatrix m; m.Clear(); m[r][c] = 7; VxVector expected(0,0,0);
        if (r < 3 && c < 3) expected[c] = 7*p[r];
        for (bool alias : {false,true}) { VxVector out; ++result.samples; result.errors += !(MRExecute(out,fn,m,p,alias) && MREqual(out,expected)); }
    }
    return result;
}
static MRResult MRIgnored(MRFn fn) {
    const unsigned patterns[] = {0,0x3f800000,0x40000000,0xc0400000,0x7f800000,0xff800000,0x7fc12345,0x80000000};
    MRResult result;
    for (unsigned pattern : patterns) for (int field : {3,7,11,12,13,14,15}) {
        VxMatrix m; VxVector p; MRLiteralInput(m,p); std::memcpy(&m[field/4][field%4],&pattern,4);
        for (bool alias : {false,true}) { VxVector out; ++result.samples; result.errors += !(MRExecute(out,fn,m,p,alias) && MREqual(out,VxVector(-10,33,-4))); }
    }
    return result;
}
static MRResult MRVector4Canary(MRFn fn) {
    MRResult result;
    for (unsigned pattern : {0u,0x3f800000u,0x40000000u,0x7f800000u,0xff800000u,0x7fc12345u,0x80000000u,0x00800001u}) {
        VxMatrix m; VxVector p; MRLiteralInput(m,p);
        for (bool alias : {false,true}) {
            VxVector4 input(p.x,p.y,p.z,0), output; std::memcpy(&input.w,&pattern,4); std::memcpy(&output.w,&pattern,4);
            VxVector4 before = input; fn(alias ? &input : &output,m,&input); const VxVector4 &actual = alias ? input : output;
            unsigned after; std::memcpy(&after,&actual.w,4);
            const bool ok = MREqual(actual,VxVector(-10,33,-4)) && after == pattern && (alias || std::memcmp(&input,&before,16) == 0);
            ++result.samples; result.errors += !ok;
        }
    }
    return result;
}
static MRResult MRMatrixRows(MRFn fn) {
    MRResult result;
    for (int inRow = -1; inRow < 4; ++inRow) for (int outRow = 0; outRow < 4; ++outRow) {
        VxMatrix m; VxVector p; MRLiteralInput(m,p);
        const VxVector *input = inRow < 0 ? &p : static_cast<const VxVector *>(&m[inRow]);
        const VxVector expected = MRIntegerExpected(m,*input); VxMatrix before = m;
        fn(static_cast<VxVector *>(&m[outRow]),m,input);
        bool ok = MREqual(m[outRow],expected);
        for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) if (r != outRow || c == 3) ok = ok && std::memcmp(&m[r][c],&before[r][c],4) == 0;
        ++result.samples; result.errors += !ok;
    }
    return result;
}
static MRResult MRNumerics(MRFn fn, int kind) {
    MRResult result;
    for (int col = 0; col < 3; ++col) for (int rotate = 0; rotate < 3; ++rotate) {
        VxMatrix m; m.Clear(); VxVector p; float x[3], v[3], expected;
        if (kind == 0) { x[0] = std::ldexp(1.0f,30); x[1] = 1; x[2] = -x[0]; v[0] = v[1] = v[2] = 1; expected = 1; }
        else if (kind == 1) { x[0] = std::ldexp(1.0f,100); x[1] = -x[0]; x[2] = x[0]; v[0] = v[1] = std::ldexp(1.0f,40); v[2] = std::ldexp(1.0f,20); expected = std::ldexp(1.0f,120); }
        else { for (int i = 0; i < 3; ++i) { x[i] = std::ldexp(1.0f,-100); v[i] = std::ldexp(1.0f,-51); } expected = std::ldexp(1.0f,-149); }
        for (int i = 0; i < 3; ++i) { const int axis = (i+rotate)%3; m[axis][col] = x[i]; p[axis] = v[i]; }
        for (bool alias : {false,true}) { VxVector out; ++result.samples; result.errors += !(MRExecute(out,fn,m,p,alias) && out[col] == expected); }
    }
    return result;
}
static MRResult MRNonfinite(MRFn fn) {
    MRResult result;
    for (int col = 0; col < 3; ++col) for (int kind = 0; kind < 3; ++kind) {
        VxMatrix m; m.Clear(); m[0][col] = kind == 1 ? -HUGE_VALF : HUGE_VALF;
        if (kind == 2) m[1][col] = -HUGE_VALF;
        const VxVector p(1,1,1);
        for (bool alias : {false,true}) {
            VxVector out; const bool inputs = MRExecute(out,fn,m,p,alias);
            const bool ok = kind == 2 ? std::isnan(out[col]) : std::isinf(out[col]) && std::signbit(out[col]) == (kind == 1);
            ++result.samples; result.errors += !(inputs && ok);
        }
    }
    return result;
}
static MRResult MRFootprint(MRFn fn) {
    MRResult result; VxMatrix m; VxVector p; MRLiteralInput(m,p); const VxVector expected(-10,33,-4);
    for (int shift = 0; shift < 16; shift += 4) {
        alignas(16) unsigned char bytes[224], before[224]; std::memset(bytes,0xa5,sizeof(bytes));
        VxMatrix *pm = new(bytes+16+shift) VxMatrix(m); VxVector *pv = new(bytes+112+shift) VxVector(p), *pr = new(bytes+160+shift) VxVector;
        std::memcpy(before,bytes,sizeof(bytes)); fn(pr,*pm,pv); bool ok = MREqual(*pr,expected);
        for (int i = 0; i < 224; ++i) if (i < 160+shift || i >= 172+shift) ok = ok && bytes[i] == before[i];
        ++result.samples; result.errors += !ok; pm->~VxMatrix(); pv->~VxVector(); pr->~VxVector();
    }
#if defined(_WIN32)
    SYSTEM_INFO info; GetSystemInfo(&info); const unsigned size = info.dwPageSize;
    unsigned char *memory = static_cast<unsigned char *>(VirtualAlloc(nullptr,9*size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if (!memory) { ++result.errors; return result; } DWORD old; bool ok = true;
    for (unsigned page : {0u,2u,3u,5u,6u,8u}) ok = ok && VirtualProtect(memory+page*size,size,PAGE_NOACCESS,&old);
    if (!ok) { VirtualFree(memory,0,MEM_RELEASE); ++result.errors; return result; }
    for (int i = 0; i < 8; ++i) {
        VxMatrix *pm = new(memory+size+(i&1 ? size-64 : 0)) VxMatrix(m);
        VxVector *pv = new(memory+4*size+(i&2 ? size-12 : 0)) VxVector(p), *pr = new(memory+7*size+(i&4 ? size-12 : 0)) VxVector;
        ok = VirtualProtect(memory+size,size,PAGE_READONLY,&old) && VirtualProtect(memory+4*size,size,PAGE_READONLY,&old);
        if (!ok) { ++result.errors; break; } fn(pr,*pm,pv); ++result.samples; result.errors += !MREqual(*pr,expected);
        VirtualProtect(memory+size,size,PAGE_READWRITE,&old); VirtualProtect(memory+4*size,size,PAGE_READWRITE,&old);
        pm->~VxMatrix(); pv->~VxVector(); pr->~VxVector();
    }
    VirtualFree(memory,0,MEM_RELEASE);
#endif
    return result;
}
static MRResult MROnlyLinearRows(MRFn fn) {
    MRResult result;
#if defined(_WIN32)
    SYSTEM_INFO info; GetSystemInfo(&info); const unsigned size = info.dwPageSize;
    unsigned char *memory = static_cast<unsigned char *>(VirtualAlloc(nullptr,3*size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if (!memory) { ++result.errors; return result; }
    VxMatrix original; VxVector p; MRLiteralInput(original,p);
    VxMatrix *m = new(memory+2*size-48) VxMatrix(original); DWORD old;
    if (!VirtualProtect(memory+2*size,size,PAGE_NOACCESS,&old)) { VirtualFree(memory,0,MEM_RELEASE); ++result.errors; return result; }
    VxVector out; fn(&out,*m,&p); ++result.samples; result.errors += !MREqual(out,VxVector(-10,33,-4));
    VirtualProtect(memory+2*size,size,PAGE_READWRITE,&old); m->~VxMatrix(); VirtualFree(memory,0,MEM_RELEASE);
#else
    VxMatrix m; VxVector p,out; MRLiteralInput(m,p); fn(&out,m,&p); ++result.samples; result.errors += !MREqual(out,VxVector(-10,33,-4));
#endif
    return result;
}
static MRResult MRMemberRows() {
    MRResult result;
    for (int row = 0; row < 4; ++row) {
        VxMatrix m; VxVector p; MRLiteralInput(m,p); VxMatrix before = m;
        const VxVector expected = MRIntegerExpected(m,m[row]); m[row].Rotate(m); bool ok = MREqual(m[row],expected);
        for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) if (r != row || c == 3) ok = ok && std::memcmp(&m[r][c],&before[r][c],4) == 0;
        ++result.samples; result.errors += !ok;
    }
    return result;
}
static MRResult MRAll(MRFn fn) {
    MRResult result;
    for (MRResult r : {MRLiteral(fn),MRIntegers(fn),MRFields(fn),MRIgnored(fn),MRVector4Canary(fn),MRMatrixRows(fn),MRNumerics(fn,0),MRNumerics(fn,1),MRNumerics(fn,2),MRNonfinite(fn),MRFootprint(fn),MROnlyLinearRows(fn)}) {
        result.samples += r.samples; result.errors += r.errors;
    }
    return result;
}
#ifndef VX_MATRIX_ROTATE_PROBE
static void MRExpect(MRResult r, int count) { EXPECT_EQ(r.samples,count); EXPECT_EQ(r.errors,0); }
TEST(MatrixRotateReference, NonOrthogonalLinearLiteralWithoutTranslation) { MRExpect(MRLiteral(MRFree),2); }
TEST(MatrixRotateReference, IntegerLatticeDirectionsAndExactVectorAliases) { MRExpect(MRIntegers(MRFree),2048); }
TEST(MatrixRotateReference, AllFieldsDistinguishLinearTermsFromIgnoredState) { MRExpect(MRFields(MRFree),32); }
TEST(MatrixRotateReference, TranslationAndFourthColumnDoNotEnterXYZ) { MRExpect(MRIgnored(MRFree),112); }
TEST(MatrixRotateReference, Vector4BaseViewsDoNotReadOrWriteW) { MRExpect(MRVector4Canary(MRFree),16); }
TEST(MatrixRotateReference, MatrixRowBaseViewsPreserveOtherBytes) { MRExpect(MRMatrixRows(MRFree),20); }
TEST(MatrixRotateReference, CancellationRetainsSmallPointCoordinates) { MRExpect(MRNumerics(MRFree,0),18); }
TEST(MatrixRotateReference, FinitePointSurvivesIntermediateProductOverflow) { MRExpect(MRNumerics(MRFree,1),18); }
TEST(MatrixRotateReference, ThreeProductTailsCombineIntoSubnormalCoordinate) { MRExpect(MRNumerics(MRFree,2),18); }
TEST(MatrixRotateReference, NonfiniteArithmeticRetainsClassification) { MRExpect(MRNonfinite(MRFree),18); }
TEST(MatrixRotateReference, Matrix64AndVector12ReadWriteFootprints) {
#if defined(_WIN32)
    MRExpect(MRFootprint(MRFree),12);
#else
    MRExpect(MRFootprint(MRFree),4);
#endif
}
TEST(MatrixRotateReference, TranslationRowCanBeInaccessible) { MRExpect(MROnlyLinearRows(MRFree),1); }
TEST(MatrixRotateReference, MemberEntryUsesLinearContract) {
#if defined(_WIN32)
    MRExpect(MRAll(MRMember),2315);
#else
    MRExpect(MRAll(MRMember),2307);
#endif
    MRExpect(MRMemberRows(),4);
}
TEST(MatrixRotateReference, ValueEntryUsesLinearContract) {
#if defined(_WIN32)
    MRExpect(MRAll(MRValue),2315);
#else
    MRExpect(MRAll(MRValue),2307);
#endif
}
#if defined(VX_SIMD_SSE)
TEST(MatrixRotateReference, PackedIntegerPointImagesAndAliases) { MRExpect(MRIntegers(MRPacked),2048); }
TEST(MatrixRotateReference, PackedIgnoredMatrixColumnAndVector4Canaries) { MRExpect(MRIgnored(MRPacked),112); MRExpect(MRVector4Canary(MRPacked),16); }
TEST(MatrixRotateReference, PackedMatrixRowBaseViews) { MRExpect(MRMatrixRows(MRPacked),20); }
TEST(MatrixRotateReference, PackedCancellationOverflowAndSubnormal) { for (int k = 0; k < 3; ++k) MRExpect(MRNumerics(MRPacked,k),18); }
TEST(MatrixRotateReference, PackedNonfiniteClassification) { MRExpect(MRNonfinite(MRPacked),18); }
TEST(MatrixRotateReference, PackedVector12Footprints) {
    MRExpect(MROnlyLinearRows(MRPacked),1);
#if defined(_WIN32)
    MRExpect(MRFootprint(MRPacked),12);
#else
    MRExpect(MRFootprint(MRPacked),4);
#endif
}
#endif
#endif
