#include <cfloat>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <new>
#include "VxPlane.h"
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#ifndef VX_PLANE_CREATE_NATIVE_PROBE
#include <gtest/gtest.h>
#endif

struct PlaneCreateSamples { int samples=0,errors=0;unsigned digest=2166136261u; };
typedef void (*PlanePointCreate)(VxPlane &,const VxVector &,const VxVector &);
typedef void (*PlaneTriangleCreate)(VxPlane &,const VxVector &,const VxVector &,const VxVector &);
static void PlanePointMember(VxPlane &plane,const VxVector &n,const VxVector &p) { plane.Create(n,p); }
static void PlaneTriangleMember(VxPlane &plane,const VxVector &a,const VxVector &b,const VxVector &c) { plane.Create(a,b,c); }

static bool PlaneCreateClose(float actual,float expected) {
    if (expected==0) return actual==0;
    return std::isfinite(actual) && std::fabs((double(actual)-expected)/expected)<=2e-6;
}

static void ObservePlaneCreate(VxPlane &plane,const VxVector &normal,float d,PlaneCreateSamples &r) {
    const float expected[]={normal.x,normal.y,normal.z,d};
    const float actual[]={plane.m_Normal.x,plane.m_Normal.y,plane.m_Normal.z,plane.m_D};
    for (int n=0;n<4;++n) {
        if (!PlaneCreateClose(actual[n],expected[n])) ++r.errors;
        // Opposite signed zeros are geometrically identical. Raw normal storage
        // preservation is checked separately, without this digest normalization.
        unsigned bits=0;if (actual[n]!=0) std::memcpy(&bits,&actual[n],sizeof(bits));
        for (int byte=0;byte<4;++byte) { r.digest=(r.digest^(bits&255u))*16777619u;bits>>=8; }
    }
    ++r.samples;
}

static PlaneCreateSamples PlanePointDataset(PlanePointCreate create=PlanePointMember) {
    PlaneCreateSamples result;
    for (int x=-2;x<=2;++x) for (int y=-2;y<=2;++y) for (int z=-2;z<=2;++z)
    for (int a=-4;a<=4;a+=2) for (int b=-4;b<=4;b+=2) for (int c=-4;c<=4;c+=2) {
        const VxVector normal{float(x),float(y),float(z)},point{float(a),float(b),float(c)};
        const VxVector oldNormal=normal,oldPoint=point;VxPlane plane;create(plane,normal,point);
        ObservePlaneCreate(plane,normal,float(-(x*a+y*b+z*c)),result);
        if (std::memcmp(&normal,&oldNormal,sizeof(normal)) || std::memcmp(&point,&oldPoint,sizeof(point))) ++result.errors;
    }
    return result;
}

static PlaneCreateSamples PlaneTriangleDataset(PlaneTriangleCreate create=PlaneTriangleMember) {
    PlaneCreateSamples result;const int sizes[]={1,2,4,8};
    for (int x=-4;x<=4;x+=4) for (int y=-4;y<=4;y+=4) for (int z=-4;z<=4;z+=4)
    for (int axis=0;axis<3;++axis) for (int width:sizes) for (int height:sizes) for (int reverse=0;reverse<2;++reverse) {
        const VxVector a{float(x),float(y),float(z)};VxVector b=a,c=a,normal(0,0,0);
        b[(axis+1)%3]+=float(width);c[(axis+2)%3]+=float(height);normal[axis]=reverse?-1.0f:1.0f;
        VxPlane plane;create(plane,a,reverse?c:b,reverse?b:c);
        ObservePlaneCreate(plane,normal,-normal[axis]*a[axis],result);
    }
    return result;
}

static PlaneCreateSamples PlaneTriangleScaleDataset(PlaneTriangleCreate create=PlaneTriangleMember) {
    PlaneCreateSamples result;const int scales[]={-120,-90,-60,-30,0,30,60,90,120};
    const VxVector edge0[]={VxVector(1,0,0),VxVector(0,1,0),VxVector(0,0,1),VxVector(1,0,0)};
    const VxVector edge1[]={VxVector(0,1,1),VxVector(1,0,1),VxVector(1,1,0),VxVector(0,1,0)};
    const float diagonal=float(1/std::sqrt(2.0));
    const VxVector normals[]={VxVector(0,-diagonal,diagonal),VxVector(diagonal,0,-diagonal),VxVector(-diagonal,diagonal,0),VxVector(0,0,1)};
    for (int exponent:scales) for (int kind=0;kind<4;++kind) for (int reverse=0;reverse<2;++reverse) for (int translated=0;translated<2;++translated) {
        const float unit=std::ldexp(1.0f,exponent);
        const VxVector a(3*translated*unit,-2*translated*unit,translated*unit),b=a+edge0[kind]*unit,c=a+edge1[kind]*unit;
        const VxVector normal=normals[kind]*(reverse?-1.0f:1.0f);VxPlane plane;
        create(plane,a,reverse?c:b,reverse?b:c);
        const float d=float(-(double(normal.x)*a.x+double(normal.y)*a.y+double(normal.z)*a.z));
        ObservePlaneCreate(plane,normal,d,result);
    }
    return result;
}

static PlaneCreateSamples PlanePointAliases(PlanePointCreate create=PlanePointMember) {
    PlaneCreateSamples result;const VxVector normal(0,0,2),point(2,-1,5);
    VxPlane plane(VxVector(3,4,0),123);create(plane,plane.m_Normal,point);ObservePlaneCreate(plane,VxVector(3,4,0),-2,result);
    plane=VxPlane(VxVector(10,20,30),123);create(plane,normal,plane.m_Normal);ObservePlaneCreate(plane,normal,-60,result);
    plane=VxPlane(VxVector(3,4,0),123);create(plane,plane.m_Normal,plane.m_Normal);ObservePlaneCreate(plane,VxVector(3,4,0),-25,result);
    return result;
}

static PlaneCreateSamples PlaneTriangleAliases(PlaneTriangleCreate create=PlaneTriangleMember) {
    PlaneCreateSamples result;const VxVector a(1,2,3),b(2,2,3),c(1,3,4);
    const float diagonal=float(1/std::sqrt(2.0));
    for (int alias=0;alias<3;++alias) {
        VxPlane plane(alias==0?a:alias==1?b:c,123);
        create(plane,alias==0?plane.m_Normal:a,alias==1?plane.m_Normal:b,alias==2?plane.m_Normal:c);
        ObservePlaneCreate(plane,VxVector(0,-diagonal,diagonal),-diagonal,result);
    }
    return result;
}

#ifndef VX_PLANE_CREATE_NATIVE_PROBE
static void RecordPlaneCreate(const PlaneCreateSamples &r) {
    char text[16];std::snprintf(text,sizeof(text),"%u",r.digest);EXPECT_EQ(r.errors,0);
    testing::Test::RecordProperty("samples",r.samples);testing::Test::RecordProperty("digest",text);
#if defined(VX_SIMD_SSE)
    testing::Test::RecordProperty("compiled_sse",1);
#else
    testing::Test::RecordProperty("compiled_sse",0);
#endif
#if defined(VX_SIMD_SSE2)
    testing::Test::RecordProperty("compiled_sse2",1);
#else
    testing::Test::RecordProperty("compiled_sse2",0);
#endif
}

TEST(PlaneCreateReferenceTest, NormalPointConstructorAndCreatePreserveRawCoefficients) {
    const VxVector normal(0,0,2),point(1,2,5);VxPlane constructed(normal,point),created;created.Create(normal,point);
    EXPECT_EQ(constructed.m_Normal,normal);EXPECT_FLOAT_EQ(constructed.m_D,-10);
    EXPECT_EQ(created,constructed);EXPECT_FLOAT_EQ(created.Classify(VxVector(0,0,7)),4);
}

TEST(PlaneCreateReferenceTest, PointGridMatchesOriginalRawNormalAndIntegerPlaneEquations) {
    const PlaneCreateSamples r=PlanePointDataset();EXPECT_EQ(r.samples,15625);RecordPlaneCreate(r);
}

TEST(PlaneCreateReferenceTest, NormalPointCreationPreservesSignedZeroAndNormalMagnitude) {
    const VxVector normal(-0.0f,2,-0.0f),point(1,3,5);VxPlane plane(normal,point);
    EXPECT_EQ(std::memcmp(&plane.m_Normal,&normal,sizeof(normal)),0);EXPECT_FLOAT_EQ(plane.m_D,-6);
}

TEST(PlaneCreateReferenceTest, PointInputMayAliasTheNormalBeingOverwritten) {
    const PlaneCreateSamples r=PlanePointAliases();EXPECT_EQ(r.samples,3);RecordPlaneCreate(r);
}

TEST(PlaneCreateReferenceTest, RawNormalDotProductCancellationRetainsAFiniteCoefficient) {
    const VxVector normal(FLT_MAX,FLT_MAX,1),point(2,-2,3);VxPlane plane(normal,point);
    EXPECT_EQ(plane.m_Normal,normal);EXPECT_FLOAT_EQ(plane.m_D,-3);
}

TEST(PlaneCreateReferenceTest, ZeroNormalIsStoredWithoutInventingAnAxis) {
    VxPlane plane(VxVector(0,0,0),VxVector(4,5,6));
    EXPECT_EQ(plane.m_Normal,VxVector(0,0,0));EXPECT_EQ(plane.m_D,0);
}

TEST(PlaneCreateReferenceTest, ThreePointConstructorMatchesCreateAndVertexWinding) {
    const VxVector a(1,2,3),b(2,2,3),c(1,3,3);VxPlane plane(a,b,c),created;created.Create(a,b,c);
    EXPECT_EQ(plane,created);EXPECT_EQ(plane.m_Normal,VxVector(0,0,1));EXPECT_FLOAT_EQ(plane.m_D,-3);
    VxPlane reverse(a,c,b);EXPECT_EQ(reverse.m_Normal,VxVector(0,0,-1));EXPECT_FLOAT_EQ(reverse.m_D,3);
}

TEST(PlaneCreateReferenceTest, OrdinaryTriangleGridMatchesIndependentAxisPlanes) {
    const PlaneCreateSamples r=PlaneTriangleDataset();EXPECT_EQ(r.samples,2592);RecordPlaneCreate(r);
}

TEST(PlaneCreateReferenceTest, NonCollinearTrianglesRetainTheirPlaneAcrossBinaryScales) {
    const PlaneCreateSamples r=PlaneTriangleScaleDataset();EXPECT_EQ(r.samples,144);RecordPlaneCreate(r);
}

TEST(PlaneCreateReferenceTest, TinyNonCollinearTriangleDoesNotBecomeTheFallbackZPlane) {
    VxPlane plane(VxVector(0,0,0),VxVector(1e-30f,0,0),VxVector(0,1e-30f,1e-30f));
    EXPECT_FLOAT_EQ(plane.m_Normal.x,0);EXPECT_FLOAT_EQ(plane.m_Normal.y,-float(1/std::sqrt(2.0)));
    EXPECT_FLOAT_EQ(plane.m_Normal.z,float(1/std::sqrt(2.0)));EXPECT_EQ(plane.m_D,0);
}

TEST(PlaneCreateReferenceTest, LargeNonCollinearTriangleProducesAFiniteUnitNormal) {
    VxPlane plane(VxVector(0,0,0),VxVector(1e20f,0,0),VxVector(0,1e20f,1e20f));
    EXPECT_FLOAT_EQ(plane.m_Normal.x,0);EXPECT_FLOAT_EQ(plane.m_Normal.y,-float(1/std::sqrt(2.0)));
    EXPECT_FLOAT_EQ(plane.m_Normal.z,float(1/std::sqrt(2.0)));EXPECT_EQ(plane.m_D,0);
}

TEST(PlaneCreateReferenceTest, FiniteOppositeEdgeCoordinatesDoNotOverflowTheirDifferences) {
    VxPlane plane(VxVector(FLT_MAX,0,0),VxVector(-FLT_MAX,0,0),VxVector(0,1,1));
    EXPECT_FLOAT_EQ(plane.m_Normal.x,0);EXPECT_FLOAT_EQ(plane.m_Normal.y,float(1/std::sqrt(2.0)));
    EXPECT_FLOAT_EQ(plane.m_Normal.z,-float(1/std::sqrt(2.0)));EXPECT_EQ(plane.m_D,0);
}

TEST(PlaneCreateReferenceTest, TriangleVerticesMayAliasTheDestinationNormal) {
    const PlaneCreateSamples r=PlaneTriangleAliases();EXPECT_EQ(r.samples,3);RecordPlaneCreate(r);
}

#if defined(VX_SIMD_SSE)
static void PlanePointSimd(VxPlane &plane,const VxVector &normal,const VxVector &point) {
    VxSIMDPlaneCreateFromPoint(&plane,&normal,&point);
}
static void PlaneTriangleSimd(VxPlane &plane,const VxVector &a,const VxVector &b,const VxVector &c) {
    VxSIMDPlaneCreateFromTriangle(&plane,&a,&b,&c);
}
TEST(PlaneCreateReferenceTest, DirectSimdPointCreationPreservesRawCoefficientsAndAliasedInputs) {
    const PlaneCreateSamples grid=PlanePointDataset(PlanePointSimd),aliases=PlanePointAliases(PlanePointSimd);
    EXPECT_EQ(aliases.samples,3);EXPECT_EQ(aliases.errors,0);RecordPlaneCreate(grid);
}
TEST(PlaneCreateReferenceTest, DirectSimdTriangleCreationRetainsScaledPlanesAndAliasedVertices) {
    const PlaneCreateSamples scales=PlaneTriangleScaleDataset(PlaneTriangleSimd),aliases=PlaneTriangleAliases(PlaneTriangleSimd);
    EXPECT_EQ(aliases.samples,3);EXPECT_EQ(aliases.errors,0);RecordPlaneCreate(scales);
}
#endif

#if defined(_WIN32)
struct PlaneCreatePages {
    unsigned char *base=nullptr;size_t size=0;
    PlaneCreatePages() {
        SYSTEM_INFO info;GetSystemInfo(&info);size=info.dwPageSize;
        base=static_cast<unsigned char *>(VirtualAlloc(nullptr,size*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        if (base) { DWORD old=0;if (!VirtualProtect(base+size,size,PAGE_NOACCESS,&old)) { VirtualFree(base,0,MEM_RELEASE);base=nullptr; } }
    }
    ~PlaneCreatePages() { if (base) VirtualFree(base,0,MEM_RELEASE); }
};

TEST(PlaneCreateReferenceTest, CreationReadsOnlyTheDeclaredVectorAndPlaneFootprints) {
    PlaneCreatePages p,n,a,b,c;ASSERT_NE(p.base,nullptr);ASSERT_NE(n.base,nullptr);ASSERT_NE(a.base,nullptr);ASSERT_NE(b.base,nullptr);ASSERT_NE(c.base,nullptr);
    VxPlane *plane=new(p.base+p.size-sizeof(VxPlane)) VxPlane;
    VxVector *normal=new(n.base+n.size-sizeof(VxVector)) VxVector(0,0,2);
    VxVector *v0=new(a.base+a.size-sizeof(VxVector)) VxVector(1,2,3);
    VxVector *v1=new(b.base+b.size-sizeof(VxVector)) VxVector(2,2,3);
    VxVector *v2=new(c.base+c.size-sizeof(VxVector)) VxVector(1,3,3);
    plane->Create(*normal,*v0);EXPECT_EQ(plane->m_Normal,*normal);EXPECT_FLOAT_EQ(plane->m_D,-6);
    plane->Create(*v0,*v1,*v2);EXPECT_EQ(plane->m_Normal,VxVector(0,0,1));EXPECT_FLOAT_EQ(plane->m_D,-3);
}
#endif
#endif
