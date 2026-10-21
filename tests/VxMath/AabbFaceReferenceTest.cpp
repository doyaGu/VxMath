#include <cmath>
#include <cfloat>
#include <cstddef>
#include <new>
#include <initializer_list>
#ifndef VX_AABB_FACE_NATIVE_PROBE
#ifndef VX_LIB
#define VX_LIB 1
#endif
#ifndef VX_AABB_FACE_IMPLEMENTATION
#define VX_AABB_FACE_IMPLEMENTATION "../../src/VxIntersect_BoxBox.cpp"
#endif
#include VX_AABB_FACE_IMPLEMENTATION
#include <gtest/gtest.h>
void VxIntersectPlaneDispatchRebuild(bool) { FAIL() << "Unrelated plane rebuild"; }
void VxIntersectSphereDispatchRebuild(bool) { FAIL() << "Unrelated sphere rebuild"; }
void VxIntersectFrustumDispatchRebuild(bool) { FAIL() << "Unrelated frustum rebuild"; }
void VxIntersectFaceDispatchRebuild(bool) { FAIL() << "Unrelated face rebuild"; }
#endif
#include "AabbFaceClippingOracle.h"
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef int (*FFunction)(const VxBbox &, const VxVector &, const VxVector &, const VxVector &, const VxVector &);
struct FResults { int samples = 0, errors = 0; unsigned digest = 2166136261u; };
static void FObserve(FResults &r, int actual, bool expected) { ++r.samples; if (actual != int(expected)) ++r.errors; r.digest = (r.digest ^ unsigned(actual)) * 16777619u; }
static VxVector FNormal(const VxVector *p) {
    double u[3],v[3],n[3];
    for (int i=0;i<3;++i) { u[i]=double(p[1][i])-p[0][i]; v[i]=double(p[2][i])-p[0][i]; }
    for (int i=0;i<3;++i) n[i]=u[(i+1)%3]*v[(i+2)%3]-u[(i+2)%3]*v[(i+1)%3];
    const double length=std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
    return length == 0 ? VxVector(0,0,0) : VxVector(float(n[0]/length),float(n[1]/length),float(n[2]/length));
}
static const int FPermutations[][3]={{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
static void FPermute(FResults &r, FFunction fn, const VxBbox &box, const VxVector *points, bool expected) {
    for (const auto &permutation:FPermutations) {
        const VxVector p[]={points[permutation[0]],points[permutation[1]],points[permutation[2]]};
        FObserve(r,fn(box,p[0],p[1],p[2],FNormal(p)),expected);
    }
}
static FResults FGrid(FFunction fn) {
    FResults r;
    const VxVector shapes[][3]={
        {VxVector(0,0,0),VxVector(.75f,0,0),VxVector(0,.75f,0)},
        {VxVector(-4,-4,0),VxVector(4,-4,0),VxVector(0,4,0)},
        {VxVector(-3,-2,-2),VxVector(3,-2,2),VxVector(0,3,0)},
        {VxVector(0,-3,-2),VxVector(0,3,-2),VxVector(0,0,3)},
        {VxVector(-3,-.25f,-.5f),VxVector(3,.25f,.5f),VxVector(1,2,-1)},
        {VxVector(0,0,0),VxVector(0,0,0),VxVector(0,0,0)},
        {VxVector(-3,0,0),VxVector(3,0,0),VxVector(0,0,0)},
        {VxVector(4,0,0),VxVector(0,4,0),VxVector(0,0,4)}
    };
    const VxBbox boxes[]={VxBbox(VxVector(-1,-1,-1),VxVector(1,1,1)),VxBbox(VxVector(0,-1,-1),VxVector(0,1,1))};
    for (const VxBbox &box:boxes) for (const auto &shape:shapes) for (int x=-3;x<=3;++x) for (int y=-3;y<=3;++y) for (int z=-3;z<=3;++z) {
        const VxVector shift(x*.5f,y*.5f,z*.5f),p[]={shape[0]+shift,shape[1]+shift,shape[2]+shift};
        FPermute(r,fn,box,p,AabbFaceClippingOracle(box,p[0],p[1],p[2]));
    }
    return r;
}
static FResults FBoundary(FFunction fn) {
    FResults r; const VxBbox box(VxVector(-1,-1,-1),VxVector(1,1,1));
    for (int axis=0;axis<3;++axis) for (int side=-1;side<=1;side+=2) for (int neighbor=-1;neighbor<=1;++neighbor) {
        VxVector p[]={VxVector(0,0,0),VxVector(0,0,0),VxVector(0,0,0)};
        const float boundary=float(side),coordinate=neighbor==0?boundary:std::nextafter(boundary,neighbor<0?-FLT_MAX:FLT_MAX);
        for (int i=0;i<3;++i) p[i][axis]=coordinate;
        p[0][(axis+1)%3]=-4;p[0][(axis+2)%3]=-4;
        p[1][(axis+1)%3]=4;p[1][(axis+2)%3]=-4;
        p[2][(axis+2)%3]=4;
        FPermute(r,fn,box,p,neighbor==0 || (side<0?neighbor>0:neighbor<0));
    }
    for (int axis=0;axis<3;++axis) {
        VxBbox flat=box;flat.Min[axis]=flat.Max[axis]=0;
        VxVector p[]={VxVector(0,0,0),VxVector(0,0,0),VxVector(0,0,0)};
        p[0][(axis+1)%3]=-4;p[0][(axis+2)%3]=-4;p[1][(axis+1)%3]=4;p[1][(axis+2)%3]=-4;p[2][(axis+2)%3]=4;
        FPermute(r,fn,flat,p,true);
    }
    const VxVector point[]={VxVector(1,1,1),VxVector(1,1,1),VxVector(1,1,1)};
    FPermute(r,fn,box,point,true);
    const VxBbox zero(VxVector(0,0,0),VxVector(0,0,0));
    const VxVector covering[]={VxVector(-4,-4,0),VxVector(4,-4,0),VxVector(0,4,0)};
    FPermute(r,fn,zero,covering,true);
    return r;
}
static FResults FDiagonals(FFunction fn) {
    FResults r;const VxBbox box(VxVector(-1,-1,-1),VxVector(1,1,1));
    for (int signs=0;signs<8;++signs) {
        VxVector p[]={VxVector(6,-2,-2),VxVector(-2,6,-2),VxVector(-2,-2,6)};
        for (int i=0;i<3;++i) for (int k=0;k<3;++k) if (signs&(1<<k)) p[i][k]=-p[i][k];
        FPermute(r,fn,box,p,true);
    }
    return r;
}
static FResults FScales(FFunction fn) {
    FResults r;const int powers[]={-120,-90,-40,-10,0,10,40,90,120};
    const VxVector shapes[][3]={{VxVector(-4,-4,0),VxVector(4,-4,0),VxVector(0,4,0)},
        {VxVector(4,0,0),VxVector(0,4,0),VxVector(0,0,4)},
        {VxVector(-2,0,0),VxVector(2,0,0),VxVector(0,2,1)}};
    for (int power:powers) for (int normalPower:{-90,-40,0,40,90}) for (const auto &shape:shapes) {
        const float s=std::ldexp(1.0f,power);const VxBbox box(VxVector(-s,-s,-s),VxVector(s,s,s));
        const VxVector points[]={shape[0]*s,shape[1]*s,shape[2]*s};
        const bool expected=AabbFaceClippingOracle(box,points[0],points[1],points[2]);
        for (const auto &permutation:FPermutations) {
            const VxVector p[]={points[permutation[0]],points[permutation[1]],points[permutation[2]]};
            FObserve(r,fn(box,p[0],p[1],p[2],FNormal(p)*std::ldexp(1.0f,normalPower)),expected);
        }
    }
    return r;
}
#if defined(_WIN32)
static FResults FFootprint(FFunction fn) {
    FResults r;SYSTEM_INFO info;GetSystemInfo(&info);
    unsigned char *pages=static_cast<unsigned char *>(VirtualAlloc(nullptr,info.dwPageSize*10,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if (!pages) { FObserve(r,0,true);return r; }
    VxBbox *box=new(pages+info.dwPageSize-sizeof(VxBbox)) VxBbox(VxVector(-1,-1,-1),VxVector(1,1,1));
    VxVector *p[4];const VxVector values[]={VxVector(6,-2,-2),VxVector(-2,6,-2),VxVector(-2,-2,6),VxVector(1,1,1)};
    for (int i=0;i<4;++i) p[i]=new(pages+info.dwPageSize*(3+2*i)-sizeof(VxVector)) VxVector(values[i]);
    bool protectedPages=true;DWORD old;
    for (int i=0;i<5;++i) protectedPages=VirtualProtect(pages+info.dwPageSize*(2*i+1),info.dwPageSize,PAGE_NOACCESS,&old) &&
        VirtualProtect(pages+info.dwPageSize*2*i,info.dwPageSize,PAGE_READONLY,&old) && protectedPages;
    if (protectedPages) { FObserve(r,fn(*box,*p[0],*p[1],*p[2],*p[3]),true); FObserve(r,fn(*box,*p[2],*p[1],*p[0],*p[3]),true); }
    else FObserve(r,0,true);
    VirtualFree(pages,0,MEM_RELEASE);return r;
}
#endif
#ifndef VX_AABB_FACE_NATIVE_PROBE
typedef int (*FOutcodeFn)(const VxBbox &,const VxVector &,unsigned &);
static FResults FOutcodes(FOutcodeFn fn) {
    FResults r;const VxBbox box(VxVector(-1,-1,-1),VxVector(1,1,1));
    const unsigned below[]={2,8,16},above[]={1,4,32};
    for (int x=-2;x<=2;++x) for (int y=-2;y<=2;++y) for (int z=-2;z<=2;++z) {
        const int position[]={x,y,z};unsigned expected=0,code=0xdeadbeef;
        for (int i=0;i<3;++i) { if (position[i]==-2) expected|=below[i]; if (position[i]==2) expected|=above[i]; }
        const int actual=fn(box,VxVector(float(x),float(y),float(z)),code);
        FObserve(r,actual,expected==0);if (code!=expected) ++r.errors;r.digest=(r.digest^code)*16777619u;
    }
    return r;
}
static void FCheck(FResults r) { EXPECT_EQ(r.errors,0);::testing::Test::RecordProperty("samples",r.samples);::testing::Test::RecordProperty("digest",std::to_string(r.digest)); }
TEST(AabbFaceReference, ScalarClippingGridAndPermutations) { FCheck(FGrid(AABBFaceCore<false>)); }
TEST(AabbFaceReference, ScalarClosedBoundaryAndFlatBoxes) { FCheck(FBoundary(AABBFaceCore<false>)); }
TEST(AabbFaceReference, ScalarCapRequiresMaximumNormalDiagonal) { FCheck(FDiagonals(AABBFaceCore<false>)); }
TEST(AabbFaceReference, ScalarGeometryAndNormalScaleInvariance) { FCheck(FScales(AABBFaceCore<false>)); }
TEST(AabbFaceReference, ScalarClosedOutcodeBits) { FCheck(FOutcodes(PointInAABB_OutCode)); }
#if defined(VX_SIMD_SSE)
TEST(AabbFaceReference, SimdClippingGridAndPermutations) { FCheck(FGrid(AABBFaceCore<true>)); }
TEST(AabbFaceReference, SimdClosedBoundaryAndFlatBoxes) { FCheck(FBoundary(AABBFaceCore<true>)); }
TEST(AabbFaceReference, SimdCapRequiresMaximumNormalDiagonal) { FCheck(FDiagonals(AABBFaceCore<true>)); }
TEST(AabbFaceReference, SimdGeometryAndNormalScaleInvariance) { FCheck(FScales(AABBFaceCore<true>)); }
TEST(AabbFaceReference, SimdClosedOutcodeBits) { FCheck(FOutcodes(PointInAABB_OutCode_SIMD)); }
#endif
TEST(AabbFaceReference, PublicForwarderUsesSelectedBoxTable) {
    const VxIntersectBoxDispatchTable *saved=GetVxIntersectBoxDispatchTable();
    for (int n=0;n<4;++n) {
        const VxIntersectBoxDispatchTable *table=&kVxIntersectBoxDispatchScalar;
#if defined(VX_SIMD_SSE)
        if (n&1) table=&kVxIntersectBoxDispatchSIMD;
#endif
        g_VxIntersectBoxDispatch.store(table,std::memory_order_release);
        FCheck(FDiagonals(VxIntersect::AABBFace));EXPECT_EQ(FBoundary(VxIntersect::AABBFace).errors,0);
    }
    g_VxIntersectBoxDispatch.store(saved,std::memory_order_release);
}
#if defined(_WIN32)
TEST(AabbFaceReference, ScalarReadonlyExactFootprints) { FCheck(FFootprint(AABBFaceCore<false>)); }
#if defined(VX_SIMD_SSE)
TEST(AabbFaceReference, SimdReadonlyExactFootprints) { FCheck(FFootprint(AABBFaceCore<true>)); }
#endif
TEST(AabbFaceReference, ActualCurrentDllExport) {
    HMODULE dll=LoadLibraryA("VxMath.dll");ASSERT_NE(dll,nullptr);
#if defined(_WIN64)
    const char *symbol="?AABBFace@VxIntersect@@SAHAEBUVxBbox@@AEBUVxVector@@111@Z";
#else
    const char *symbol="?AABBFace@VxIntersect@@SAHABUVxBbox@@ABUVxVector@@111@Z";
#endif
    FFunction fn=reinterpret_cast<FFunction>(GetProcAddress(dll,symbol));ASSERT_NE(fn,nullptr);
    FCheck(FGrid(fn));EXPECT_EQ(FBoundary(fn).errors,0);EXPECT_EQ(FDiagonals(fn).errors,0);EXPECT_EQ(FScales(fn).errors,0);EXPECT_EQ(FFootprint(fn).errors,0);
    EXPECT_NE(FreeLibrary(dll),0);
}
#endif
#endif
