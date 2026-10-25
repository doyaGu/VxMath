#include <float.h>
#include <math.h>
#include <string.h>
#include <new>
#ifndef VX_PROJECT_Z_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef decltype(&VxProjectBoxZExtents) PZFn;
struct PZResult { int calls = 0, errors = 0, faults = 0; };
static unsigned PZBits(float value) {
    unsigned bits;
    memcpy(&bits, &value, 4);
    return bits;
}
static float PZFloat(unsigned bits) {
    float value;
    memcpy(&value, &bits, 4);
    return value;
}
static void PZMatrix(VxMatrix &m) {
    memset(&m, 0, sizeof(m));
    m[0][0] = m[1][1] = m[2][2] = m[3][3] = 1;
}
// Independent corner table and direct homogeneous dot products. No runtime helper calls.
static int PZModel(const VxMatrix &m, const VxBbox &b, float values[8]) {
    static const unsigned corners[][8] = {{0,4,2,6,1,5,3,7}, {0,1,2,3}, {0,4,2,6}, {0,4,1,5}};
    const float widths[3] = {b.Max.x-b.Min.x, b.Max.y-b.Min.y, b.Max.z-b.Min.z};
    const int shape = widths[2] <= FLT_EPSILON ? 1 : widths[0] <= FLT_EPSILON ? 2 : widths[1] <= FLT_EPSILON ? 3 : 0;
    const int count = shape ? 4 : 8;
    for (int i = 0; i < count; ++i) {
        const unsigned mask = corners[shape][i];
        const long double p[4] = {mask&1 ? b.Max.x : b.Min.x, mask&2 ? b.Max.y : b.Min.y, mask&4 ? b.Max.z : b.Min.z, 1};
        long double z = 0, w = 0;
        for (int row = 0; row < 4; ++row) { z += p[row]*m[row][2]; w += p[row]*m[row][3]; }
        values[i] = !(w < 0) ? static_cast<float>(z/w) : 0;
    }
    return count;
}
static void PZReduce(const float values[8], int count, float &lo, float &hi) {
    lo = 1.0e10f;
    hi = -1.0e10f;
    for (int i = 0; i < count; ++i) {
        if (values[i] < lo) lo = values[i];
        if (values[i] > hi) hi = values[i];
    }
}
static bool PZCall(PZFn fn, const VxMatrix &m, const VxBbox &b, float &lo, float &hi) {
#if defined(_WIN32)
    __try { fn(m,b,lo,hi); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
#else
    fn(m,b,lo,hi);
    return true;
#endif
}
static void PZCheck(PZResult &r, PZFn fn, const VxMatrix &m, const VxBbox &b, float &lo, float &hi) {
    float values[8], expectedLo = 0, expectedHi = 0;
    const int count = PZModel(m,b,values);
    PZReduce(values,count,expectedLo,expectedHi);
    ++r.calls;
    const bool ok = PZCall(fn,m,b,lo,hi);
    r.faults += !ok;
    r.errors += !ok || PZBits(lo)!=PZBits(expectedLo) || PZBits(hi)!=PZBits(expectedHi);
}
static PZResult PZOrdinary(PZFn fn) {
    PZResult r;
    unsigned random = 319;
    for (int i = 0; i < 768; ++i) {
        VxMatrix m;
        for (int row = 0; row < 4; ++row) for (int col = 0; col < 4; ++col) {
            random = random*1664525u+1013904223u;
            m[row][col] = static_cast<float>(int((random>>16)%9)-4);
        }
        VxBbox b(VxVector(-3,-2,-1),VxVector(4,3,5));
        if (i%4==1) b.Max.z=b.Min.z;
        if (i%4==2) b.Max.x=b.Min.x;
        if (i%4==3) b.Max.y=b.Min.y;
        float lo = 123, hi = -45;
        PZCheck(r,fn,m,b,lo,hi);
    }
    return r;
}
static PZResult PZThin(PZFn fn) {
    PZResult r;
    const float widths[] = {0,FLT_EPSILON*0.5f,FLT_EPSILON,PZFloat(PZBits(FLT_EPSILON)+1)};
    for (int axis = 0; axis < 3; ++axis) for (float width : widths) for (int varyingW = 0; varyingW < 2; ++varyingW) {
        VxMatrix m;
        PZMatrix(m);
        m[0][2]=2; m[1][2]=4; m[2][2]=8;
        m[axis][2]=8388608.0f;
        m[axis][3]=varyingW ? 4194304.0f : 0;
        VxBbox b(VxVector(0,0,0),VxVector(1,1,1));
        (&b.Max.x)[axis]=width;
        float lo=0,hi=0;
        PZCheck(r,fn,m,b,lo,hi);
    }
    return r;
}
static PZResult PZLiteral(PZFn fn) {
    PZResult r;
    for (int kind = 0; kind < 18; ++kind) {
        VxMatrix m;
        PZMatrix(m);
        VxBbox b(VxVector(-1,-1,2),VxVector(1,1,6));
        float expectedLo=2, expectedHi=6;
        if (kind==1) { m[3][3]=-1; expectedLo=expectedHi=0; }
        if (kind>=2 && kind<=4) {
            m[2][2]=0; m[3][2]=kind==2 ? 4.0f : kind==3 ? -4.0f : 0;
            m[3][3]=0;
            expectedLo=kind==3 ? -HUGE_VALF : 1.0e10f;
            expectedHi=kind==2 ? HUGE_VALF : -1.0e10f;
        }
        if (kind==5) { m[2][2]=0; m[3][2]=2; m[0][3]=1; m[3][3]=0; expectedLo=0; expectedHi=2; }
        if (kind==6) { m[2][2]=0; m[3][2]=2.0e10f; expectedLo=1.0e10f; expectedHi=2.0e10f; }
        if (kind==7) { m[2][2]=0; m[3][2]=-2.0e10f; expectedLo=-2.0e10f; expectedHi=-1.0e10f; }
        if (kind==8 || kind==9) {
            b=VxBbox(VxVector(1,1,1),VxVector(1,1,1));
            const float zero=kind==8 ? -0.0f : 0.0f;
            for (int row=0;row<4;++row) m[row][2]=zero;
            expectedLo=expectedHi=zero;
        }
        if (kind==10) {
            b=VxBbox(VxVector(1,1,1),VxVector(1,1,1));
            m[2][2]=0; m[3][2]=4;
            for (int row=0;row<4;++row) m[row][3]=-0.0f;
            expectedLo=-HUGE_VALF; expectedHi=-1.0e10f;
        }
        if (kind>=11 && kind<=16) {
            b=VxBbox(VxVector(1,1,1),VxVector(1,1,1));
            m[2][2]=0; m[3][2]=4;
            if (kind==11) { m[3][3]=PZFloat(0x7fc12345u); expectedLo=1.0e10f; expectedHi=-1.0e10f; }
            if (kind==12) { m[3][2]=PZFloat(0x7fc12345u); m[3][3]=-1; expectedLo=expectedHi=0; }
            if (kind==13 || kind==14) { m[3][3]=kind==13 ? HUGE_VALF : -HUGE_VALF; expectedLo=expectedHi=0; }
            if (kind==15) { m[3][2]=HUGE_VALF; expectedLo=1.0e10f; expectedHi=HUGE_VALF; }
            if (kind==16) { m[3][2]=PZFloat(0x7fc12345u); expectedLo=1.0e10f; expectedHi=-1.0e10f; }
        }
        if (kind==17) {
            for (int row=0;row<4;++row) m[row][0]=m[row][1]=PZFloat(0x7fc12345u);
        }
        float lo=123,hi=456;
        ++r.calls;
        const bool ok=PZCall(fn,m,b,lo,hi);
        r.faults+=!ok;
        r.errors+=!ok || PZBits(lo)!=PZBits(expectedLo) || PZBits(hi)!=PZBits(expectedHi);
    }
    return r;
}
static void PZAliasInput(VxMatrix &m, VxBbox &b, int shape) {
    PZMatrix(m);
    m[0][2]=2; m[1][2]=3; m[2][2]=4; m[3][2]=5;
    b=VxBbox(VxVector(0,0,0),VxVector(1,2,3));
    if (shape==1) b.Max.z=0;
    if (shape==2) b.Max.x=0;
    if (shape==3) b.Max.y=0;
}
static PZResult PZAliases(PZFn fn, int kind) {
    PZResult r;
    const int fields=kind==0 ? 16 : 6;
    for (int shape=0;shape<4;++shape) for (int a=0;a<fields;++a) for (int b=0;b<fields;++b) {
        VxMatrix m, expectedM;
        VxBbox box, expectedBox;
        PZAliasInput(m,box,shape);
        memcpy(&expectedM,&m,sizeof(m)); memcpy(&expectedBox,&box,sizeof(box));
        float values[8]; const int count=PZModel(m,box,values);
        float *actual=kind==0 ? &m[0][0] : &box.Max.x;
        float *expected=kind==0 ? &expectedM[0][0] : &expectedBox.Max.x;
        PZReduce(values,count,expected[a],expected[b]);
        ++r.calls;
        const bool ok=PZCall(fn,m,box,actual[a],actual[b]);
        r.faults+=!ok;
        r.errors+=!ok || memcmp(&m,&expectedM,sizeof(m))!=0 || memcmp(&box,&expectedBox,sizeof(box))!=0;
    }
    return r;
}
static PZResult PZShared(PZFn fn) {
    PZResult r;
    for (int shape=0;shape<4;++shape) for (int sign=0;sign<2;++sign) {
        VxMatrix m; VxBbox b; PZAliasInput(m,b,shape);
        if (sign) for (int row=0;row<4;++row) m[row][2]=-m[row][2];
        float values[8],expected=0;
        const int count=PZModel(m,b,values);
        PZReduce(values,count,expected,expected);
        float actual=777;
        ++r.calls; const bool ok=PZCall(fn,m,b,actual,actual); r.faults+=!ok;
        r.errors+=!ok || PZBits(actual)!=PZBits(expected);
    }
    return r;
}
static void PZWideCase(int kind, VxMatrix &m, VxBbox &b, float &lo, float &hi) {
    PZMatrix(m);
    b=VxBbox(VxVector(0,0,0),VxVector(0,0,0));
    m[2][2]=0;
    lo=hi=1;
    if (kind==0) { b.Min.x=-FLT_MAX; b.Max.x=FLT_MAX; b.Max.y=b.Max.z=1; m[0][2]=1; m[3][3]=FLT_MAX; lo=-1; }
    if (kind==1) { b.Max.x=FLT_MAX; b.Max.y=b.Max.z=1; m[0][2]=2; m[3][3]=FLT_MAX; lo=0; hi=2; }
    if (kind==2) { b.Min.x=b.Max.x=FLT_MAX; m[0][2]=m[0][3]=2; m[3][3]=0; }
    if (kind==3) { b.Min.x=b.Max.x=PZFloat(1); m[0][2]=m[0][3]=PZFloat(1); m[3][3]=0; }
    if (kind==4) { b.Min.x=b.Max.x=FLT_MAX; b.Min.y=b.Max.y=FLT_MAX; m[0][2]=2; m[1][2]=-2; m[3][2]=5; lo=hi=5; }
    if (kind==5) { b.Min.x=b.Max.x=PZFloat(1); m[0][2]=1; lo=hi=PZFloat(1); }
    if (kind==6) { b.Min.x=b.Max.x=1; m[0][2]=PZFloat(1); m[0][3]=PZFloat(1); m[3][3]=0; }
    if (kind>=7) {
        b=VxBbox(VxVector(1,1,1),VxVector(1,1,1));
        if (kind==7) { m[0][2]=1.0e30f; m[1][2]=1; m[2][2]=-1.0e30f; }
        else { m[3][2]=4; m[0][3]=1.0e30f; m[1][3]=kind==8 ? 1.0f : -1.0f; m[2][3]=-1.0e30f; m[3][3]=0; lo=hi=kind==8 ? 4.0f : 0; }
    }
}
static PZResult PZWide(PZFn fn) {
    PZResult r;
    for (int kind=0;kind<10;++kind) {
        VxMatrix m; VxBbox b; float expectedLo,expectedHi;
        PZWideCase(kind,m,b,expectedLo,expectedHi);
        float lo=123,hi=456;
        ++r.calls; const bool ok=PZCall(fn,m,b,lo,hi); r.faults+=!ok;
        r.errors+=!ok || PZBits(lo)!=PZBits(expectedLo) || PZBits(hi)!=PZBits(expectedHi);
    }
    return r;
}
static PZResult PZFootprint(PZFn fn) {
    PZResult r;
#if defined(_WIN32)
    SYSTEM_INFO info; GetSystemInfo(&info); const size_t page=info.dwPageSize;
    unsigned char *memory[4]={};
    for (int i=0;i<4;++i) {
        memory[i]=static_cast<unsigned char *>(VirtualAlloc(nullptr,page*3,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS));
        if (!memory[i]) { ++r.errors; for (int j=0;j<i;++j) VirtualFree(memory[j],0,MEM_RELEASE); return r; }
    }
    for (int shape=0;shape<4;++shape) for (unsigned edges=0;edges<16;++edges) {
        DWORD old; for (int i=0;i<4;++i) r.errors+=!VirtualProtect(memory[i]+page,page,PAGE_READWRITE,&old);
        VxMatrix m; VxBbox b; PZAliasInput(m,b,shape);
        unsigned char *at[4]; const size_t sizes[]={sizeof(m),sizeof(b),4,4};
        for (int i=0;i<4;++i) at[i]=memory[i]+page+((edges&(1u<<i)) ? page-sizes[i] : 0);
        VxMatrix *matrix=::new (at[0]) VxMatrix(m);
        VxBbox *box=::new (at[1]) VxBbox(b);
        float *lo=::new (at[2]) float(123), *hi=::new (at[3]) float(456);
        r.errors+=!VirtualProtect(memory[0]+page,page,PAGE_READONLY,&old);
        r.errors+=!VirtualProtect(memory[1]+page,page,PAGE_READONLY,&old);
        PZCheck(r,fn,*matrix,*box,*lo,*hi);
        r.errors+=memcmp(matrix,&m,sizeof(m))!=0 || memcmp(box,&b,sizeof(b))!=0;
    }
    for (int i=0;i<4;++i) VirtualFree(memory[i],0,MEM_RELEASE);
#endif
    return r;
}
#ifndef VX_PROJECT_Z_PROBE
#if defined(VX_SIMD_SSE)
static void PZRaw(const VxMatrix &m,const VxBbox &b,float &lo,float &hi) { VxSIMDProjectBoxZExtents(&m,&b,&lo,&hi); }
#endif
class ProjectBoxZExtentsReference : public testing::TestWithParam<int> {
protected:
    int previous=VX_SIMD_MODE_AUTO;
    void SetUp() override {
        previous=VxGetSIMDOverride(); const int wanted=GetParam()==0 ? VX_SIMD_MODE_NONE : VX_SIMD_MODE_SSE2;
        ASSERT_TRUE(VxSetSIMDOverride(wanted)); ASSERT_EQ(VxGetSIMDEffectiveBackend(),wanted);
    }
    void TearDown() override { EXPECT_TRUE(VxSetSIMDOverride(previous)); }
    PZFn Function() const {
#if defined(VX_SIMD_SSE)
        if (GetParam()==2) return PZRaw;
#endif
        return VxProjectBoxZExtents;
    }
    void Check(PZResult r,int calls) { EXPECT_EQ(r.calls,calls); EXPECT_EQ(r.errors+r.faults,0); }
};
TEST_P(ProjectBoxZExtentsReference, MatchIndependentCornerEnumeration) { Check(PZOrdinary(Function()),768); }
TEST_P(ProjectBoxZExtentsReference, KeepOriginalThinAxisThresholdAndPriority) { Check(PZThin(Function()),24); }
TEST_P(ProjectBoxZExtentsReference, PreserveLiteralWAndSentinelRules) { Check(PZLiteral(Function()),18); }
TEST_P(ProjectBoxZExtentsReference, SnapshotMatrixBeforeAliasedOutputs) { Check(PZAliases(Function(),0),1024); }
TEST_P(ProjectBoxZExtentsReference, SnapshotBoxBeforeAliasedOutputs) { Check(PZAliases(Function(),1),144); }
TEST_P(ProjectBoxZExtentsReference, PreserveSharedOutputWriteOrder) { Check(PZShared(Function()),8); }
TEST_P(ProjectBoxZExtentsReference, KeepFiniteHomogeneousRatiosAcrossFloatRange) { Check(PZWide(Function()),10); }
#if defined(_WIN32)
TEST_P(ProjectBoxZExtentsReference, RespectReadOnlyInputsAndExactOutputBounds) { Check(PZFootprint(Function()),64); }
#endif
TEST_P(ProjectBoxZExtentsReference, KeepInvalidBoxDefaults) {
    for (int axis=0;axis<3;++axis) {
        VxMatrix m; PZMatrix(m); VxBbox b(VxVector(0,0,0),VxVector(1,1,1)); (&b.Min.x)[axis]=2;
        float lo=123,hi=456; Function()(m,b,lo,hi); EXPECT_EQ(lo,1.0e10f); EXPECT_EQ(hi,-1.0e10f);
    }
}
#if defined(VX_SIMD_SSE)
INSTANTIATE_TEST_SUITE_P(ScalarSSE2AndRaw,ProjectBoxZExtentsReference,testing::Values(0,1,2));
#else
INSTANTIATE_TEST_SUITE_P(ScalarOnly,ProjectBoxZExtentsReference,testing::Values(0));
#endif
#endif
