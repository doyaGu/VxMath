#include <cmath>
#include <cfloat>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <new>
#include <initializer_list>
#ifndef VX_POINT_IN_FACE_NATIVE_PROBE
#ifndef VX_LIB
#define VX_LIB 1
#endif
#ifndef VX_POINT_IN_FACE_IMPLEMENTATION
#define VX_POINT_IN_FACE_IMPLEMENTATION "../../src/VxIntersect_FaceFace.cpp"
#endif
#include VX_POINT_IN_FACE_IMPLEMENTATION
#include <gtest/gtest.h>
void VxIntersectPlaneDispatchRebuild(bool) { FAIL()<<"Unrelated plane rebuild"; }
void VxIntersectSphereDispatchRebuild(bool) { FAIL()<<"Unrelated sphere rebuild"; }
void VxIntersectFrustumDispatchRebuild(bool) { FAIL()<<"Unrelated frustum rebuild"; }
#endif
#include "PointInFaceOracle.h"
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef int (*PFFn)(const VxVector &,const VxVector &,const VxVector &,const VxVector &,const VxVector &,int &,int &);
struct PFSamples {int samples=0,errors=0,rawErrors=0,truthErrors=0,axisErrors=0,preservationErrors=0;unsigned digest=2166136261u;};
static VxVector PFProject(float x,float y,int drop,float other=0) {
    VxVector p(0,0,0);int first=drop==0?1:0,second=drop==2?1:2;p[first]=x;p[second]=y;p[drop]=other;return p;
}
static void PFObserve(PFSamples &r,PFFn fn,const VxVector &p,const VxVector &a,const VxVector &b,const VxVector &c,const VxVector &n,bool alias=false) {
    const VxVector before[]={p,a,b,c,n};int first=71,second=83;
    const PointInFaceResult want=PointInFaceOracle(p,a,b,c,n);
    const int actual=fn(p,a,b,c,n,first,alias?first:second);
    const bool badAxes=alias?first!=want.second:first!=want.first||second!=want.second;
    const bool changed=std::memcmp(&p,&before[0],12)||std::memcmp(&a,&before[1],12)||std::memcmp(&b,&before[2],12)||std::memcmp(&c,&before[3],12)||std::memcmp(&n,&before[4],12);
    ++r.samples;r.rawErrors+=actual!=want.code;r.truthErrors+=(actual!=0)!=(want.code!=0);r.axisErrors+=badAxes;r.preservationErrors+=changed;
    r.errors+=actual!=want.code||badAxes||changed;r.digest=(r.digest^unsigned(actual))*16777619u;
    r.digest=(r.digest^unsigned(first))*16777619u;r.digest=(r.digest^unsigned(alias?first:second))*16777619u;
}
static const float PFTriangles[][6]={
    {0,0,4,0,0,4},{-2,-1,3,0,-1,3},{0,0,1,2,3,-1},
    {0,0,1,1,2,2},{1,-1,1,-1,1,-1},{0,0,4,0,4,0},{0,-2,0,0,0,3}
};
static PFSamples PFGrid(PFFn fn) {
    PFSamples r;
    for(const auto &t:PFTriangles)for(int winding=0;winding<2;++winding)for(int drop=0;drop<3;++drop)for(int sign:{-1,1}) {
        const VxVector a=PFProject(t[0],t[1],drop),b=PFProject(t[winding?4:2],t[winding?5:3],drop),c=PFProject(t[winding?2:4],t[winding?3:5],drop);
        VxVector n(0,0,0);n[drop]=float(sign);
        for(int x=-2;x<=6;++x)for(int y=-2;y<=6;++y)for(int other:{-7,0,11})PFObserve(r,fn,PFProject(float(x),float(y),drop,float(other)),a,b,c,n);
    }
    return r;
}
static const float PFNormals[][3]={{1,0,0},{0,1,0},{0,0,1},{1,1,1},{-1,1,0},{0,-1,1},{1,0,-1},{0,0,0},{0,2,1},{1,1,2},{-2,-1,-2},{0x1p-120f,0,0},{0,0x1p120f,0},{FLT_MAX,FLT_MAX,FLT_MAX}};
static PFSamples PFProjections(PFFn fn) {
    PFSamples r;
    for(const auto &n:PFNormals)for(int plane=0;plane<4;++plane) {
        const VxVector a=plane==3?VxVector(-1,0,1):PFProject(0,0,plane);
        const VxVector b=plane==3?VxVector(2,-1,0):PFProject(2,0,plane);
        const VxVector c=plane==3?VxVector(0,2,-1):PFProject(0,2,plane);
        for(int x=-2;x<=2;++x)for(int y=-2;y<=2;++y)for(int z=-2;z<=2;++z)PFObserve(r,fn,VxVector(float(x),float(y),float(z)),a,b,c,VxVector(n[0],n[1],n[2]));
    }
    return r;
}
static PFSamples PFBoundary(PFFn fn) {
    PFSamples r;const float tinyValue=std::nextafter(0.0f,1.0f),below=std::nextafter(.5f,0.0f),above=std::nextafter(.5f,1.0f);
    const float points[][2]={{0,0},{1,0},{0,1},{.5f,0},{0,.5f},{.5f,.5f},{tinyValue,tinyValue},{-tinyValue,tinyValue},{tinyValue,-tinyValue},{below,.5f},{above,.5f},{.5f,below},{.5f,above},{1,tinyValue},{std::nextafter(1.0f,2.0f),0}};
    for(int drop=0;drop<3;++drop)for(int winding=0;winding<2;++winding)for(const auto &p:points){
        const VxVector a=PFProject(0,0,drop),b=PFProject(winding?0.0f:1.0f,winding?1.0f:0.0f,drop),c=PFProject(winding?1.0f:0.0f,winding?0.0f:1.0f,drop);
        VxVector n(0,0,0);n[drop]=1;PFObserve(r,fn,PFProject(p[0],p[1],drop,9),a,b,c,n);
    }
    return r;
}
static PFSamples PFScales(PFFn fn) {
    PFSamples r;const float points[][2]={{.5f,.5f},{1,1},{2,2},{0,0},{4,0},{0,4},{5,0},{-1,1},{3,2}};
    for(int exponent:{-120,-90,-40,-10,0,10,40,90,120})for(int drop=0;drop<3;++drop)for(int winding=0;winding<2;++winding)for(const auto &p:points){
        const float size=std::ldexp(1.0f,exponent);const VxVector a=PFProject(0,0,drop),b=PFProject(winding?0.0f:4*size,winding?4*size:0.0f,drop),c=PFProject(winding?4*size:0.0f,winding?0.0f:4*size,drop);
        VxVector n(0,0,0);n[drop]=1;PFObserve(r,fn,PFProject(p[0]*size,p[1]*size,drop),a,b,c,n);
    }
    return r;
}
static PFSamples PFWide(PFFn fn) {
    PFSamples r;const float m=FLT_MAX;const float points[][2]={{0,0},{-.5f,-.5f},{.5f,.5f},{1,-1},{-1,1},{-1,-1},{1,1}};
    for(int drop=0;drop<3;++drop)for(int winding=0;winding<2;++winding)for(const auto &p:points){
        const VxVector a=PFProject(-m,-m,drop),b=PFProject(winding?-m:m,winding?m:-m,drop),c=PFProject(winding?m:-m,winding?-m:m,drop);
        VxVector n(0,0,0);n[drop]=1;PFObserve(r,fn,PFProject(p[0]*m,p[1]*m,drop),a,b,c,n);
    }
    return r;
}
static PFSamples PFSeparatedExponents(PFFn fn) {
    PFSamples r;
    for(int exponent:{-149,-120,-90,-54,-53,-52,-30})for(int drop=0;drop<3;++drop)for(int winding=0;winding<2;++winding){
        const float tinyValue=std::ldexp(1.0f,exponent);
        const float points[][2]={{1,tinyValue},{tinyValue,1},{1,-tinyValue},{-tinyValue,1},{0,tinyValue},{tinyValue,0},{tinyValue,tinyValue},{-tinyValue,-tinyValue}};
        const VxVector a=PFProject(0,0,drop),b=PFProject(winding?0.0f:1.0f,winding?1.0f:0.0f,drop),c=PFProject(winding?1.0f:0.0f,winding?0.0f:1.0f,drop);
        VxVector n(0,0,0);n[drop]=1;
        for(int i=0;i<8;++i){const VxVector p=PFProject(points[i][0],points[i][1],drop);const int literal=i>=4&&i<=6?(winding?2:1):0;
            if(PointInFaceOracle(p,a,b,c,n).code!=literal)++r.errors;
            PFObserve(r,fn,p,a,b,c,n);
        }
    }return r;
}
static PFSamples PFAliases(PFFn fn) {
    PFSamples r;for(int drop=0;drop<3;++drop)for(int winding=0;winding<2;++winding)for(float x:{.25f,2.0f}){
        VxVector n(0,0,0);n[drop]=1;PFObserve(r,fn,PFProject(x,.25f,drop),PFProject(0,0,drop),PFProject(winding?0.0f:1.0f,winding?1.0f:0.0f,drop),PFProject(winding?1.0f:0.0f,winding?0.0f:1.0f,drop),n,true);
    }return r;
}
static PFSamples PFLiterals(PFFn fn) {
    PFSamples r;const VxVector a(0,0,0),b(1,0,0),c(0,1,0),n(0,0,1);
    const VxVector points[]={VxVector(.25f,.25f,99),VxVector(.5f,.5f,0),VxVector(0,0,0),VxVector(2,2,0)};
    for(int winding=0;winding<2;++winding)for(int i=0;i<4;++i){
        int first=71,second=83;const int code=fn(points[i],a,winding?c:b,winding?b:c,n,first,second),literal=i==3?0:winding?2:1;
        ++r.samples;r.errors+=code!=literal||first!=0||second!=1;r.rawErrors+=code!=literal;r.axisErrors+=first!=0||second!=1;r.digest=(r.digest^unsigned(code))*16777619u;
    }
    const VxVector d(1,1,0);int first=71,second=83;
    for(const VxVector &p:{VxVector(1,1,100),VxVector(2,1,0)}){
        const int code=fn(p,d,d,d,n,first,second),literal=p.x==1?2:0;
        ++r.samples;r.errors+=code!=literal;r.rawErrors+=code!=literal;r.digest=(r.digest^unsigned(code))*16777619u;
    }return r;
}
#if defined(_WIN32)
static PFSamples PFFootprint(PFFn fn) {
    PFSamples r;SYSTEM_INFO info;GetSystemInfo(&info);const size_t page=info.dwPageSize;
    unsigned char *pages=static_cast<unsigned char *>(VirtualAlloc(nullptr,page*14,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));if(!pages){r.errors=1;return r;}
    VxVector *inputs[5];for(int i=0;i<5;++i)inputs[i]=new(pages+page*(1+2*i)-12)VxVector(0,0,0);
    inputs[0]->Set(.25f,.25f,0);inputs[2]->Set(1,0,0);inputs[3]->Set(0,1,0);inputs[4]->Set(0,0,1);
    int *first=new(pages+page*11-4)int(71),*second=new(pages+page*13-4)int(83);DWORD old;bool good=true;
    for(int i=0;i<7;++i)good=VirtualProtect(pages+page*(2*i+1),page,PAGE_NOACCESS,&old)&&good;
    for(int i=0;i<5;++i)good=VirtualProtect(pages+page*2*i,page,PAGE_READONLY,&old)&&good;
    if(good){const int actual=fn(*inputs[0],*inputs[1],*inputs[2],*inputs[3],*inputs[4],*first,*second);r.samples=1;r.errors=actual!=1||*first!=0||*second!=1;r.digest=(r.digest^unsigned(actual))*16777619u;}
    else r.errors=1;VirtualFree(pages,0,MEM_RELEASE);return r;
}
#endif
#ifndef VX_POINT_IN_FACE_NATIVE_PROBE
static void PFCheck(PFSamples r){EXPECT_EQ(r.errors,0)<<"raw="<<r.rawErrors<<" truth="<<r.truthErrors<<" axes="<<r.axisErrors<<" preservation="<<r.preservationErrors;::testing::Test::RecordProperty("samples",r.samples);::testing::Test::RecordProperty("digest",std::to_string(r.digest));}
#define PF_CASES(prefix,fn) \
TEST(PointInFaceReference,prefix##ProjectedLattice){PFCheck(PFGrid(fn));} \
TEST(PointInFaceReference,prefix##ProjectionAndNormalTies){PFCheck(PFProjections(fn));} \
TEST(PointInFaceReference,prefix##ClosedEdgesBothWindings){PFCheck(PFBoundary(fn));} \
TEST(PointInFaceReference,prefix##FiniteScaleGeometry){PFCheck(PFScales(fn));} \
TEST(PointInFaceReference,prefix##FiniteExtremeDifferences){PFCheck(PFWide(fn));} \
TEST(PointInFaceReference,prefix##ExactSignsAcrossSeparatedExponents){PFCheck(PFSeparatedExponents(fn));} \
TEST(PointInFaceReference,prefix##AliasedAxisOutputs){PFCheck(PFAliases(fn));} \
TEST(PointInFaceReference,prefix##LiteralGeometryAndReturnCodes){PFCheck(PFLiterals(fn));}
PF_CASES(Scalar,PointInFaceScalarCore)
#if defined(VX_SIMD_SSE)
PF_CASES(Simd,PointInFaceSIMDCore)
#endif
TEST(PointInFaceReference,ProjectionHelperContract){
    for(const auto &n:PFNormals){const VxVector normal(n[0],n[1],n[2]);const auto expected=PointInFaceOracle(normal,normal,normal,normal,normal);int first=71,second=83;SelectFaceProjectionAxes(normal,first,second);EXPECT_EQ(first,expected.first);EXPECT_EQ(second,expected.second);int alias=71;SelectFaceProjectionAxes(normal,alias,alias);EXPECT_EQ(alias,expected.second);}
}
TEST(PointInFaceReference,PublicForwardersUseFaceDispatch){const auto *saved=GetVxIntersectFaceDispatchTable();for(int i=0;i<4;++i){const auto *table=&kVxIntersectFaceDispatchScalar;
#if defined(VX_SIMD_SSE)
    if(i&1)table=&kVxIntersectFaceDispatchSIMD;
#endif
    g_VxIntersectFaceDispatch.store(table,std::memory_order_release);PFCheck(PFLiterals(VxIntersect::PointInFace));EXPECT_EQ(PFBoundary(VxIntersect::PointInFace).errors,0);}
    g_VxIntersectFaceDispatch.store(saved,std::memory_order_release);
}
#if defined(_WIN32)
TEST(PointInFaceReference,ScalarReadonlyInputsAndGuardedOutputs){PFCheck(PFFootprint(PointInFaceScalarCore));}
#if defined(VX_SIMD_SSE)
TEST(PointInFaceReference,SimdReadonlyInputsAndGuardedOutputs){PFCheck(PFFootprint(PointInFaceSIMDCore));}
#endif
TEST(PointInFaceReference,ActualCurrentDllExport){HMODULE dll=LoadLibraryA("VxMath.dll");ASSERT_NE(dll,nullptr);
#if defined(_WIN64)
    const char *name="?PointInFace@VxIntersect@@SAHAEBUVxVector@@0000AEAH1@Z";
#else
    const char *name="?PointInFace@VxIntersect@@SAHABUVxVector@@0000AAH1@Z";
#endif
    PFFn fn=reinterpret_cast<PFFn>(GetProcAddress(dll,name));ASSERT_NE(fn,nullptr);PFCheck(PFGrid(fn));EXPECT_EQ(PFProjections(fn).errors,0);EXPECT_EQ(PFBoundary(fn).errors,0);EXPECT_EQ(PFScales(fn).errors,0);EXPECT_EQ(PFWide(fn).errors,0);EXPECT_EQ(PFSeparatedExponents(fn).errors,0);EXPECT_EQ(PFAliases(fn).errors,0);EXPECT_EQ(PFLiterals(fn).errors,0);EXPECT_EQ(PFFootprint(fn).errors,0);EXPECT_NE(FreeLibrary(dll),0);}
#endif
#endif
