#include <cmath>
#include <cfloat>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <new>
#include <initializer_list>
#ifndef VX_LINE_BOX_NATIVE_PROBE
#ifndef VX_LIB
#define VX_LIB 1
#endif
#ifndef VX_LINE_BOX_IMPLEMENTATION
#define VX_LINE_BOX_IMPLEMENTATION "../../src/VxIntersect_BoxBox.cpp"
#endif
#include VX_LINE_BOX_IMPLEMENTATION
#include <gtest/gtest.h>
void VxIntersectPlaneDispatchRebuild(bool) { FAIL() << "Unrelated plane rebuild"; }
void VxIntersectSphereDispatchRebuild(bool) { FAIL() << "Unrelated sphere rebuild"; }
void VxIntersectFrustumDispatchRebuild(bool) { FAIL() << "Unrelated frustum rebuild"; }
void VxIntersectFaceDispatchRebuild(bool) { FAIL() << "Unrelated face rebuild"; }
#endif
#include "LineBoxFaceOracle.h"
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
    const int actual = fn(ray, box), expected = LineBoxFaceOracle(ray, box, false).code;
    const bool mutated = std::memcmp(&ray, &beforeRay, sizeof(ray)) || std::memcmp(&box, &beforeBox, sizeof(box));
    RBCode(r, actual, expected); r.preservationErrors += mutated; r.errors += actual != expected || mutated;
}
static void RBDetail(RBSamples &r, RBDetailed fn, const VxRay &ray, const VxBbox &box, int mask = 7) {
    const VxRay beforeRay = ray; const VxBbox beforeBox = box;
    VxVector output[] = {VxVector(101, 102, 103), VxVector(201, 202, 203), VxVector(301, 302, 303), VxVector(401, 402, 403)};
    const VxVector saved[] = {output[0], output[1], output[2], output[3]};
    const LineBoxFaceResult expected = LineBoxFaceOracle(ray, box, true);
    #ifdef VX_LINE_BOX_NATIVE_PROBE
    // Undefined original indices can write beyond a normal, so omit those
    // pointers rather than reading or invoking an indeterminate write.
    const int actual = fn(ray, box, output[0], mask & 1 ? &output[1] : nullptr,
        (mask & 2) && expected.originalNearNormalDefined ? &output[2] : nullptr,
        (mask & 4) && expected.originalFarNormalDefined ? &output[3] : nullptr);
#else
    const int actual = fn(ray, box, output[0], mask & 1 ? &output[1] : nullptr, mask & 2 ? &output[2] : nullptr, mask & 4 ? &output[3] : nullptr);
#endif
    RBCode(r, actual, expected.code);
    bool failed = actual != expected.code;
    for (int i = 0; i < 4; ++i) {
        const bool present = i == 0 || (mask & (1 << (i - 1)));
        if (!present || expected.code == 0) {
            if (std::memcmp(&output[i], &saved[i], sizeof(VxVector))) { ++r.preservationErrors; failed = true; }
            continue;
        }
#ifdef VX_LINE_BOX_NATIVE_PROBE
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
        if(boolean && LineBoxFaceOracle(RBRay(o,d),box,false).code!=1){++r.errors;++r.codeErrors;}
        if(detailed) {
            // These two crossings are distinct even if both double parameters
            // round to one. A forward line from +FLT_MAX reaches 2 before 1.
            const LineBoxFaceResult expected=LineBoxFaceOracle(RBRay(o,d),box,true);
            const float first=sign<0?2.0f:1.0f,last=sign<0?1.0f:2.0f;
            if(expected.entry[axis]!=first||expected.exit[axis]!=last) ++r.errors;
        }
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
    const LineBoxFaceResult expected=LineBoxFaceOracle(ray,box,true);
    VxVector one(10,11,12);int rc=fn(ray,box,one,&one,&one,&one);RBCode(r,rc,expected.code);r.errors+=rc!=expected.code||!RBEqual(one,expected.exitNormal);
    VxVector normal(10,11,12);rc=fn(ray,box,one,&one,&normal,&normal);RBCode(r,rc,expected.code);r.errors+=rc!=expected.code||!RBEqual(one,expected.exit)||!RBEqual(normal,expected.exitNormal);
    return r;
}
static RBSamples RBWitnesses(RBBoolean boolean, RBDetailed detailed) {
    RBSamples r;
    struct Witness { VxVector origin,direction,first,last; int code; bool tied; };
    const Witness cases[]={
        {VxVector(-3,0,0),VxVector(1,1,0),VxVector(0,0,0),VxVector(0,0,0),0,false},
        {VxVector(-3,-2,0),VxVector(1,1,0),VxVector(-1,0,0),VxVector(0,1,0),1,false},
        {VxVector(3,2,0),VxVector(1,1,0),VxVector(1,0,0),VxVector(0,-1,0),1,false},
        {VxVector(-2,0,0),VxVector(1,1,0),VxVector(-1,1,0),VxVector(-1,1,0),1,false},
        {VxVector(0,0,0),VxVector(1,0,0),VxVector(-1,0,0),VxVector(1,0,0),1,true},
        {VxVector(0,0,0),VxVector(-1,0,0),VxVector(-1,0,0),VxVector(1,0,0),1,true},
        {VxVector(0,0,0),VxVector(1,1,1),VxVector(-1,-1,-1),VxVector(1,1,1),1,true},
        {VxVector(-3,0,0),VxVector(1,0,0),VxVector(-1,0,0),VxVector(1,0,0),1,false},
        {VxVector(3,0,0),VxVector(1,0,0),VxVector(1,0,0),VxVector(-1,0,0),1,false},
        {VxVector(2,0,0),VxVector(0,1,0),VxVector(0,0,0),VxVector(0,0,0),0,false},
        {VxVector(2,0,0),VxVector(0,0,0),VxVector(0,0,0),VxVector(0,0,0),0,false},
        {VxVector(0,0,0),VxVector(0,0,0),VxVector(0,0,0),VxVector(0,0,0),1,false}
    };
    const VxBbox box(VxVector(-1,-1,-1),VxVector(1,1,1));
    for(int rotation=0;rotation<3;++rotation) for(int signs=0;signs<8;++signs) for(const Witness &c:cases) {
        VxVector origin,direction,first,last;
        for(int j=0;j<3;++j) {const int k=(j+rotation)%3; const float sign=signs&(1<<k)?-1.0f:1.0f;
            origin[k]=sign*c.origin[j];direction[k]=sign*c.direction[j];first[k]=sign*c.first[j];last[k]=sign*c.last[j];}
        const VxRay ray=RBRay(origin,direction);const LineBoxFaceResult expected=LineBoxFaceOracle(ray,box,!boolean);
        bool pointMatch=RBEqual(expected.entry,first)&&RBEqual(expected.exit,last);
        if(c.tied) pointMatch=pointMatch||(RBEqual(expected.entry,last)&&RBEqual(expected.exit,first));
        if(expected.code!=c.code || (detailed&&c.code&&!pointMatch)){++r.errors;}
        RBBoundaryOne(r,boolean,detailed,ray,box);
    }
    // Equal distance on a single axis keeps the original Min face first,
    // including negative directions. Lock this independently of the oracle.
    if(detailed) for(int axis=0;axis<3;++axis) for(int sign:{-1,1}) {
        VxVector d(0,0,0),first,last;d[axis]=float(sign);const VxRay ray=RBRay(VxVector(0,0,0),d);
        const int code=detailed(ray,box,first,&last,nullptr,nullptr);RBCode(r,code,1);
        VxVector lo(0,0,0),hi(0,0,0);lo[axis]=-1;hi[axis]=1;
        r.errors+=code!=1||!RBEqual(first,lo)||!RBEqual(last,hi);
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
        RBBool(r,boolean,*ray,*box);const LineBoxFaceResult expected=LineBoxFaceOracle(*ray,*box,true);
        const int rc=detailed(*ray,*box,*out[0],out[1],out[2],out[3]);RBCode(r,rc,expected.code);
        r.errors+=rc!=expected.code||!RBEqual(*out[0],expected.entry)||!RBEqual(*out[1],expected.exit)||!RBEqual(*out[2],expected.entryNormal)||!RBEqual(*out[3],expected.exitNormal);
        const int optional=detailed(*ray,*box,*out[0],nullptr,nullptr,nullptr);RBCode(r,optional,expected.code);r.errors+=optional!=expected.code||!RBEqual(*out[0],expected.entry);
    } else ++r.errors;
    VirtualFree(pages,0,MEM_RELEASE);return r;
}
#endif
#ifndef VX_LINE_BOX_NATIVE_PROBE
static void RBCheck(RBSamples r) {
    EXPECT_EQ(r.errors,0)<<"return="<<r.codeErrors<<" points="<<r.pointErrors<<" normals="<<r.normalErrors<<" preservation="<<r.preservationErrors;
    ::testing::Test::RecordProperty("samples",r.samples);::testing::Test::RecordProperty("digest",std::to_string(r.digest));
}
TEST(LineBoxReference, ScalarBooleanFaceEnumeration) {RBCheck(RBGrid(LineBoxCore<false>,nullptr));}
TEST(LineBoxReference, ScalarDetailedFaceEnumeration) {RBCheck(RBGrid(nullptr,LineBoxDetailedCore<false>));}
TEST(LineBoxReference, ScalarClosedFacesAndZeroDirections) {RBCheck(RBBoundaries(LineBoxCore<false>,nullptr));}
TEST(LineBoxReference, ScalarOptionalOutputsAndParallelThreshold) {RBCheck(RBBoundaries(nullptr,LineBoxDetailedCore<false>));}
TEST(LineBoxReference, ScalarIndependentGeometryAndDirectionScales) {RBCheck(RBScales(LineBoxCore<false>,nullptr));EXPECT_EQ(RBScales(nullptr,LineBoxDetailedCore<false>).errors,0);}
TEST(LineBoxReference, ScalarFiniteExtremesAndFullLineOutputs) {RBCheck(RBWide(LineBoxCore<false>,nullptr));EXPECT_EQ(RBWide(nullptr,LineBoxDetailedCore<false>).errors,0);}
TEST(LineBoxReference, ScalarOutputAliasingOrder) {RBCheck(RBAliases(LineBoxDetailedCore<false>));}
TEST(LineBoxReference, ScalarLiteralLineMembership) {RBCheck(RBWitnesses(LineBoxCore<false>,nullptr));}
TEST(LineBoxReference, ScalarLiteralClosestCrossings) {RBCheck(RBWitnesses(nullptr,LineBoxDetailedCore<false>));}
#if defined(VX_SIMD_SSE)
TEST(LineBoxReference, SimdBooleanFaceEnumeration) {RBCheck(RBGrid(LineBoxCore<true>,nullptr));}
TEST(LineBoxReference, SimdDetailedFaceEnumeration) {RBCheck(RBGrid(nullptr,LineBoxDetailedCore<true>));}
TEST(LineBoxReference, SimdClosedFacesAndZeroDirections) {RBCheck(RBBoundaries(LineBoxCore<true>,nullptr));}
TEST(LineBoxReference, SimdOptionalOutputsAndParallelThreshold) {RBCheck(RBBoundaries(nullptr,LineBoxDetailedCore<true>));}
TEST(LineBoxReference, SimdIndependentGeometryAndDirectionScales) {RBCheck(RBScales(LineBoxCore<true>,nullptr));EXPECT_EQ(RBScales(nullptr,LineBoxDetailedCore<true>).errors,0);}
TEST(LineBoxReference, SimdFiniteExtremesAndFullLineOutputs) {RBCheck(RBWide(LineBoxCore<true>,nullptr));EXPECT_EQ(RBWide(nullptr,LineBoxDetailedCore<true>).errors,0);}
TEST(LineBoxReference, SimdOutputAliasingOrder) {RBCheck(RBAliases(LineBoxDetailedCore<true>));}
TEST(LineBoxReference, SimdLiteralLineMembership) {RBCheck(RBWitnesses(LineBoxCore<true>,nullptr));}
TEST(LineBoxReference, SimdLiteralClosestCrossings) {RBCheck(RBWitnesses(nullptr,LineBoxDetailedCore<true>));}
#endif
TEST(LineBoxReference, PublicForwardersUseSelectedBoxTable) {
    const auto *saved=GetVxIntersectBoxDispatchTable();
    for(int i=0;i<4;++i){const auto *table=&kVxIntersectBoxDispatchScalar;
#if defined(VX_SIMD_SSE)
        if(i&1)table=&kVxIntersectBoxDispatchSIMD;
#endif
        g_VxIntersectBoxDispatch.store(table,std::memory_order_release);
        RBCheck(RBBoundaries(static_cast<RBBoolean>(VxIntersect::LineBox),nullptr));EXPECT_EQ(RBBoundaries(nullptr,static_cast<RBDetailed>(VxIntersect::LineBox)).errors,0);
    }
    g_VxIntersectBoxDispatch.store(saved,std::memory_order_release);
}
#if defined(_WIN32)
TEST(LineBoxReference, ScalarReadonlyInputsAndExactOutputFootprints) {RBCheck(RBFootprint(LineBoxCore<false>,LineBoxDetailedCore<false>));}
#if defined(VX_SIMD_SSE)
TEST(LineBoxReference, SimdReadonlyInputsAndExactOutputFootprints) {RBCheck(RBFootprint(LineBoxCore<true>,LineBoxDetailedCore<true>));}
#endif
TEST(LineBoxReference, ActualCurrentDllExports) {
    HMODULE dll=LoadLibraryA("VxMath.dll");ASSERT_NE(dll,nullptr);
#if defined(_WIN64)
    const char *simple="?LineBox@VxIntersect@@SAHAEBVVxRay@@AEBUVxBbox@@@Z",*detail="?LineBox@VxIntersect@@SAHAEBVVxRay@@AEBUVxBbox@@AEAUVxVector@@PEAU4@33@Z";
#else
    const char *simple="?LineBox@VxIntersect@@SAHABVVxRay@@ABUVxBbox@@@Z",*detail="?LineBox@VxIntersect@@SAHABVVxRay@@ABUVxBbox@@AAUVxVector@@PAU4@33@Z";
#endif
    RBBoolean boolean=reinterpret_cast<RBBoolean>(GetProcAddress(dll,simple));RBDetailed detailed=reinterpret_cast<RBDetailed>(GetProcAddress(dll,detail));ASSERT_NE(boolean,nullptr);ASSERT_NE(detailed,nullptr);
    RBCheck(RBGrid(boolean,nullptr));EXPECT_EQ(RBGrid(nullptr,detailed).errors,0);EXPECT_EQ(RBBoundaries(nullptr,detailed).errors,0);
    EXPECT_EQ(RBScales(boolean,nullptr).errors,0);EXPECT_EQ(RBScales(nullptr,detailed).errors,0);EXPECT_EQ(RBWide(boolean,nullptr).errors,0);EXPECT_EQ(RBWide(nullptr,detailed).errors,0);EXPECT_EQ(RBAliases(detailed).errors,0);EXPECT_EQ(RBWitnesses(boolean,nullptr).errors,0);EXPECT_EQ(RBWitnesses(nullptr,detailed).errors,0);EXPECT_EQ(RBFootprint(boolean,detailed).errors,0);
    EXPECT_NE(FreeLibrary(dll),0);
}
#endif
#endif
