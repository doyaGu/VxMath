#include <cmath>
#include <cfloat>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <new>
#include <initializer_list>
#ifndef VX_SEGMENT_BOX_NATIVE_PROBE
#ifndef VX_LIB
#define VX_LIB 1
#endif
#ifndef VX_SEGMENT_BOX_IMPLEMENTATION
#define VX_SEGMENT_BOX_IMPLEMENTATION "../../src/VxIntersect_BoxBox.cpp"
#endif
#include VX_SEGMENT_BOX_IMPLEMENTATION
#include <gtest/gtest.h>
void VxIntersectPlaneDispatchRebuild(bool) { FAIL() << "Unrelated plane rebuild"; }
void VxIntersectSphereDispatchRebuild(bool) { FAIL() << "Unrelated sphere rebuild"; }
void VxIntersectFrustumDispatchRebuild(bool) { FAIL() << "Unrelated frustum rebuild"; }
void VxIntersectFaceDispatchRebuild(bool) { FAIL() << "Unrelated face rebuild"; }
#endif
#include "SegmentBoxFaceOracle.h"
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef int (*RBBoolean)(const VxRay &, const VxBbox &);
typedef int (*RBDetailed)(const VxRay &, const VxBbox &, VxVector &, VxVector *, VxVector *, VxVector *);
struct RBSamples {
    int samples = 0, errors = 0, codeErrors = 0, pointErrors = 0, normalErrors = 0, preservationErrors = 0;
    unsigned digest = 2166136261u;
};
static VxRay RBRay(const VxVector &origin, const VxVector &direction) { VxRay r; r.m_Origin = origin; r.m_Direction = direction; return r; }
static bool RBEqual(const VxVector &actual, const VxVector &expected) {
    for (int i = 0; i < 3; ++i) if (actual[i] != expected[i] &&
        !(std::fabs(double(actual[i]) - expected[i]) <= std::fabs(double(expected[i])) * 2.0e-6)) return false;
    return true;
}
static void RBCode(RBSamples &r, int actual, int expected) {
    ++r.samples; r.codeErrors += actual != expected; r.digest = (r.digest ^ unsigned(actual)) * 16777619u;
}
static void RBBool(RBSamples &r, RBBoolean fn, const VxRay &ray, const VxBbox &box) {
    const VxRay beforeRay = ray; const VxBbox beforeBox = box;
    const int actual = fn(ray, box), expected = SegmentBoxFaceOracle(ray, box, false).code;
    const bool mutated = std::memcmp(&ray, &beforeRay, sizeof(ray)) || std::memcmp(&box, &beforeBox, sizeof(box));
    RBCode(r, actual, expected); r.preservationErrors += mutated; r.errors += actual != expected || mutated;
}
static void RBDetail(RBSamples &r, RBDetailed fn, const VxRay &ray, const VxBbox &box, int mask = 7) {
    const VxRay beforeRay = ray; const VxBbox beforeBox = box;
    VxVector output[] = {VxVector(101, 102, 103), VxVector(201, 202, 203), VxVector(301, 302, 303), VxVector(401, 402, 403)};
    const VxVector saved[] = {output[0], output[1], output[2], output[3]};
    const SegmentBoxFaceResult expected = SegmentBoxFaceOracle(ray, box, true);
    const int actual = fn(ray, box, output[0], mask & 1 ? &output[1] : nullptr, mask & 2 ? &output[2] : nullptr, mask & 4 ? &output[3] : nullptr);
    RBCode(r, actual, expected.code);
    bool failed = actual != expected.code;
    for (int i = 0; i < 4; ++i) {
        const bool present = i == 0 || (mask & (1 << (i - 1)));
        if (!present || expected.code == 0) {
            if (std::memcmp(&output[i], &saved[i], sizeof(VxVector))) { ++r.preservationErrors; failed = true; }
            continue;
        }
#ifdef VX_SEGMENT_BOX_NATIVE_PROBE
        // Do not read/compare original uninitialized normal values. The case
        // and its return/point behavior are still measured, not filtered out.
        if ((i == 2 && !expected.originalNearNormalDefined) || (i == 3 && !expected.originalFarNormalDefined)) continue;
#endif
        const VxVector *want[] = {&expected.entry, &expected.exit, &expected.entryNormal, &expected.exitNormal};
        if (!RBEqual(output[i], *want[i])) { if (i < 2) ++r.pointErrors; else ++r.normalErrors; failed = true; }
    }
    if (std::memcmp(&ray, &beforeRay, sizeof(ray)) || std::memcmp(&box, &beforeBox, sizeof(box))) { ++r.preservationErrors; failed = true; }
    r.errors += failed;
}
static const int RBDirections[][3] = {
    {0,0,0},{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1},
    {1,1,1},{-1,-1,-1},{1,-1,1},{-1,1,-1},{1,1,-1},{-1,-1,1},{-1,1,1},{1,-1,-1},
    {2,1,0},{-2,-1,0},{0,3,-2},{0,-3,2}
};
static RBSamples RBGrid(RBBoolean boolean, RBDetailed detailed) {
    RBSamples r;
    const VxBbox boxes[] = {
        VxBbox(VxVector(-1,-1,-1),VxVector(1,1,1)),VxBbox(VxVector(0,0,0),VxVector(0,0,0)),
        VxBbox(VxVector(0,-1,-1),VxVector(0,1,1)),VxBbox(VxVector(0,-1,0),VxVector(0,1,0)),
        VxBbox(VxVector(1,-2,.5f),VxVector(3,0,1.5f)),VxBbox(VxVector(0,0,0),VxVector(2,2,2)),
        VxBbox(VxVector(-2,-1,-.5f),VxVector(-1,2,.5f))
    };
    for (const VxBbox &box : boxes) for (int x = -2; x <= 2; ++x) for (int y = -2; y <= 2; ++y) for (int z = -2; z <= 2; ++z) for (const auto &d : RBDirections) {
        const VxRay ray = RBRay(VxVector(float(x),float(y),float(z)),VxVector(float(d[0]),float(d[1]),float(d[2])));
        if (boolean) RBBool(r,boolean,ray,box); else RBDetail(r,detailed,ray,box);
    }
    return r;
}
static void RBBoundaryOne(RBSamples &r, RBBoolean boolean, RBDetailed detailed, const VxRay &ray, const VxBbox &box) {
    if (boolean) RBBool(r,boolean,ray,box); else for (int mask = 0; mask < 8; ++mask) RBDetail(r,detailed,ray,box,mask);
}
static RBSamples RBBoundaries(RBBoolean boolean, RBDetailed detailed) {
    RBSamples r; const VxBbox box(VxVector(-1,-1,-1),VxVector(1,1,1));
    for (int axis = 0; axis < 3; ++axis) for (int sign : {-1,1}) {
        for (float coordinate : {std::nextafter(float(sign),-FLT_MAX),float(sign),std::nextafter(float(sign),FLT_MAX)}) for (int toward : {-1,1}) {
            VxVector origin(0,0,0),direction(0,0,0); origin[axis] = coordinate; direction[axis] = float(toward);
            RBBoundaryOne(r,boolean,detailed,RBRay(origin,direction),box);
        }
        for (float speed : {std::nextafter(0x1p-23f,0.0f),0x1p-23f,std::nextafter(0x1p-23f,FLT_MAX)}) {
            VxVector origin(0,0,0),direction(0,0,0); origin[axis] = float(sign*2); direction[axis] = -sign*speed;
            RBBoundaryOne(r,boolean,detailed,RBRay(origin,direction),box);
        }
    }
    for (float value : {std::nextafter(0x1p-23f,0.0f),0x1p-23f,std::nextafter(0x1p-23f,FLT_MAX)}) {
        RBBoundaryOne(r,boolean,detailed,RBRay(VxVector(0,0,0),VxVector(1,0,0)),VxBbox(VxVector(value,-1,-1),VxVector(0x1p-22f,1,1)));
        RBBoundaryOne(r,boolean,detailed,RBRay(VxVector(0,0,0),VxVector(1,0,0)),VxBbox(VxVector(-1,-1,-1),VxVector(value,1,1)));
    }
    RBBoundaryOne(r,boolean,detailed,RBRay(VxVector(-2,-2,-2),VxVector(1,1,1)),box);
    RBBoundaryOne(r,boolean,detailed,RBRay(VxVector(-2,0,0),VxVector(1,1,0)),box);
    RBBoundaryOne(r,boolean,detailed,RBRay(VxVector(-2,-2,-2),VxVector(1,1,1)),VxBbox(VxVector(0,0,0),VxVector(0,0,0)));
    for (const VxVector &d : {VxVector(0,0,0),VxVector(-0.0f,0,0),VxVector(0x1p-24f,0,0)}) {
        RBBoundaryOne(r,boolean,detailed,RBRay(VxVector(0,0,0),d),box);
        RBBoundaryOne(r,boolean,detailed,RBRay(VxVector(1,1,1),d),box);
        RBBoundaryOne(r,boolean,detailed,RBRay(VxVector(2,0,0),d),box);
    }
    return r;
}
static RBSamples RBScales(RBBoolean boolean,RBDetailed detailed) {
    RBSamples r;const int geometryPowers[]={-120,-90,-40,-10,0,10,40,90,120},speedPowers[]={-120,-90,-40,-23,-22,0,40,90,120};
    const VxVector origins[]={VxVector(-2,0,0),VxVector(2,2,0),VxVector(0,0,0),VxVector(-2,-2,-2),VxVector(2,1,0)};
    const VxVector directions[]={VxVector(1,0,0),VxVector(-1,-1,0),VxVector(1,1,1)};
    for (int g:geometryPowers) for (int d:speedPowers) for (const auto &o:origins) for (const auto &direction:directions) {
        const float size=std::ldexp(1.0f,g),speed=std::ldexp(1.0f,d);
        const VxBbox box(VxVector(-size,-size,-size),VxVector(size,size,size));const VxRay ray=RBRay(o*size,direction*speed);
        if (boolean) RBBool(r,boolean,ray,box); else RBDetail(r,detailed,ray,box);
    }
    return r;
}
static RBSamples RBWide(RBBoolean boolean,RBDetailed detailed) {
    RBSamples r;
    for (int axis=0;axis<3;++axis) for (int sign:{-1,1}) {
        VxBbox box(VxVector(-1,-1,-1),VxVector(1,1,1));VxVector o(0,0,0),d(0,0,0);
        box.Min[axis]=1;box.Max[axis]=2;o[axis]=-sign*FLT_MAX;d[axis]=sign*FLT_MAX;
        if(boolean && SegmentBoxFaceOracle(RBRay(o,d),box,false).code!=int(sign<0)){++r.errors;++r.codeErrors;}
        RBBoundaryOne(r,boolean,detailed,RBRay(o,d),box);
        box.Min[axis]=-FLT_MAX;box.Max[axis]=FLT_MAX;o[axis]=0;d[axis]=float(sign);
        RBBoundaryOne(r,boolean,detailed,RBRay(o,d),box);
        box.Min[axis]=FLT_MAX*.5f;box.Max[axis]=FLT_MAX;o[axis]=-FLT_MAX;d[axis]=1;
        RBBoundaryOne(r,boolean,detailed,RBRay(o,d),box);
        box.Min[axis]=-1;box.Max[axis]=1;o[axis]=FLT_MAX;d[axis]=0;
        RBBoundaryOne(r,boolean,detailed,RBRay(o,d),box);
    }
    return r;
}
static RBSamples RBAliases(RBDetailed fn) {
    RBSamples r;const VxBbox box(VxVector(-1,-1,-1),VxVector(1,1,1));const VxRay ray=RBRay(VxVector(-2,0,-3),VxVector(1,.25f,2));
    const SegmentBoxFaceResult expected=SegmentBoxFaceOracle(ray,box,true);
    VxVector one(10,11,12);int rc=fn(ray,box,one,&one,&one,&one);RBCode(r,rc,expected.code);r.errors+=rc!=expected.code||!RBEqual(one,expected.exitNormal);
    VxVector normal(10,11,12);rc=fn(ray,box,one,&one,&normal,&normal);RBCode(r,rc,expected.code);r.errors+=rc!=expected.code||!RBEqual(one,expected.exit)||!RBEqual(normal,expected.exitNormal);
    return r;
}
static RBSamples RBEndpoints(RBBoolean boolean, RBDetailed detailed) {
    RBSamples r;
    const float epsilon=0x1p-23f, limit=1.0f+epsilon;
    struct EndpointCase { float entry,exit; int boolean,detail; };
    const EndpointCase cases[]={
        {-.5f,.5f,1,-1},{-.5f,1,1,-1},{-.5f,limit,1,-1},{-.5f,std::nextafter(limit,FLT_MAX),1,0},
        {0,.5f,1,-1},{0,2,1,0},{std::nextafter(epsilon,0.0f),2,1,0},{epsilon,2,1,-1},
        {std::nextafter(epsilon,FLT_MAX),2,1,1},{1,2,1,1},{limit,2,0,1},{std::nextafter(limit,FLT_MAX),2,0,0},
        {-1,0,1,0},{-1,std::nextafter(epsilon,0.0f),1,0},{-1,epsilon,1,-1},{1,1,1,1},{0,0,1,0}
    };
    for(int axis=0;axis<3;++axis) for(int sign:{-1,1}) for(const EndpointCase &c:cases) {
        VxBbox box(VxVector(-1,-1,-1),VxVector(1,1,1));VxVector direction(0,0,0);
        box.Min[axis]=sign>0?c.entry:-c.exit;box.Max[axis]=sign>0?c.exit:-c.entry;direction[axis]=float(sign);
        const VxRay segment=RBRay(VxVector(0,0,0),direction);
        const int literal=boolean?c.boolean:c.detail;
        if(SegmentBoxFaceOracle(segment,box,boolean==nullptr).code!=literal){++r.errors;++r.codeErrors;}
        RBBoundaryOne(r,boolean,detailed,segment,box);
    }
    return r;
}
#if defined(_WIN32)
static RBSamples RBFootprint(RBBoolean boolean,RBDetailed detailed) {
    RBSamples r;SYSTEM_INFO info;GetSystemInfo(&info);const size_t page=info.dwPageSize;
    unsigned char *pages=static_cast<unsigned char *>(VirtualAlloc(nullptr,page*12,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if (!pages) {r.errors=1;return r;}
    VxRay *ray=new(pages+page-sizeof(VxRay)) VxRay;ray->m_Origin.Set(-2,0,0);ray->m_Direction.Set(1,0,0);
    VxBbox *box=new(pages+page*3-sizeof(VxBbox)) VxBbox(VxVector(-1,-1,-1),VxVector(1,1,1));
    VxVector *out[4];for(int i=0;i<4;++i)out[i]=new(pages+page*(5+2*i)-sizeof(VxVector)) VxVector(100,101,102);
    DWORD old;bool okay=true;
    for(int i=0;i<6;++i)okay=VirtualProtect(pages+page*(2*i+1),page,PAGE_NOACCESS,&old)&&okay;
    for(int i=0;i<2;++i)okay=VirtualProtect(pages+page*(2*i),page,PAGE_READONLY,&old)&&okay;
    if (okay) {
        RBBool(r,boolean,*ray,*box);const SegmentBoxFaceResult expected=SegmentBoxFaceOracle(*ray,*box,true);
        const int rc=detailed(*ray,*box,*out[0],out[1],out[2],out[3]);RBCode(r,rc,expected.code);
        r.errors+=rc!=expected.code||!RBEqual(*out[0],expected.entry)||!RBEqual(*out[1],expected.exit)||!RBEqual(*out[2],expected.entryNormal)||!RBEqual(*out[3],expected.exitNormal);
        const int optional=detailed(*ray,*box,*out[0],nullptr,nullptr,nullptr);RBCode(r,optional,expected.code);r.errors+=optional!=expected.code||!RBEqual(*out[0],expected.entry);
    } else ++r.errors;
    VirtualFree(pages,0,MEM_RELEASE);return r;
}
#endif
#ifndef VX_SEGMENT_BOX_NATIVE_PROBE
static void RBCheck(RBSamples r) {
    EXPECT_EQ(r.errors,0)<<"return="<<r.codeErrors<<" points="<<r.pointErrors<<" normals="<<r.normalErrors<<" preservation="<<r.preservationErrors;
    ::testing::Test::RecordProperty("samples",r.samples);::testing::Test::RecordProperty("digest",std::to_string(r.digest));
}
TEST(SegmentBoxReference, ScalarBooleanFaceEnumeration) {RBCheck(RBGrid(SegmentBoxCore<false>,nullptr));}
TEST(SegmentBoxReference, ScalarDetailedFaceEnumeration) {RBCheck(RBGrid(nullptr,SegmentBoxDetailedCore<false>));}
TEST(SegmentBoxReference, ScalarClosedFacesAndForwardThreshold) {RBCheck(RBBoundaries(SegmentBoxCore<false>,nullptr));}
TEST(SegmentBoxReference, ScalarOptionalOutputsAndParallelThreshold) {RBCheck(RBBoundaries(nullptr,SegmentBoxDetailedCore<false>));}
TEST(SegmentBoxReference, ScalarIndependentGeometryAndDirectionScales) {RBCheck(RBScales(SegmentBoxCore<false>,nullptr));EXPECT_EQ(RBScales(nullptr,SegmentBoxDetailedCore<false>).errors,0);}
TEST(SegmentBoxReference, ScalarFiniteExtremesAndFullLineOutputs) {RBCheck(RBWide(SegmentBoxCore<false>,nullptr));EXPECT_EQ(RBWide(nullptr,SegmentBoxDetailedCore<false>).errors,0);}
TEST(SegmentBoxReference, ScalarOutputAliasingOrder) {RBCheck(RBAliases(SegmentBoxDetailedCore<false>));}
TEST(SegmentBoxReference, ScalarClosedSegmentEndpoints) {RBCheck(RBEndpoints(SegmentBoxCore<false>,nullptr));}
TEST(SegmentBoxReference, ScalarSurfaceCrossingEndpointTolerance) {RBCheck(RBEndpoints(nullptr,SegmentBoxDetailedCore<false>));}
#if defined(VX_SIMD_SSE)
TEST(SegmentBoxReference, SimdBooleanFaceEnumeration) {RBCheck(RBGrid(SegmentBoxCore<true>,nullptr));}
TEST(SegmentBoxReference, SimdDetailedFaceEnumeration) {RBCheck(RBGrid(nullptr,SegmentBoxDetailedCore<true>));}
TEST(SegmentBoxReference, SimdClosedFacesAndForwardThreshold) {RBCheck(RBBoundaries(SegmentBoxCore<true>,nullptr));}
TEST(SegmentBoxReference, SimdOptionalOutputsAndParallelThreshold) {RBCheck(RBBoundaries(nullptr,SegmentBoxDetailedCore<true>));}
TEST(SegmentBoxReference, SimdIndependentGeometryAndDirectionScales) {RBCheck(RBScales(SegmentBoxCore<true>,nullptr));EXPECT_EQ(RBScales(nullptr,SegmentBoxDetailedCore<true>).errors,0);}
TEST(SegmentBoxReference, SimdFiniteExtremesAndFullLineOutputs) {RBCheck(RBWide(SegmentBoxCore<true>,nullptr));EXPECT_EQ(RBWide(nullptr,SegmentBoxDetailedCore<true>).errors,0);}
TEST(SegmentBoxReference, SimdOutputAliasingOrder) {RBCheck(RBAliases(SegmentBoxDetailedCore<true>));}
TEST(SegmentBoxReference, SimdClosedSegmentEndpoints) {RBCheck(RBEndpoints(SegmentBoxCore<true>,nullptr));}
TEST(SegmentBoxReference, SimdSurfaceCrossingEndpointTolerance) {RBCheck(RBEndpoints(nullptr,SegmentBoxDetailedCore<true>));}
#endif
TEST(SegmentBoxReference, PublicForwardersUseSelectedBoxTable) {
    const auto *saved=GetVxIntersectBoxDispatchTable();
    for(int i=0;i<4;++i){const auto *table=&kVxIntersectBoxDispatchScalar;
#if defined(VX_SIMD_SSE)
        if(i&1)table=&kVxIntersectBoxDispatchSIMD;
#endif
        g_VxIntersectBoxDispatch.store(table,std::memory_order_release);
        RBCheck(RBBoundaries(static_cast<RBBoolean>(VxIntersect::SegmentBox),nullptr));EXPECT_EQ(RBBoundaries(nullptr,static_cast<RBDetailed>(VxIntersect::SegmentBox)).errors,0);
    }
    g_VxIntersectBoxDispatch.store(saved,std::memory_order_release);
}
#if defined(_WIN32)
TEST(SegmentBoxReference, ScalarReadonlyInputsAndExactOutputFootprints) {RBCheck(RBFootprint(SegmentBoxCore<false>,SegmentBoxDetailedCore<false>));}
#if defined(VX_SIMD_SSE)
TEST(SegmentBoxReference, SimdReadonlyInputsAndExactOutputFootprints) {RBCheck(RBFootprint(SegmentBoxCore<true>,SegmentBoxDetailedCore<true>));}
#endif
TEST(SegmentBoxReference, ActualCurrentDllExports) {
    HMODULE dll=LoadLibraryA("VxMath.dll");ASSERT_NE(dll,nullptr);
#if defined(_WIN64)
    const char *simple="?SegmentBox@VxIntersect@@SAHAEBVVxRay@@AEBUVxBbox@@@Z",*detail="?SegmentBox@VxIntersect@@SAHAEBVVxRay@@AEBUVxBbox@@AEAUVxVector@@PEAU4@33@Z";
#else
    const char *simple="?SegmentBox@VxIntersect@@SAHABVVxRay@@ABUVxBbox@@@Z",*detail="?SegmentBox@VxIntersect@@SAHABVVxRay@@ABUVxBbox@@AAUVxVector@@PAU4@33@Z";
#endif
    RBBoolean boolean=reinterpret_cast<RBBoolean>(GetProcAddress(dll,simple));RBDetailed detailed=reinterpret_cast<RBDetailed>(GetProcAddress(dll,detail));ASSERT_NE(boolean,nullptr);ASSERT_NE(detailed,nullptr);
    RBCheck(RBGrid(boolean,nullptr));EXPECT_EQ(RBGrid(nullptr,detailed).errors,0);EXPECT_EQ(RBBoundaries(nullptr,detailed).errors,0);
    EXPECT_EQ(RBScales(boolean,nullptr).errors,0);EXPECT_EQ(RBScales(nullptr,detailed).errors,0);EXPECT_EQ(RBWide(boolean,nullptr).errors,0);EXPECT_EQ(RBWide(nullptr,detailed).errors,0);EXPECT_EQ(RBAliases(detailed).errors,0);EXPECT_EQ(RBEndpoints(boolean,nullptr).errors,0);EXPECT_EQ(RBEndpoints(nullptr,detailed).errors,0);EXPECT_EQ(RBFootprint(boolean,detailed).errors,0);
    EXPECT_NE(FreeLibrary(dll),0);
}
#endif
#endif
