#include <cmath>
#include <cfloat>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <new>
#include <type_traits>
#ifndef VX_OBB_NATIVE_PROBE
#include "VxOBB.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

struct ObbSamples { int samples=0,errors=0; unsigned digest=2166136261u; };
static void ObbObserve(ObbSamples &r,float value,double expected) {
    if (!std::isfinite(value) || std::fabs(double(value)-expected)>2e-6*(1+std::fabs(expected))) ++r.errors;
    unsigned bits=0;if (value!=0) std::memcpy(&bits,&value,4);
    for (int byte=0;byte<4;++byte) { r.digest=(r.digest^(bits&255u))*16777619u;bits>>=8; }
}
static void ObbBasis(VxVector axes[3],int frame) {
    axes[0]=VxVector(1,0,0);axes[1]=VxVector(0,1,0);axes[2]=VxVector(0,0,1);
    if (frame==1) { axes[0]=VxVector(0,1,0);axes[1]=VxVector(-1,0,0); }
    if (frame==2) { axes[0]=VxVector(0,0,1);axes[1]=VxVector(1,0,0);axes[2]=VxVector(0,1,0); }
    if (frame==3) axes[0]=VxVector(-1,0,0);
}
static ObbSamples ObbCreationDataset() {
    ObbSamples r;const VxVector halfsizes[]={VxVector(1,2,3),VxVector(0,1,2),VxVector(0.5f,1.5f,2.5f)};
    const VxVector scales[]={VxVector(1,1,1),VxVector(2,0.5f,4),VxVector(-0.5f,4,2)};
    for (int x=-2;x<=2;x+=2) for (int y=-2;y<=2;y+=2) for (int z=-2;z<=2;z+=2)
    for (const VxVector &half:halfsizes) for (const VxVector &scale:scales) for (int frame=0;frame<4;++frame) for (int shift=-1;shift<=1;++shift) {
        const VxVector localCenter{float(x),float(y),float(z)};const VxBbox box(localCenter-half,localCenter+half);VxVector axes[3];ObbBasis(axes,frame);
        VxMatrix m;m.SetIdentity();for (int i=0;i<3;++i) m[i]=axes[i]*scale[i];m[3]=VxVector(float(shift*2),float(-shift*3),float(shift));
        VxOBB out;out.Create(box,m);
        for (int component=0;component<3;++component) {
            double center=m[3][component];for (int row=0;row<3;++row) center+=double(localCenter[row])*m[row][component];
            ObbObserve(r,out.m_Center[component],center);
            for (int row=0;row<3;++row) ObbObserve(r,out.m_Axis[row][component],axes[row][component]*(scale[row]<0?-1:1));
            ObbObserve(r,out.m_Extents[component],half[component]*std::fabs(scale[component]));
        }
        ++r.samples;
    }
    return r;
}
static ObbSamples ObbScaleDataset() {
    ObbSamples r;const int powers[]={-120,-90,-60,-30,0,30,60,90,120};
    const VxBbox box(VxVector(-0.5f,-1,-2),VxVector(0.5f,1,2));
    for (int exponent:powers) for (int sign=-1;sign<=1;sign+=2) {
        const float unit=std::ldexp(1.0f,exponent);VxMatrix m;m.SetIdentity();
        m[0]=VxVector(sign*3*unit,sign*4*unit,0);m[1]=VxVector(-4*unit,3*unit,0);m[2]=VxVector(0,0,2*unit);
        const VxVector axes[]={VxVector(sign*0.6f,sign*0.8f,0),VxVector(-0.8f,0.6f,0),VxVector(0,0,1)};
        const VxVector expected(2.5f*unit,5*unit,4*unit);VxOBB out(box,m);
        for (int row=0;row<3;++row) {
            for (int c=0;c<3;++c) ObbObserve(r,out.m_Axis[row][c],axes[row][c]);
            // Relative comparison is necessary for nonzero sub-unit extents.
            if (!std::isfinite(out.m_Extents[row]) || std::fabs(double(out.m_Extents[row])/expected[row]-1)>2e-6) ++r.errors;
            ObbObserve(r,out.m_Extents[row],expected[row]);
        }
        ++r.samples;
    }
    return r;
}
static ObbSamples ObbQueryDataset(bool boxes=false) {
    ObbSamples r;const VxVector extents[]={VxVector(1,2,3),VxVector(0,1,2),VxVector(2,1,1)};
    for (int cx=-2;cx<=2;cx+=4) for (int cy=-2;cy<=2;cy+=4) for (int cz=-2;cz<=2;cz+=4)
    for (int frame=0;frame<4;++frame) for (const VxVector &extent:extents) {
        VxOBB obb;obb.m_Center=VxVector(float(cx),float(cy),float(cz));ObbBasis(obb.m_Axis,frame);obb.m_Extents=extent;
        // For these signed permutation frames, the OBB is exactly a known
        // world-axis interval. The oracle does not call projection or Create.
        VxVector worldHalf=extent;if (frame==1) worldHalf=VxVector(extent.y,extent.x,extent.z);if (frame==2) worldHalf=VxVector(extent.y,extent.z,extent.x);
        const int step=boxes?2:1;
        for (int x=-4;x<=4;x+=step) for (int y=-4;y<=4;y+=step) for (int z=-4;z<=4;z+=step) for (int size=0;size<(boxes?3:1);++size) {
            const VxVector point(float(cx+x),float(cy+y),float(cz+z));const float half=size*0.5f;
            const bool inside=std::abs(x)+half<=worldHalf.x && std::abs(y)+half<=worldHalf.y && std::abs(z)+half<=worldHalf.z;
            const VxVector h(half,half,half);const float actual=float(boxes?obb.IsBoxInside(VxBbox(point-h,point+h)):obb.VectorIn(point));
            ObbObserve(r,actual,inside);++r.samples;
        }
    }
    return r;
}
static VxOBB ObbOverflowQueryBox() {
    VxOBB box;box.m_Center=VxVector(-FLT_MAX,-FLT_MAX,-FLT_MAX);box.m_Extents=VxVector(1,1,1);
    box.m_Axis[0]=VxVector(2.0f/3,2.0f/3,-1.0f/3);box.m_Axis[1]=VxVector(-1.0f/3,2.0f/3,2.0f/3);box.m_Axis[2]=VxVector(2.0f/3,-1.0f/3,2.0f/3);
    return box;
}

#ifndef VX_OBB_NATIVE_PROBE
static void RecordObb(const ObbSamples &r) {
    EXPECT_EQ(r.errors,0);char digest[16];std::snprintf(digest,sizeof(digest),"%u",r.digest);testing::Test::RecordProperty("samples",r.samples);testing::Test::RecordProperty("digest",digest);
#if defined(VX_SIMD_SSE2)
    testing::Test::RecordProperty("compiled_sse2",1);
#else
    testing::Test::RecordProperty("compiled_sse2",0);
#endif
}
TEST(ObbReferenceTest, LayoutAndImplicitCopiesRetainFifteenFloatsWithoutSimdPadding) {
    EXPECT_EQ(sizeof(VxOBB),60u);EXPECT_EQ(alignof(VxOBB),4u);EXPECT_EQ(offsetof(VxOBB,m_Center),0u);EXPECT_EQ(offsetof(VxOBB,m_Axis),12u);EXPECT_EQ(offsetof(VxOBB,m_Extents),48u);
    EXPECT_TRUE(std::is_standard_layout<VxOBB>::value);EXPECT_TRUE(std::is_trivially_copyable<VxOBB>::value);EXPECT_TRUE(std::is_trivially_destructible<VxOBB>::value);
    const VxBbox source(1);VxMatrix m;m.SetIdentity();VxOBB box(source,m),copy(box),assigned;assigned=box;assigned.m_Axis[1].x=9;copy.m_Extents.z=99;
    EXPECT_EQ(box.m_Axis[1],VxVector(0,1,0));EXPECT_FLOAT_EQ(box.m_Extents.z,1);
}
TEST(ObbReferenceTest, DefaultConstructorKeepsVectorDefaultsWithoutInventingUnitAxes) {
    const VxOBB box;EXPECT_EQ(box.m_Center,VxVector(0,0,0));EXPECT_EQ(box.m_Extents,VxVector(0,0,0));for (int i=0;i<3;++i) EXPECT_EQ(box.m_Axis[i],VxVector(0,0,0));
}
TEST(ObbReferenceTest, AllMutableAndConstAccessorsReferToTheStoredFields) {
    VxOBB box;const VxOBB &view=box;EXPECT_EQ(&box.GetCenter(),&box.m_Center);EXPECT_EQ(&view.GetCenter(),&box.m_Center);
    EXPECT_EQ(box.GetAxes(),box.m_Axis);EXPECT_EQ(view.GetAxes(),box.m_Axis);EXPECT_EQ(box.GetExtents(),&box.m_Extents.x);EXPECT_EQ(view.GetExtents(),&box.m_Extents.x);
    EXPECT_TRUE((std::is_same<decltype(view.GetAxis(0)),const VxVector &>::value));EXPECT_TRUE((std::is_same<decltype(view.GetExtent(0)),const float &>::value));
    for (int i=0;i<3;++i) { EXPECT_EQ(&box.GetAxis(i),&box.m_Axis[i]);EXPECT_EQ(&view.GetAxis(i),&box.m_Axis[i]);EXPECT_EQ(&box.GetExtent(i),&box.m_Extents[i]);EXPECT_EQ(&view.GetExtent(i),&box.m_Extents[i]);box.GetAxis(i)[i]=float(i+2);box.GetExtent(i)=float(i+4);EXPECT_FLOAT_EQ(view.GetAxes()[i][i],float(i+2));EXPECT_FLOAT_EQ(view.GetExtents()[i],float(i+4)); }
    box.GetCenter()=VxVector(1,2,3);EXPECT_EQ(view.GetCenter(),VxVector(1,2,3));
}
TEST(ObbReferenceTest, CreatePreservesSourceCenterAxisSignsAndScaledHalfLengths) { RecordObb(ObbCreationDataset()); }
TEST(ObbReferenceTest, BoxMatrixConstructorDelegatesToTheSameOriginalGeometry) {
    VxMatrix m;m.SetIdentity();m[0]=VxVector(0,-2,0);m[1]=VxVector(3,0,0);m[2]=VxVector(0,0,0.5f);m[3]=VxVector(10,20,30);
    const VxBbox source(VxVector(1,2,3),VxVector(3,6,11));const VxOBB out(source,m);
    EXPECT_EQ(out.m_Center,VxVector(22,16,33.5f));EXPECT_EQ(out.m_Axis[0],VxVector(0,-1,0));EXPECT_EQ(out.m_Axis[1],VxVector(1,0,0));EXPECT_EQ(out.m_Axis[2],VxVector(0,0,1));EXPECT_EQ(out.m_Extents,VxVector(2,6,2));
}
TEST(ObbReferenceTest, NonzeroMatrixAxesRetainTheirUnitDirectionsAcrossBinaryScales) { RecordObb(ObbScaleDataset()); }
TEST(ObbReferenceTest, AxisLengthMayExceedFloatRangeWhenTheFinalExtentIsRepresentable) {
    VxMatrix m;m.SetIdentity();m[0]=VxVector(FLT_MAX,FLT_MAX,0);m[1]=VxVector(-FLT_MAX,FLT_MAX,0);
    const float half=1e-20f;const VxBbox source(VxVector(-half,-half,-1),VxVector(half,half,1));const VxOBB out(source,m);
    const double diagonal=1/std::sqrt(2.0),extent=std::hypot(double(FLT_MAX),double(FLT_MAX))*half;
    EXPECT_NEAR(out.m_Axis[0].x,diagonal,1e-6);EXPECT_NEAR(out.m_Axis[0].y,diagonal,1e-6);EXPECT_NEAR(out.m_Axis[1].x,-diagonal,1e-6);EXPECT_NEAR(out.m_Axis[1].y,diagonal,1e-6);
    EXPECT_TRUE(std::isfinite(out.m_Extents.x));EXPECT_NEAR(double(out.m_Extents.x)/extent,1,2e-6);
}
TEST(ObbReferenceTest, PointContainmentMatchesKnownClosedWorldIntervals) { RecordObb(ObbQueryDataset()); }
TEST(ObbReferenceTest, WholeAabbContainmentMatchesKnownClosedWorldIntervals) { RecordObb(ObbQueryDataset(true)); }
TEST(ObbReferenceTest, RotatedAabbContainmentChecksTheOtherSixCorners) {
    VxMatrix m;m.SetIdentity();m[0]=VxVector(3,4,0);m[1]=VxVector(-4,3,0);
    const VxOBB box(VxBbox(VxVector(-1,-0.1f,-1),VxVector(1,0.1f,1)),m);const VxBbox candidate(VxVector(-1,-1,0),VxVector(1,1,0));
    EXPECT_TRUE(box.VectorIn(candidate.Min));EXPECT_TRUE(box.VectorIn(candidate.Max));EXPECT_FALSE(box.VectorIn(VxVector(-1,1,0)));EXPECT_FALSE(box.IsBoxInside(candidate));
}
TEST(ObbReferenceTest, FloatProjectionBoundariesStillIncludeThinObliqueBoxCorners) {
    VxMatrix m;m.SetIdentity();m[0]=VxVector(3,4,0);m[1]=VxVector(-4,3,0);
    const VxOBB box(VxBbox(VxVector(-1,0,-1),VxVector(1,0,1)),m);
    for (int sign=-1;sign<=1;sign+=2) for (int z=-1;z<=1;z+=2) EXPECT_TRUE(box.VectorIn(VxVector(float(sign*3),float(sign*4),float(z))));
}
TEST(ObbReferenceTest, OppositeFiniteCoordinatesCannotBecomeNanAndAcceptAFarPoint) {
    const VxOBB box=ObbOverflowQueryBox();EXPECT_FALSE(box.VectorIn(VxVector(FLT_MAX,FLT_MAX,FLT_MAX)));
}
TEST(ObbReferenceTest, WholeAabbContainmentUsesTheCorrectedWideDisplacementQuery) {
    const VxOBB box=ObbOverflowQueryBox();const VxVector p(FLT_MAX,FLT_MAX,FLT_MAX);EXPECT_FALSE(box.IsBoxInside(VxBbox(p,p)));
}
TEST(ObbReferenceTest, WideQueriesCanContainPointsAndRetainTheSmallestOrthogonalDisplacement) {
    VxOBB box;box.m_Center=VxVector(-FLT_MAX,0,0);box.m_Extents=VxVector(FLT_MAX,FLT_MAX,0);
    const float diagonal=float(1/std::sqrt(2.0));box.m_Axis[0]=VxVector(diagonal,diagonal,0);box.m_Axis[1]=VxVector(-diagonal,diagonal,0);box.m_Axis[2]=VxVector(0,0,1);
    EXPECT_TRUE(box.VectorIn(VxVector(FLT_MAX*0.25f,0,0)));
    EXPECT_FALSE(box.VectorIn(VxVector(FLT_MAX*0.25f,0,std::ldexp(1.0f,-149))));
}
TEST(ObbReferenceTest, EqualityComparesAllCenterAxisAndExtentComponentsNumerically) {
    VxMatrix m;m.SetIdentity();const VxOBB box(VxBbox(1),m);EXPECT_TRUE(box==VxOBB(box));
    for (int n=0;n<15;++n) { VxOBB other=box;if (n<3) other.m_Center[n]+=1;else if (n<12) other.m_Axis[(n-3)/3][(n-3)%3]+=1;else other.m_Extents[n-12]+=1;EXPECT_FALSE(box==other); }
    VxOBB zero=box;zero.m_Center.x=-0.0f;EXPECT_TRUE(zero==box);zero.m_Extents.y=std::nanf("");EXPECT_FALSE(zero==zero);
}
#if defined(_WIN32)
TEST(ObbReferenceTest, ObjectBoxPointAndMatrixFootprintsEndAtGuardPages) {
    SYSTEM_INFO system;GetSystemInfo(&system);const SIZE_T page=system.dwPageSize;XBYTE *p=static_cast<XBYTE *>(VirtualAlloc(nullptr,8*page,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));ASSERT_NE(p,nullptr);
    DWORD old=0;for (int i=1;i<8;i+=2) ASSERT_TRUE(VirtualProtect(p+i*page,page,PAGE_NOACCESS,&old));
    VxOBB *obb=new(p+page-sizeof(VxOBB)) VxOBB;VxBbox *box=new(p+3*page-sizeof(VxBbox)) VxBbox(1);VxVector *point=new(p+5*page-sizeof(VxVector)) VxVector(0,0,0);VxMatrix *m=new(p+7*page-sizeof(VxMatrix)) VxMatrix;m->SetIdentity();
    obb->Create(*box,*m);EXPECT_TRUE(obb->VectorIn(*point));EXPECT_TRUE(obb->IsBoxInside(*box));EXPECT_EQ(obb->GetAxis(2),VxVector(0,0,1));EXPECT_FLOAT_EQ(obb->GetExtent(2),1);
    VirtualFree(p,0,MEM_RELEASE);
}
#endif
#endif
