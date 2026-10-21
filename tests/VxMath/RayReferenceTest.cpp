#include <cfloat>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstddef>
#include <limits>
#include <new>
#include <type_traits>
#include "VxRay.h"
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#ifndef VX_RAY_NATIVE_PROBE
#include <gtest/gtest.h>
#endif

struct RaySamples { int samples=0,errors=0;unsigned digest=2166136261u; };

static bool RayClose(float actual,float expected) {
    if (expected==0) return actual==0;
    if (std::isinf(expected)) return actual==expected;
    return std::isfinite(actual) && std::fabs((double(actual)-expected)/expected)<=2e-6;
}

static void ObserveRayFloat(float actual,float expected,RaySamples &r) {
    if (!RayClose(actual,expected)) ++r.errors;
    unsigned bits=0;std::memcpy(&bits,&actual,sizeof(bits));
    for (int n=0;n<4;++n) { r.digest=(r.digest^(bits&255u))*16777619u;bits>>=8; }
}

static VxVector RayAxis(int axis,float length) {
    VxVector out(0,0,0);out[axis%3]=axis<3?length:-length;return out;
}

static void ObserveRayDistance(VxRay &ray,const VxVector &point,double expectedSquared,RaySamples &r) {
    const VxRay old=ray;const VxVector oldPoint=point;
    ObserveRayFloat(ray.SquareDistance(point),float(expectedSquared),r);
    ObserveRayFloat(ray.Distance(point),float(std::sqrt(expectedSquared)),r);
    if (std::memcmp(&ray,&old,sizeof(ray)) || std::memcmp(&point,&oldPoint,sizeof(point))) ++r.errors;
    ++r.samples;
}

static RaySamples RayUnitDistanceDataset() {
    RaySamples result;
    for (int x=-8;x<=8;x+=8) for (int y=-8;y<=8;y+=8) for (int z=-8;z<=8;z+=8)
    for (int axis=0;axis<6;++axis) for (int a=-4;a<=4;a+=2) for (int b=-4;b<=4;b+=2) for (int c=-4;c<=4;c+=2) {
        VxRay ray(VxVector(float(x),float(y),float(z)),RayAxis(axis,1),nullptr);
        const VxVector point(float(x+a),float(y+b),float(z+c));
        const int perpendicular[]={b*b+c*c,a*a+c*c,a*a+b*b};
        ObserveRayDistance(ray,point,perpendicular[axis%3],result);
    }
    return result;
}

static RaySamples RayScaleDistanceDataset() {
    RaySamples result;const int directionScales[]={-120,-90,-60,-30,0,30,60,90,120};
    const int pointScales[]={-100,-50,0,50,100};
    for (int d:directionScales) for (int exponent:pointScales) for (int axis=0;axis<6;++axis)
    for (int parallel=0;parallel<3;++parallel) for (int translated=0;translated<2;++translated) {
        const float unit=std::ldexp(1.0f,exponent);
        const VxVector origin(translated*8*unit,translated*-4*unit,translated*2*unit);
        VxRay ray(origin,RayAxis(axis,std::ldexp(1.0f,d)),nullptr);VxVector point=origin;
        point[axis%3]+=parallel==2?std::ldexp(unit,20):parallel*unit;
        point[(axis+1)%3]+=3*unit;point[(axis+2)%3]+=4*unit;
        ObserveRayDistance(ray,point,25*double(unit)*unit,result);
    }
    return result;
}

static const float RayMatrices[8][16]={
    {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1},
    {1,0,0,0, 0,1,0,0, 0,0,1,0, 11,-7,5,1},
    {2,0,0,0, 0,-3,0,0, 0,0,0.5f,0, 0,0,0,1},
    {0,1,0,0, -1,0,0,0, 0,0,1,0, 0,0,0,1},
    {1,2,0,0, 0,1,3,0, 4,0,1,0, 1,2,3,1},
    {1,0,0,0, 0,1,0,0, 0,0,0,0, 0,0,0,1},
    {0,0,0,0, 0,0,0,0, 0,0,0,0, 11,-7,5,1},
    {1,0,0,7, 0,1,0,8, 0,0,1,9, 11,-7,5,10}
};

static VxVector RayExpectedTransform(const VxVector &v,int kind,bool position) {
    switch (kind) {
        case 1:case 7:return v+(position?VxVector(11,-7,5):VxVector(0,0,0));
        case 2:return VxVector(2*v.x,-3*v.y,0.5f*v.z);
        case 3:return VxVector(-v.y,v.x,v.z);
        case 4:return VxVector(v.x+4*v.z+(position?1:0),2*v.x+v.y+(position?2:0),3*v.y+v.z+(position?3:0));
        case 5:return VxVector(v.x,v.y,0);
        case 6:return position?VxVector(11,-7,5):VxVector(0,0,0);
        default:return v;
    }
}

typedef void (*RayTransformFunction)(VxRay &,VxRay &,const VxMatrix &);
static void RayTransformMember(VxRay &ray,VxRay &out,const VxMatrix &matrix) { ray.Transform(out,matrix); }

static RaySamples RayTransformDataset(RayTransformFunction call=RayTransformMember) {
    RaySamples result;
    for (int kind=0;kind<8;++kind) for (int signs=0;signs<8;++signs) for (int alias=0;alias<2;++alias) {
        VxMatrix matrix;std::memcpy(&matrix,RayMatrices[kind],sizeof(matrix));const VxMatrix oldMatrix=matrix;
        const VxVector origin((signs&1)?-2:2,(signs&2)?-3:3,(signs&4)?-4:4);
        const VxVector direction((signs&4)?-5:5,(signs&1)?-6:6,(signs&2)?-7:7);
        VxRay ray(origin,direction,nullptr),dest;const VxRay old=ray;VxRay &out=alias?ray:dest;call(ray,out,matrix);
        const VxVector expectedOrigin=RayExpectedTransform(origin,kind,true),expectedDirection=RayExpectedTransform(direction,kind,false);
        for (int n=0;n<3;++n) { ObserveRayFloat(out.m_Origin[n],expectedOrigin[n],result);ObserveRayFloat(out.m_Direction[n],expectedDirection[n],result); }
        if (std::memcmp(&matrix,&oldMatrix,sizeof(matrix)) || (!alias && std::memcmp(&ray,&old,sizeof(ray)))) ++result.errors;
        ++result.samples;
    }
    return result;
}

static RaySamples RayInterpolationDataset() {
    RaySamples result;const float parameters[]={-2,-1,-0.5f,0,0.5f,1,2};
    for (int signs=0;signs<8;++signs) for (float t:parameters) for (int alias=0;alias<3;++alias) {
        const VxVector origin((signs&1)?-2:2,(signs&2)?-3:3,(signs&4)?-4:4);
        const VxVector direction((signs&4)?-5:5,(signs&1)?-6:6,(signs&2)?-7:7);
        VxRay ray(origin,direction,nullptr);VxVector value;VxVector &out=alias==1?ray.m_Origin:alias==2?ray.m_Direction:value;
        ray.Interpolate(out,t);
        for (int n=0;n<3;++n) {
            ObserveRayFloat(out[n],float(double(origin[n])+double(t)*direction[n]),result);
            if (alias!=1 && ray.m_Origin[n]!=origin[n]) ++result.errors;
            if (alias!=2 && ray.m_Direction[n]!=direction[n]) ++result.errors;
        }
        ++result.samples;
    }
    return result;
}

#ifndef VX_RAY_NATIVE_PROBE
static void RecordRay(const RaySamples &r) {
    char text[16];std::snprintf(text,sizeof(text),"%u",r.digest);EXPECT_EQ(r.errors,0);
    testing::Test::RecordProperty("samples",r.samples);testing::Test::RecordProperty("digest",text);
#if defined(VX_SIMD_SSE)
    testing::Test::RecordProperty("compiled_sse",1);
#else
    testing::Test::RecordProperty("compiled_sse",0);
#endif
}

TEST(RayReferenceTest, ConstructorsDistinguishEndpointsFromRawDirectionsWithoutNormalizing) {
    VxRay empty;EXPECT_EQ(empty.m_Origin,VxVector(0,0,0));EXPECT_EQ(empty.m_Direction,VxVector(0,0,0));
    VxVector start(1,2,3),end(5,8,11);VxRay endpoints(start,end);int dummy=27;VxRay direction(start,end,&dummy);
    EXPECT_EQ(endpoints.m_Origin,start);EXPECT_EQ(endpoints.m_Direction,VxVector(4,6,8));
    EXPECT_EQ(direction.m_Direction,end);EXPECT_EQ(dummy,27);start.x=99;EXPECT_EQ(endpoints.m_Origin.x,1);
}

TEST(RayReferenceTest, AccessorsReturnConstAndMutableReferencesToTheSixFloatLayout) {
    VxRay ray(VxVector(1,2,3),VxVector(4,5,6),nullptr);const VxRay &view=ray;
    EXPECT_EQ(sizeof(VxRay),24u);EXPECT_EQ(alignof(VxRay),4u);EXPECT_EQ(offsetof(VxRay,m_Origin),0u);EXPECT_EQ(offsetof(VxRay,m_Direction),12u);
    EXPECT_EQ(&ray.GetOrigin(),&ray.m_Origin);EXPECT_EQ(&view.GetOrigin(),&ray.m_Origin);
    EXPECT_EQ(&ray.GetDirection(),&ray.m_Direction);EXPECT_EQ(&view.GetDirection(),&ray.m_Direction);
    ray.GetOrigin().x=7;ray.GetDirection().z=8;EXPECT_EQ(view.GetOrigin().x,7);EXPECT_EQ(view.GetDirection().z,8);
    EXPECT_TRUE((std::is_same<decltype(view.GetOrigin()),const VxVector &>::value));
    EXPECT_TRUE((std::is_same<decltype(view.GetDirection()),const VxVector &>::value));
    EXPECT_TRUE(std::is_standard_layout<VxRay>::value);EXPECT_TRUE(std::is_trivially_copyable<VxRay>::value);EXPECT_TRUE(std::is_trivially_destructible<VxRay>::value);
}

TEST(RayReferenceTest, CopiesAndAssignmentsAreIndependentAndEqualityChecksAllComponents) {
    VxRay ray(VxVector(1,2,3),VxVector(4,5,6),nullptr),copy=ray,assigned;assigned=ray;EXPECT_TRUE(ray==copy);EXPECT_TRUE(ray==assigned);
    for (int n=0;n<6;++n) { copy=ray;(n<3?copy.m_Origin[n]:copy.m_Direction[n-3])+=1;EXPECT_FALSE(ray==copy); }
    copy.m_Origin.x=9;EXPECT_EQ(ray.m_Origin.x,1);assigned.m_Direction.z=10;EXPECT_EQ(ray.m_Direction.z,6);
    VxRay zero,negative;negative.m_Origin.x=-0.0f;negative.m_Direction.z=-0.0f;EXPECT_TRUE(zero==negative);
    copy=ray;copy.m_Direction.x=std::numeric_limits<float>::quiet_NaN();EXPECT_FALSE(copy==copy);
}

TEST(RayReferenceTest, TransformMatchesEightKnownAffineMapsIncludingInPlaceAndSingularMaps) {
    const RaySamples r=RayTransformDataset();EXPECT_EQ(r.samples,128);RecordRay(r);
}

TEST(RayReferenceTest, InterpolationAllowsNegativeAndBeyondEndParametersAndOutputAliases) {
    const RaySamples r=RayInterpolationDataset();EXPECT_EQ(r.samples,168);RecordRay(r);
}

TEST(RayReferenceTest, UnitLineDistancesMatchIndependentAxisGeometry) {
    const RaySamples r=RayUnitDistanceDataset();EXPECT_EQ(r.samples,20250);RecordRay(r);
}

TEST(RayReferenceTest, NonUnitDirectionStillMeasuresTheInfiniteLineBehindTheOrigin) {
    VxRay ray(VxVector(0,0,0),VxVector(2,0,0),nullptr);const VxVector point(-3,4,0);
    EXPECT_FLOAT_EQ(ray.SquareDistance(point),16);EXPECT_FLOAT_EQ(ray.Distance(point),4);
}

TEST(RayReferenceTest, ExactlyZeroDirectionMeasuresDistanceToTheOrigin) {
    VxRay ray(VxVector(1,2,3),VxVector(0,0,0),nullptr);const VxVector point(4,6,3);
    EXPECT_FLOAT_EQ(ray.SquareDistance(point),25);EXPECT_FLOAT_EQ(ray.Distance(point),5);
}

TEST(RayReferenceTest, TinyNonZeroDirectionsAreNotCollapsedByAnEpsilonThreshold) {
    const float directions[]={1e-10f,1e-30f};
    for (float d:directions) { VxRay ray(VxVector(0,0,0),VxVector(d,0,0),nullptr);EXPECT_FLOAT_EQ(ray.SquareDistance(VxVector(3,4,0)),16);EXPECT_FLOAT_EQ(ray.Distance(VxVector(3,4,0)),4); }
}

TEST(RayReferenceTest, FarParallelComponentsDoNotCancelThePerpendicularDistance) {
    VxRay ray(VxVector(0,0,0),VxVector(1,0,0),nullptr);const VxVector point(1e8f,3,4);
    EXPECT_FLOAT_EQ(ray.SquareDistance(point),25);EXPECT_FLOAT_EQ(ray.Distance(point),5);
}

TEST(RayReferenceTest, LargeDirectionMagnitudeDoesNotTurnTheDistanceIntoZero) {
    VxRay ray(VxVector(0,0,0),VxVector(1e20f,0,0),nullptr);const VxVector point(3,4,0);
    EXPECT_FLOAT_EQ(ray.SquareDistance(point),16);EXPECT_FLOAT_EQ(ray.Distance(point),4);
}

TEST(RayReferenceTest, ObliqueThreeComponentDirectionsKeepTheirPerpendicularDistanceWhenRescaled) {
    const int scales[]={-120,-60,0,60,120};
    for (int exponent:scales) {
        const float unit=std::ldexp(1.0f,exponent);
        VxRay ray(VxVector(1,2,3),VxVector(3*unit,4*unit,12*unit),nullptr);
        // Displacement is 10*(3,4,12)+(4,-3,0); the latter is perpendicular.
        const VxVector point(35,39,123);
        EXPECT_FLOAT_EQ(ray.SquareDistance(point),25);EXPECT_FLOAT_EQ(ray.Distance(point),5);
    }
}

TEST(RayReferenceTest, FiniteOppositeCoordinatesDoNotOverflowBeforeRemovingParallelDisplacement) {
    VxRay ray(VxVector(FLT_MAX,0,0),VxVector(1,0,0),nullptr);const VxVector point(-FLT_MAX,3,4);
    EXPECT_FLOAT_EQ(ray.SquareDistance(point),25);EXPECT_FLOAT_EQ(ray.Distance(point),5);
}

TEST(RayReferenceTest, DistanceRemainsFiniteWhenItsFloatSquareOverflows) {
    VxRay ray(VxVector(0,0,0),VxVector(1,0,0),nullptr);const VxVector point(0,3e20f,4e20f);
    EXPECT_TRUE(std::isinf(ray.SquareDistance(point)));EXPECT_FLOAT_EQ(ray.Distance(point),5e20f);
}

TEST(RayReferenceTest, DistanceRemainsNonZeroWhenItsFloatSquareUnderflows) {
    VxRay ray(VxVector(0,0,0),VxVector(1,0,0),nullptr);const VxVector point(0,3e-30f,4e-30f);
    EXPECT_EQ(ray.SquareDistance(point),0);EXPECT_FLOAT_EQ(ray.Distance(point),5e-30f);
}

TEST(RayReferenceTest, DirectionRescalingAndPointScalePreserveTheSameLineGeometry) {
    const RaySamples r=RayScaleDistanceDataset();EXPECT_EQ(r.samples,1620);RecordRay(r);
}

#if defined(_WIN32)
struct RayGuardedPage {
    unsigned char *base=nullptr;size_t size=0;
    RayGuardedPage() {
        SYSTEM_INFO info;GetSystemInfo(&info);size=info.dwPageSize;
        base=static_cast<unsigned char *>(VirtualAlloc(nullptr,size*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        if (base) { DWORD old=0;if (!VirtualProtect(base+size,size,PAGE_NOACCESS,&old)) { VirtualFree(base,0,MEM_RELEASE);base=nullptr; } }
    }
    ~RayGuardedPage() { if (base) VirtualFree(base,0,MEM_RELEASE); }
};

TEST(RayReferenceTest, ValidRayVectorAndMatrixObjectsNeedNoBytesBeyondTheirDeclaredFootprints) {
    RayGuardedPage r,d,p,m;ASSERT_NE(r.base,nullptr);ASSERT_NE(d.base,nullptr);ASSERT_NE(p.base,nullptr);ASSERT_NE(m.base,nullptr);
    VxRay *ray=new(r.base+r.size-sizeof(VxRay)) VxRay(VxVector(1,2,3),VxVector(2,0,0),nullptr);
    VxRay *dest=new(d.base+d.size-sizeof(VxRay)) VxRay;
    VxVector *point=new(p.base+p.size-sizeof(VxVector)) VxVector(4,6,3);
    VxMatrix *matrix=new(m.base+m.size-sizeof(VxMatrix)) VxMatrix;std::memcpy(matrix,RayMatrices[1],sizeof(VxMatrix));
    EXPECT_FLOAT_EQ(ray->SquareDistance(*point),16);EXPECT_FLOAT_EQ(ray->Distance(*point),4);
    ray->Transform(*dest,*matrix);EXPECT_EQ(dest->m_Origin,VxVector(12,-5,8));EXPECT_EQ(dest->m_Direction,VxVector(2,0,0));
    ray->Interpolate(*point,2);EXPECT_EQ(*point,VxVector(5,2,3));
}
#endif
#endif
