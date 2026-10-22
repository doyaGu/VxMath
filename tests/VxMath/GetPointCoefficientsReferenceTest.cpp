#include <cmath>
#include <cfloat>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <new>
#include <initializer_list>
#ifndef VX_COEFFICIENTS_NATIVE_PROBE
#ifndef VX_LIB
#define VX_LIB 1
#endif
#ifndef VX_COEFFICIENTS_IMPLEMENTATION
#define VX_COEFFICIENTS_IMPLEMENTATION "../../src/VxIntersect_FaceFace.cpp"
#endif
#include VX_COEFFICIENTS_IMPLEMENTATION
#include <gtest/gtest.h>
void VxIntersectPlaneDispatchRebuild(bool) { FAIL() << "Unrelated plane rebuild"; }
void VxIntersectSphereDispatchRebuild(bool) { FAIL() << "Unrelated sphere rebuild"; }
void VxIntersectFrustumDispatchRebuild(bool) { FAIL() << "Unrelated frustum rebuild"; }
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef void (*CFFn)(const VxVector &, const VxVector &, const VxVector &, const VxVector &, const int &, const int &, float &, float &, float &);
struct CFSamples { int samples = 0, errors = 0, weightErrors = 0, inputErrors = 0; unsigned digest = 2166136261u; };
static const int CFAxes[][2] = {{0,1},{1,0},{0,2},{2,0},{1,2},{2,1}};
static const int CFPermutations[][3] = {{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
static VxVector CFProject(float x, float y, int first, int second, float other = 0) {
    VxVector p(other,other,other); p[first] = x; p[second] = y; return p;
}
static void CFObserve(CFSamples &r, CFFn fn, const VxVector &p, const VxVector &a, const VxVector &b, const VxVector &c,
                      int first, int second, float w0, float w1, float w2, int alias = 0, bool singular = false) {
    const VxVector before[] = {p,a,b,c}; const int oldFirst = first, oldSecond = second;
    // Alias partitions: distinct, 0=1, 0=2, 1=2, all. The final value of a
    // shared slot is determined by the original V2,V1,V0 output order.
    const int slots[][3] = {{0,1,2},{0,0,2},{0,1,0},{0,1,1},{0,0,0}};
    float values[] = {71,83,97}, expected[] = {71,83,97};
    expected[slots[alias][2]] = w2; expected[slots[alias][1]] = w1; expected[slots[alias][0]] = w0;
    fn(p,a,b,c,first,second,values[slots[alias][0]],values[slots[alias][1]],values[slots[alias][2]]);
    bool bad = false;
    if (singular) bad = std::isfinite(values[0]) && std::isfinite(values[1]) && std::isfinite(values[2]);
    else for (int i = 0; i < 3; ++i) bad = bad || values[i] != expected[i];
    const bool changed = first != oldFirst || second != oldSecond || std::memcmp(&p,&before[0],12) || std::memcmp(&a,&before[1],12) ||
        std::memcmp(&b,&before[2],12) || std::memcmp(&c,&before[3],12);
    ++r.samples; r.errors += bad || changed; r.weightErrors += bad; r.inputErrors += changed;
    for (float v : values) { unsigned bits; std::memcpy(&bits,&v,4); r.digest = (r.digest ^ bits) * 16777619u; }
}
// Known affine combinations of integer/dyadic coordinates, including
// extrapolation. Expected weights do not call a production solver.
static const float CFTriangles[][6] = {{0,0,4,0,0,4},{-2,1,2,1,-2,5},{0,0,0,4,4,0},{0,0,4,4,8,-4}};
static CFSamples CFGrid(CFFn fn) {
    CFSamples r;
    for (const auto &t : CFTriangles) for (const auto &axes : CFAxes) for (const auto &perm : CFPermutations)
        for (int u = -2; u <= 4; ++u) for (int v = -2; v <= 4; ++v) for (float other : {-11.0f,0.0f,17.0f}) {
            const float w[] = {1 - (u+v)*.25f,u*.25f,v*.25f};
            const VxVector vertices[] = {CFProject(t[0],t[1],axes[0],axes[1]),CFProject(t[2],t[3],axes[0],axes[1]),CFProject(t[4],t[5],axes[0],axes[1])};
            const VxVector p = CFProject(t[0]*w[0]+t[2]*w[1]+t[4]*w[2],t[1]*w[0]+t[3]*w[1]+t[5]*w[2],axes[0],axes[1],other);
            CFObserve(r,fn,p,vertices[perm[0]],vertices[perm[1]],vertices[perm[2]],axes[0],axes[1],w[perm[0]],w[perm[1]],w[perm[2]]);
        }
    return r;
}
static CFSamples CFScales(CFFn fn) {
    CFSamples r; const float weights[][2] = {{0,0},{1,0},{0,1},{.25f,.25f},{-.25f,.5f},{1,1},{.5f,.25f}};
    for (int e : {-120,-90,-40,-10,0,10,40,90,120}) for (const auto &axes : CFAxes) for (int winding = 0; winding < 2; ++winding)
        for (const auto &t : CFTriangles) for (const auto &uv : weights) {
            const float scale = std::ldexp(1.0f,e), w[] = {1-uv[0]-uv[1],uv[0],uv[1]};
            const VxVector a = CFProject(t[0]*scale,t[1]*scale,axes[0],axes[1]),b = CFProject(t[2]*scale,t[3]*scale,axes[0],axes[1]),c = CFProject(t[4]*scale,t[5]*scale,axes[0],axes[1]);
            const VxVector p = CFProject((t[0]*w[0]+t[2]*w[1]+t[4]*w[2])*scale,(t[1]*w[0]+t[3]*w[1]+t[5]*w[2])*scale,axes[0],axes[1],9);
            CFObserve(r,fn,p,a,winding?c:b,winding?b:c,axes[0],axes[1],w[0],w[winding?2:1],w[winding?1:2]);
        }
    return r;
}
static CFSamples CFPivot(CFFn fn) {
    CFSamples r; const float points[][2] = {{.25f,.25f},{.5f,.25f},{-.25f,.5f}};
    for (int e : {-30,-54,-90,-120,-149}) for (const auto &axes : CFAxes) for (int winding = 0; winding < 2; ++winding) for (const auto &q : points) {
        const VxVector a = CFProject(0,0,axes[0],axes[1]),b = CFProject(std::ldexp(1.0f,e),1,axes[0],axes[1]),c = CFProject(1,0,axes[0],axes[1]);
        const VxVector p = CFProject(q[0],q[1],axes[0],axes[1],23);
        CFObserve(r,fn,p,a,winding?c:b,winding?b:c,axes[0],axes[1],1-q[0]-q[1],q[winding?0:1],q[winding?1:0]);
    }
    return r;
}
static CFSamples CFWide(CFFn fn) {
    CFSamples r; const float xy[][2] = {{-FLT_MAX,-FLT_MAX},{FLT_MAX,-FLT_MAX},{-FLT_MAX,FLT_MAX},{0,0},{-FLT_MAX*.5f,-FLT_MAX*.5f}};
    const float weights[][3] = {{1,0,0},{0,1,0},{0,0,1},{0,.5f,.5f},{.5f,.25f,.25f}};
    for (const auto &axes : CFAxes) for (int winding = 0; winding < 2; ++winding) for (int i = 0; i < 5; ++i) {
        const VxVector a = CFProject(xy[0][0],xy[0][1],axes[0],axes[1]),b = CFProject(xy[1][0],xy[1][1],axes[0],axes[1]),c = CFProject(xy[2][0],xy[2][1],axes[0],axes[1]);
        CFObserve(r,fn,CFProject(xy[i][0],xy[i][1],axes[0],axes[1]),a,winding?c:b,winding?b:c,axes[0],axes[1],weights[i][0],weights[i][winding?2:1],weights[i][winding?1:2]);
    }
    return r;
}
static CFSamples CFCancellation(CFFn fn) {
    CFSamples r;
    for (int e : {25,30}) for (const auto &axes : CFAxes) for (int winding = 0; winding < 2; ++winding) {
        const float m = std::ldexp(1.0f,e); const VxVector a = CFProject(m,m,axes[0],axes[1]),b = CFProject(0,1,axes[0],axes[1]),c = CFProject(1,0,axes[0],axes[1]);
        CFObserve(r,fn,CFProject(1,1,axes[0],axes[1]),a,winding?c:b,winding?b:c,axes[0],axes[1],std::ldexp(1.0f,-e-1),.5f,.5f);
    }
    return r;
}
static CFSamples CFAliases(CFFn fn) {
    CFSamples r;
    for (const auto &axes : CFAxes) for (int winding = 0; winding < 2; ++winding) for (int alias = 0; alias < 5; ++alias) {
        const VxVector a = CFProject(0,0,axes[0],axes[1]),b = CFProject(4,0,axes[0],axes[1]),c = CFProject(0,4,axes[0],axes[1]);
        CFObserve(r,fn,CFProject(.5f,1,axes[0],axes[1]),a,winding?c:b,winding?b:c,axes[0],axes[1],.625f,winding?.25f:.125f,winding?.125f:.25f,alias);
    }
    return r;
}
static CFSamples CFSingular(CFFn fn) {
    CFSamples r; const float cases[][6] = {{0,0,1,1,2,2},{0,0,0,1,0,2},{1,1,1,1,1,1},{0,0,1,0,1,0}};
    for (const auto &axes : CFAxes) for (const auto &t : cases) for (const auto &q : {VxVector(.5f,.5f,0),VxVector(1,0,0)})
        CFObserve(r,fn,CFProject(q.x,q.y,axes[0],axes[1]),CFProject(t[0],t[1],axes[0],axes[1]),CFProject(t[2],t[3],axes[0],axes[1]),CFProject(t[4],t[5],axes[0],axes[1]),axes[0],axes[1],0,0,0,0,true);
    return r;
}
#if defined(_WIN32)
static CFSamples CFFootprint(CFFn fn) {
    CFSamples r; SYSTEM_INFO info; GetSystemInfo(&info); const size_t page = info.dwPageSize;
    unsigned char *pages = static_cast<unsigned char *>(VirtualAlloc(nullptr,page*18,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if (!pages) { r.errors=1; return r; }
    const VxVector data[] = {VxVector(.5f,1,9),VxVector(0,0,0),VxVector(4,0,0),VxVector(0,4,0)};
    VxVector *v[4]; for(int i=0;i<4;++i)v[i]=new(pages+page*(1+2*i)-12)VxVector(data[i]);
    int *first=new(pages+page*9-4)int(0),*second=new(pages+page*11-4)int(1);
    float *w0=new(pages+page*13-4)float(71),*w1=new(pages+page*15-4)float(83),*w2=new(pages+page*17-4)float(97);
    DWORD old;bool good=true;for(int i=0;i<9;++i)good=VirtualProtect(pages+page*(2*i+1),page,PAGE_NOACCESS,&old)&&good;
    for(int i=0;i<6;++i)good=VirtualProtect(pages+page*2*i,page,PAGE_READONLY,&old)&&good;
    if(good){fn(*v[0],*v[1],*v[2],*v[3],*first,*second,*w0,*w1,*w2);r.samples=1;r.errors=*w0!=.625f||*w1!=.125f||*w2!=.25f;}
    else r.errors=1;VirtualFree(pages,0,MEM_RELEASE);return r;
}
#endif
#ifndef VX_COEFFICIENTS_NATIVE_PROBE
static void CFCheck(CFSamples r) {
    EXPECT_EQ(r.errors,0) << "weights=" << r.weightErrors << " inputs=" << r.inputErrors;
    ::testing::Test::RecordProperty("samples",r.samples);::testing::Test::RecordProperty("digest",std::to_string(r.digest));
}
#define CF_TESTS(prefix,fn) \
TEST(GetPointCoefficientsReference,prefix##AffineGridAxesAndVertexPermutations){CFCheck(CFGrid(fn));} \
TEST(GetPointCoefficientsReference,prefix##ScaleInvariantWeights){CFCheck(CFScales(fn));} \
TEST(GetPointCoefficientsReference,prefix##TinyPivotDoesNotLoseWeight){CFCheck(CFPivot(fn));} \
TEST(GetPointCoefficientsReference,prefix##FiniteExtremeCoordinates){CFCheck(CFWide(fn));} \
TEST(GetPointCoefficientsReference,prefix##SmallFirstWeightSurvivesCancellation){CFCheck(CFCancellation(fn));} \
TEST(GetPointCoefficientsReference,prefix##AliasedOutputsKeepIndependentWeights){CFCheck(CFAliases(fn));} \
TEST(GetPointCoefficientsReference,prefix##SingularProjectionHasNoFiniteSolution){CFCheck(CFSingular(fn));}
CF_TESTS(Scalar,GetPointCoefficientsScalarCore)
#if defined(VX_SIMD_SSE)
CF_TESTS(Simd,GetPointCoefficientsSIMDCore)
#endif
#if defined(_WIN32)
TEST(GetPointCoefficientsReference,ScalarReadonlyInputsAndGuardedOutputs){CFCheck(CFFootprint(GetPointCoefficientsScalarCore));}
#if defined(VX_SIMD_SSE)
TEST(GetPointCoefficientsReference,SimdReadonlyInputsAndGuardedOutputs){CFCheck(CFFootprint(GetPointCoefficientsSIMDCore));}
#endif
#endif
TEST(GetPointCoefficientsReference,PublicForwarderUsesFaceDispatch) {
    const auto *saved=GetVxIntersectFaceDispatchTable();
    for(int i=0;i<4;++i){const auto *table=&kVxIntersectFaceDispatchScalar;
#if defined(VX_SIMD_SSE)
        if(i&1)table=&kVxIntersectFaceDispatchSIMD;
#endif
        g_VxIntersectFaceDispatch.store(table,std::memory_order_release);
        CFCheck(CFPivot(VxIntersect::GetPointCoefficients));EXPECT_EQ(CFWide(VxIntersect::GetPointCoefficients).errors,0);EXPECT_EQ(CFAliases(VxIntersect::GetPointCoefficients).errors,0);
    }
    g_VxIntersectFaceDispatch.store(saved,std::memory_order_release);
}
#if defined(_WIN32)
TEST(GetPointCoefficientsReference,ActualCurrentDllExport) {
    HMODULE dll=LoadLibraryA("VxMath.dll");ASSERT_NE(dll,nullptr);
#if defined(_WIN64)
    const char *name="?GetPointCoefficients@VxIntersect@@SAXAEBUVxVector@@000AEBH1AEAM22@Z";
#else
    const char *name="?GetPointCoefficients@VxIntersect@@SAXABUVxVector@@000ABH1AAM22@Z";
#endif
    CFFn fn=reinterpret_cast<CFFn>(GetProcAddress(dll,name));ASSERT_NE(fn,nullptr);
    CFCheck(CFGrid(fn));EXPECT_EQ(CFScales(fn).errors,0);EXPECT_EQ(CFPivot(fn).errors,0);EXPECT_EQ(CFWide(fn).errors,0);EXPECT_EQ(CFCancellation(fn).errors,0);EXPECT_EQ(CFAliases(fn).errors,0);EXPECT_EQ(CFSingular(fn).errors,0);EXPECT_EQ(CFFootprint(fn).errors,0);
    EXPECT_NE(FreeLibrary(dll),0);
}
#endif
#endif
