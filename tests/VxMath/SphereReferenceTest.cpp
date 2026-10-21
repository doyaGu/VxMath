#include <cfloat>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstddef>
#include <limits>
#include <type_traits>
#include "VxSphere.h"
#ifndef VX_SPHERE_NATIVE_PROBE
#include <gtest/gtest.h>
#endif

struct SphereSamples { int samples=0,errors=0; unsigned digest=2166136261u; };

static long double SpherePointDistance(const VxVector &center,const VxVector &point) {
    return std::hypot(static_cast<long double>(point.x)-center.x,
        static_cast<long double>(point.y)-center.y,static_cast<long double>(point.z)-center.z);
}

// Enumerating all eight corners is independent of the implementation's
// componentwise farthest-endpoint calculation.
static bool SphereCornerOracle(const VxSphere &sphere,const VxBbox &box) {
    const long double radius=std::fabs(static_cast<long double>(sphere.Radius()));
    for (int corner=0;corner<8;++corner) {
        VxVector p((corner&1)?box.Max.x:box.Min.x,(corner&2)?box.Max.y:box.Min.y,(corner&4)?box.Max.z:box.Min.z);
        if (!(SpherePointDistance(sphere.Center(),p)<radius)) return false;
    }
    return true;
}

static void ObserveSphere(VxSphere &sphere,const VxVector &point,const VxBbox &box,SphereSamples &r) {
    const VxSphere old=sphere;const VxVector oldPoint=point;const VxBbox oldBox=box;
    const long double distance=SpherePointDistance(sphere.Center(),point);
    const long double radius=std::fabs(static_cast<long double>(sphere.Radius()));
    const bool expected[]={distance<=radius,distance==radius,SphereCornerOracle(sphere,box)};
    const bool actual[]={sphere.IsPointInside(point),sphere.IsPointOnSurface(point),sphere.IsBoxTotallyInside(box)};
    for (int n=0;n<3;++n) {
        if (actual[n]!=expected[n]) ++r.errors;
        r.digest=(r.digest^unsigned(actual[n]))*16777619u;
    }
    if (std::memcmp(&sphere,&old,sizeof(sphere)) || std::memcmp(&point,&oldPoint,sizeof(point)) || std::memcmp(&box,&oldBox,sizeof(box))) ++r.errors;
    ++r.samples;
}

static SphereSamples SphereOrdinaryDataset() {
    SphereSamples result;const int coordinates[]={-6,-4,-3,-1,0,1,3,4,6};
    const int radii[]={-5,-3,-1,0,1,3,5};
    for (int cx=-8;cx<=8;cx+=8) for (int cy=-8;cy<=8;cy+=8) for (int cz=-8;cz<=8;cz+=8)
    for (int radius:radii) for (int x:coordinates) for (int y:coordinates) for (int z:coordinates) {
        VxSphere sphere{VxVector(float(cx),float(cy),float(cz)),float(radius)};
        const VxVector point(float(cx+x),float(cy+y),float(cz+z));
        const VxBbox box(point-VxVector(0.5f),point+VxVector(0.5f));ObserveSphere(sphere,point,box,result);
    }
    return result;
}

static SphereSamples SphereScaleDataset() {
    SphereSamples result;const int scales[]={-120,-90,-60,-30,0,30,60,90,120};
    for (int exponent:scales) for (int signs=0;signs<8;++signs) for (int translated=0;translated<2;++translated)
    for (int negative=0;negative<2;++negative) for (int rank=0;rank<6;++rank) {
        const float unit=std::ldexp(1.0f,exponent);
        const VxVector center(translated*16*unit,translated*-8*unit,translated*4*unit);
        VxSphere sphere(center,(negative?-5:5)*unit);
        const float x=(rank%3)*3*unit*((signs&1)?-1:1);
        const float y=(rank%3)*4*unit*((signs&2)?-1:1);
        const float z=(rank>=3?2:0)*unit*((signs&4)?-1:1);
        const VxVector point=center+VxVector(x,y,z);
        const VxVector extent((rank%3)*3*unit,(rank%3)*4*unit,rank>=3?2*unit:0);
        const VxBbox box(center-extent,center+extent);ObserveSphere(sphere,point,box,result);
    }
    return result;
}

#ifndef VX_SPHERE_NATIVE_PROBE
static void RecordSphere(const SphereSamples &r) {
    char text[16];std::snprintf(text,sizeof(text),"%u",r.digest);
    EXPECT_EQ(r.errors,0);testing::Test::RecordProperty("samples",r.samples);testing::Test::RecordProperty("digest",text);
#if defined(VX_SIMD_SSE)
    testing::Test::RecordProperty("compiled_sse",1);
#else
    testing::Test::RecordProperty("compiled_sse",0);
#endif
}

TEST(SphereReferenceTest, ConstructionCopiesValuesAndAccessorsReferenceTheirStorage) {
    VxVector center(1,2,3);VxSphere sphere(center,-5);center.x=20;
    EXPECT_EQ(sphere.Center().x,1);EXPECT_EQ(sphere.Radius(),-5);
    sphere.Center().y=7;sphere.Radius()=9;const VxSphere &view=sphere;
    EXPECT_EQ(&view.Center(),&sphere.Center());EXPECT_EQ(&view.Radius(),&sphere.Radius());
    EXPECT_EQ(view.Center().y,7);EXPECT_EQ(view.Radius(),9);
    EXPECT_TRUE((std::is_same<decltype(view.Center()),const VxVector &>::value));
    EXPECT_TRUE((std::is_same<decltype(view.Radius()),const float &>::value));
}

TEST(SphereReferenceTest, DefaultCenterIsZeroAndRadiusIsAssignedBeforeReading) {
    VxSphere sphere;EXPECT_EQ(sphere.Center().x,0);EXPECT_EQ(sphere.Center().y,0);EXPECT_EQ(sphere.Center().z,0);
    sphere.Radius()=2;EXPECT_TRUE(sphere.IsPointInside(VxVector(1,0,0)));
}

TEST(SphereReferenceTest, ImplicitCopiesAndAssignmentsOwnTheirFields) {
    VxSphere a(VxVector(1,2,3),4),b=a,c;c=a;b.Center().x=9;c.Radius()=8;
    EXPECT_EQ(a.Center().x,1);EXPECT_EQ(a.Radius(),4);EXPECT_EQ(b.Radius(),4);EXPECT_EQ(c.Center().x,1);
    a=a;EXPECT_EQ(a.Radius(),4);
}

TEST(SphereReferenceTest, LayoutRetainsFourFloatsWithNoVectorPadding) {
    VxSphere sphere(VxVector(1,2,3),4);const unsigned char *base=reinterpret_cast<const unsigned char *>(&sphere);
    EXPECT_EQ(sizeof(VxSphere),16u);EXPECT_EQ(alignof(VxSphere),4u);
    EXPECT_EQ(reinterpret_cast<const unsigned char *>(&sphere.Center())-base,0);
    EXPECT_EQ(reinterpret_cast<const unsigned char *>(&sphere.Radius())-base,12);
    EXPECT_TRUE(std::is_standard_layout<VxSphere>::value);EXPECT_TRUE(std::is_trivially_copyable<VxSphere>::value);
    EXPECT_TRUE(std::is_trivially_destructible<VxSphere>::value);
    float raw[4];std::memcpy(raw,&sphere,sizeof(raw));for (int n=0;n<4;++n) EXPECT_EQ(raw[n],float(n+1));
}

TEST(SphereReferenceTest, PointBoundaryIsClosedAndBoxBoundaryIsStrict) {
    VxSphere sphere(VxVector(10,20,30),5);const VxVector p(13,24,30);
    EXPECT_TRUE(sphere.IsPointInside(p));EXPECT_TRUE(sphere.IsPointOnSurface(p));
    EXPECT_FALSE(sphere.IsBoxTotallyInside(VxBbox(p,p)));
    EXPECT_TRUE(sphere.IsBoxTotallyInside(VxBbox(VxVector(9,18,28),VxVector(11,22,32))));
}

TEST(SphereReferenceTest, BoxContainmentRequiresAllCornersRatherThanAnIntersection) {
    VxSphere sphere(VxVector(0,0,0),5);
    EXPECT_FALSE(sphere.IsBoxTotallyInside(VxBbox(VxVector(-1,-1,-1),VxVector(6,1,1))));
    EXPECT_FALSE(sphere.IsBoxTotallyInside(VxBbox(VxVector(-4,-4,-4),VxVector(4,4,4))));
    EXPECT_TRUE(sphere.IsBoxTotallyInside(VxBbox(VxVector(-1,-2,-2),VxVector(1,2,2))));
}

TEST(SphereReferenceTest, ZeroRadiusContainsOnlyItsCenterAndNoBox) {
    VxSphere sphere(VxVector(1,2,3),0);
    EXPECT_TRUE(sphere.IsPointInside(sphere.Center()));EXPECT_TRUE(sphere.IsPointOnSurface(sphere.Center()));
    EXPECT_FALSE(sphere.IsPointInside(VxVector(1,2,4)));
    EXPECT_FALSE(sphere.IsBoxTotallyInside(VxBbox(sphere.Center(),sphere.Center())));
}

TEST(SphereReferenceTest, NegativeRadiusPreservesTheOriginalSquaredMagnitudeContract) {
    VxSphere positive(VxVector(0,0,0),5),negative(VxVector(0,0,0),-5);
    const VxVector point(3,4,0);const VxBbox box(VxVector(-1,-2,-2),VxVector(1,2,2));
    EXPECT_TRUE(negative.IsPointInside(point));EXPECT_TRUE(negative.IsPointOnSurface(point));EXPECT_TRUE(negative.IsBoxTotallyInside(box));
    EXPECT_FALSE(positive==negative);EXPECT_EQ(negative.Radius(),-5);
}

TEST(SphereReferenceTest, EqualityComparesAllFourComponentsNumerically) {
    VxSphere sphere(VxVector(1,2,3),4),copy=sphere;EXPECT_TRUE(sphere==copy);
    for (int n=0;n<3;++n) { copy=sphere;copy.Center()[n]+=1;EXPECT_FALSE(sphere==copy); }
    copy=sphere;copy.Radius()=5;EXPECT_FALSE(sphere==copy);
    VxSphere positive(VxVector(0,0,0),0),negative(VxVector(-0.0f,0,-0.0f),-0.0f);EXPECT_TRUE(positive==negative);
    copy=sphere;copy.Radius()=std::numeric_limits<float>::quiet_NaN();EXPECT_FALSE(copy==copy);
    copy=sphere;copy.Center().z=std::numeric_limits<float>::quiet_NaN();EXPECT_FALSE(copy==copy);
}

TEST(SphereReferenceTest, OrdinaryGeometryMatchesIndependentDistancesAndEightCornerEnumeration) {
    const SphereSamples result=SphereOrdinaryDataset();EXPECT_EQ(result.samples,137781);RecordSphere(result);
}

TEST(SphereReferenceTest, PowersOfTwoPreservePointSurfaceAndStrictBoxClassification) {
    const SphereSamples result=SphereScaleDataset();EXPECT_EQ(result.samples,1728);RecordSphere(result);
}

TEST(SphereReferenceTest, LargeOutsidePointsDoNotBecomeInsideOrOnTheSurface) {
    VxSphere sphere(VxVector(0,0,0),1e20f);const VxVector point(2e20f,0,0);
    EXPECT_FALSE(sphere.IsPointInside(point));EXPECT_FALSE(sphere.IsPointOnSurface(point));
}

TEST(SphereReferenceTest, TinyOutsidePointsDoNotBecomeInsideOrOnTheSurface) {
    VxSphere sphere(VxVector(0,0,0),1e-30f);const VxVector point(2e-30f,0,0);
    EXPECT_FALSE(sphere.IsPointInside(point));EXPECT_FALSE(sphere.IsPointOnSurface(point));
}

TEST(SphereReferenceTest, LargeInteriorBoxRemainsStrictlyInside) {
    VxSphere sphere(VxVector(0,0,0),1e20f);
    EXPECT_TRUE(sphere.IsBoxTotallyInside(VxBbox(VxVector(-1e19f,-2e19f,-2e19f),VxVector(1e19f,2e19f,2e19f))));
}

TEST(SphereReferenceTest, TinyInteriorBoxRemainsStrictlyInside) {
    VxSphere sphere(VxVector(0,0,0),1e-30f);
    EXPECT_TRUE(sphere.IsBoxTotallyInside(VxBbox(VxVector(-1e-31f,-2e-31f,-2e-31f),VxVector(1e-31f,2e-31f,2e-31f))));
}

TEST(SphereReferenceTest, FiniteOppositeCoordinatesCannotBecomeASurfaceThroughSubtractionOverflow) {
    VxSphere sphere(VxVector(FLT_MAX,0,0),FLT_MAX);const VxVector point(-FLT_MAX,0,0);
    EXPECT_FALSE(sphere.IsPointInside(point));EXPECT_FALSE(sphere.IsPointOnSurface(point));
}

TEST(SphereReferenceTest, ExactAxisSurfaceAndInteriorSurviveLargeAndTinyScales) {
    const float radii[]={1e20f,1e-30f};
    for (float radius:radii) {
        VxSphere sphere(VxVector(0,0,0),radius);
        EXPECT_TRUE(sphere.IsPointInside(VxVector(radius,0,0)));EXPECT_TRUE(sphere.IsPointOnSurface(VxVector(radius,0,0)));
        EXPECT_FALSE(sphere.IsPointOnSurface(VxVector(radius*0.5f,0,0)));
    }
}

TEST(SphereReferenceTest, SurfaceComparisonRetainsSmallSquaredComponents) {
    VxSphere sphere(VxVector(0,0,0),1);const VxVector point(1,std::ldexp(1.0f,-20),0);
    EXPECT_FALSE(sphere.IsPointInside(point));EXPECT_FALSE(sphere.IsPointOnSurface(point));
}
#endif
