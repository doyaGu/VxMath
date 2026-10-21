#include <cmath>
#include <cfloat>
#include <cstring>
#include <cstdio>
#include <new>
#ifndef VX_FRUSTUM_BOX_NATIVE_PROBE
#ifndef VX_LIB
#define VX_LIB 1
#endif
#include "../../src/VxIntersect_Frustum.cpp"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef XBOOL (*FBFn)(const VxFrustum &,const VxBbox &,const VxMatrix &);
typedef XBOOL (*FBAabbFn)(const VxFrustum &,const VxBbox &);
struct FBResults { int samples=0,errors=0;unsigned digest=2166136261u; };
static void FBObserve(FBResults &r,int actual,int expected) {
    if (actual!=expected) ++r.errors;unsigned bits=unsigned(actual);
    for (int i=0;i<4;++i) { r.digest=(r.digest^(bits&255u))*16777619u;bits>>=8; }++r.samples;
}
static VxFrustum FBCamera(int frame=0,float shift=0) {
    VxFrustum f;f.GetOrigin()=VxVector(shift,-shift*2,shift*3);f.GetRight()=VxVector(1,0,0);f.GetUp()=VxVector(0,1,0);f.GetDir()=VxVector(0,0,1);
    if (frame==1) { f.GetRight()=VxVector(0,1,0);f.GetUp()=VxVector(-1,0,0); }
    if (frame==2) { f.GetRight()=VxVector(0,0,1);f.GetUp()=VxVector(1,0,0);f.GetDir()=VxVector(0,1,0); }
    f.GetRBound()=2;f.GetUBound()=1;f.GetDMin()=2;f.GetDMax()=10;f.Update();return f;
}
static VxVector FBWorld(const VxFrustum &f,float x,float y,float z) {
    VxVector v;for (int c=0;c<3;++c) v[c]=f.GetOrigin()[c]+x*f.GetRight()[c]+y*f.GetUp()[c]+z*f.GetDir()[c];return v;
}
static const VxPlane &FBPlane(const VxFrustum &f,int i) {
    const VxPlane *planes[]={&f.GetNearPlane(),&f.GetFarPlane(),&f.GetLeftPlane(),&f.GetRightPlane(),&f.GetUpPlane(),&f.GetBottomPlane()};return *planes[i];
}
static void FBBoxVertices(const VxBbox &b,const VxMatrix &m,double vertices[8][3]) {
    for (int v=0;v<8;++v) {
        const double p[]={v&1?b.Max.x:b.Min.x,v&2?b.Max.y:b.Min.y,v&4?b.Max.z:b.Min.z};
        for (int c=0;c<3;++c) vertices[v][c]=m[3][c]+p[0]*m[0][c]+p[1]*m[1][c]+p[2]*m[2][c];
    }
}
static void FBFrustumVertices(const VxFrustum &f,double vertices[8][3]) {
    const int signs[4][2]={{-1,-1},{-1,1},{1,1},{1,-1}};
    for (int v=0;v<8;++v) {
        const double ratio=v<4?1:double(f.GetDMax())/f.GetDMin();
        for (int c=0;c<3;++c) vertices[v][c]=f.GetOrigin()[c]+ratio*(double(f.GetDir()[c])*f.GetDMin()+signs[v%4][0]*double(f.GetRight()[c])*f.GetRBound()+signs[v%4][1]*double(f.GetUp()[c])*f.GetUBound());
    }
}
static bool FBIntervalsOverlap(const double a[8][3],const double b[8][3],const VxVector &axis) {
    double aLow=1e300,aHigh=-1e300,bLow=1e300,bHigh=-1e300;
    for (int v=0;v<8;++v) {
        double av=0,bv=0;for (int c=0;c<3;++c) { av+=a[v][c]*axis[c];bv+=b[v][c]*axis[c]; }
        if (av<aLow) aLow=av;if (av>aHigh) aHigh=av;if (bv<bLow) bLow=bv;if (bv>bHigh) bHigh=bv;
    }
    return aLow<=bHigh&&bLow<=aHigh;
}
static bool FBObbOracle(const VxFrustum &f,const VxBbox &b,const VxMatrix &m) {
    double box[8][3],frustum[8][3];FBBoxVertices(b,m,box);FBFrustumVertices(f,frustum);
    // Project all corners, rather than copying the production center/radius
    // formula. The original contract tests eight candidate directions only.
    const VxVector axes[]={VxVector(m[0].x,m[0].y,m[0].z),VxVector(m[1].x,m[1].y,m[1].z),VxVector(m[2].x,m[2].y,m[2].z),f.GetFarPlane().m_Normal,f.GetBottomPlane().m_Normal,f.GetUpPlane().m_Normal,f.GetLeftPlane().m_Normal,f.GetRightPlane().m_Normal};
    for (const VxVector &axis:axes) if (!FBIntervalsOverlap(box,frustum,axis)) return false;return true;
}
static bool FBLegacyOracle(const VxFrustum &f,const VxBbox &b,const VxMatrix &m) {
    double vertices[8][3];FBBoxVertices(b,m,vertices);
    for (int p=0;p<6;++p) {
        const VxPlane &plane=FBPlane(f,p);double low=1e300,high=-1e300;
        for (int v=0;v<8;++v) {
            double distance=plane.m_D;for (int c=0;c<3;++c) distance+=vertices[v][c]*plane.m_Normal[c];
            if (distance<low) low=distance;if (distance>high) high=distance;
        }
        if (low>0) return false;if (high>=0) return true;
    }
    return true;
}
static FBResults FBGeometry(FBFn fn,bool legacy) {
    FBResults r;const VxVector center(0.25f,-0.5f,0.75f);const VxVector half(0.375f,0.625f,0.875f);const VxBbox b(center-half,center+half);
    for (int frame=0;frame<3;++frame) for (int shift=-2;shift<=2;shift+=2) for (int map=0;map<3;++map) {
        const VxFrustum f=FBCamera(frame,float(shift));VxMatrix m;m.SetIdentity();
        if (map==1) { m[0]=VxVector(0.6f,0.8f,0);m[1]=VxVector(-0.8f,0.6f,0);m[2]=VxVector(0,0,1); }
        if (map==2) { m[0]=VxVector(0,-2,0);m[1]=VxVector(0.5f,0,0);m[2]=VxVector(0,0,4); }
        for (int x=-18;x<=18;x+=9) for (int y=-8;y<=8;y+=4) for (int z=-1;z<=15;z+=4) {
            const VxVector target=FBWorld(f,float(x),float(y),float(z));
            for (int c=0;c<3;++c) m[3][c]=target[c]-center.x*m[0][c]-center.y*m[1][c]-center.z*m[2][c];
            const bool expected=legacy?FBLegacyOracle(f,b,m):FBObbOracle(f,b,m);const int actual=fn(f,b,m);
            if (actual!=int(expected)&&r.errors<8) std::fprintf(stderr,"geometry legacy=%d frame=%d shift=%d map=%d center=(%d,%d,%d): got%d expected%d\n",int(legacy),frame,shift,map,x,y,z,actual,int(expected));
            FBObserve(r,actual,expected);
        }
    }
    return r;
}
static FBResults FBAabbGeometry(FBAabbFn fn) {
    FBResults r;VxMatrix m;m.SetIdentity();
    for (int frame=0;frame<3;++frame) for (int shift=-2;shift<=2;shift+=2) {
        const VxFrustum f=FBCamera(frame,float(shift));
        for (int x=-18;x<=18;x+=9) for (int y=-8;y<=8;y+=4) for (int z=-1;z<=15;z+=4) {
            const VxVector center=FBWorld(f,float(x),float(y),float(z));const VxVector half(0.375f,0.625f,0.875f);const VxBbox b(center-half,center+half);
            FBObserve(r,fn(f,b),FBObbOracle(f,b,m));
        }
    }
    return r;
}
static FBResults FBScales(FBFn fn,bool legacy) {
    FBResults r;const int powers[]={-90,-60,-30,-10,0,10,30,60,90};const VxFrustum f=FBCamera();
    for (int power:powers) for (int sign=-1;sign<=1;sign+=2) for (int x=-16;x<=16;x+=8) for (int z=-1;z<=19;z+=4) {
        const float unit=std::ldexp(1.0f,power);const VxVector half(0.5f/unit,0.75f/unit,1.0f/unit);const VxBbox b(-half,half);VxMatrix m;m.SetIdentity();
        m[0]=VxVector(sign*unit,0,0);m[1]=VxVector(0,unit,0);m[2]=VxVector(0,0,unit);m[3]=VxVector(float(x),0,float(z));
        FBObserve(r,fn(f,b,m),legacy?FBLegacyOracle(f,b,m):FBObbOracle(f,b,m));
    }
    return r;
}
static FBResults FBWide(FBFn fn,bool legacy) {
    FBResults r;const VxFrustum f=FBCamera();const VxBbox b(VxVector(-FLT_MAX,-FLT_MAX,-FLT_MAX),VxVector(FLT_MAX,FLT_MAX,FLT_MAX));
    for (int z:{-5,1,5,20}) {
        VxMatrix m;m.SetIdentity();const float unit=std::ldexp(1.0f,-127);m[0]*=unit;m[1]*=unit;m[2]*=unit;m[3]=VxVector(0,0,float(z));
        FBObserve(r,fn(f,b,m),legacy?FBLegacyOracle(f,b,m):FBObbOracle(f,b,m));
    }
    const VxVector p(FLT_MAX,FLT_MAX,FLT_MAX);const VxBbox point(p,p);VxMatrix id;id.SetIdentity();FBObserve(r,fn(f,point,id),0);return r;
}
static FBResults FBAabbWide(FBAabbFn fn) {
    FBResults r;const VxFrustum f=FBCamera();const VxVector p(FLT_MAX,FLT_MAX,FLT_MAX);FBObserve(r,fn(f,VxBbox(p,p)),0);return r;
}
static FBResults FBAabbContacts(FBAabbFn fn) {
    FBResults r;
    for (int shift=-2;shift<=2;shift+=2) for (int side=-1;side<=1;side+=2) {
        const VxFrustum f=FBCamera(0,float(shift));const VxVector center=FBWorld(f,0,float(side*4),3),half(0.3125f,0.75f,3.5f);const VxBbox b(center-half,center+half);
        FBObserve(r,fn(f,b),1);
    }
    return r;
}
static FBResults FBLegacySequences(FBFn fn) {
    FBResults r;const VxBbox b(VxVector(0,0,0),VxVector(0,0,0));VxMatrix m;m.SetIdentity();
    for (int code=0;code<729;++code) {
        VxFrustum f;int digits=code;bool expected=true,resolved=false;
        for (int p=0;p<6;++p) {
            const int value=digits%3-1;digits/=3;const_cast<VxPlane &>(FBPlane(f,p))=VxPlane(VxVector(0,0,0),float(value));
            if (!resolved&&value>=0) { expected=value==0;resolved=true; }
        }
        FBObserve(r,fn(f,b,m),expected);
    }
    return r;
}
#ifndef VX_FRUSTUM_BOX_NATIVE_PROBE
static void FBCheck(FBResults r) {
    EXPECT_EQ(r.errors,0);::testing::Test::RecordProperty("samples",r.samples);::testing::Test::RecordProperty("digest",std::to_string(r.digest));
#if defined(VX_SIMD_SSE2)
    ::testing::Test::RecordProperty("compiled_sse2",1);
#else
    ::testing::Test::RecordProperty("compiled_sse2",0);
#endif
}
TEST(FrustumBoxReference, ScalarObbMatchesIndependentEightCornerProjections) { FBCheck(FBGeometry(FrustumOBBScalarCore,false)); }
TEST(FrustumBoxReference, ScalarLegacyMatchesOrderedPlaneIntervals) { FBCheck(FBGeometry(FrustumBoxScalarCore,true)); }
TEST(FrustumBoxReference, ScalarObbPreservesEquivalentWorldBoxesAcrossScales) { FBCheck(FBScales(FrustumOBBScalarCore,false)); }
TEST(FrustumBoxReference, ScalarLegacyPreservesEquivalentWorldBoxesAcrossScales) { FBCheck(FBScales(FrustumBoxScalarCore,true)); }
TEST(FrustumBoxReference, ScalarLegacyChecksAll729OrderedPlaneSequences) { FBCheck(FBLegacySequences(FrustumBoxScalarCore)); }
TEST(FrustumBoxReference, ScalarObbHandlesRepresentableWideHalfsizeAndPointCenters) { FBCheck(FBWide(FrustumOBBScalarCore,false)); }
TEST(FrustumBoxReference, ScalarLegacyHandlesRepresentableWideHalfsizeAndPointCenters) { FBCheck(FBWide(FrustumBoxScalarCore,true)); }
TEST(FrustumBoxReference, PublicAabbMatchesEightCornerProjections) { VxIntersectFrustumDispatchRebuild(false);FBCheck(FBAabbGeometry(VxIntersect::FrustumAABB)); }
TEST(FrustumBoxReference, PublicAabbRejectsFiniteExtremePoint) { VxIntersectFrustumDispatchRebuild(false);FBCheck(FBAabbWide(VxIntersect::FrustumAABB)); }
TEST(FrustumBoxReference, PublicAabbRetainsClosedCornerContacts) { VxIntersectFrustumDispatchRebuild(false);FBCheck(FBAabbContacts(VxIntersect::FrustumAABB)); }
TEST(FrustumBoxReference, LegacyEarlyResolutionRetainsTheOutputWhenUnresolved) {
    for (float value:{-3.0f,-1.0f,-0.0f,0.0f,1.0f,3.0f}) {
        XBOOL result=7;const XBOOL done=VxFrustumBoxResolveEarlyPlane(value,result);EXPECT_EQ(done,value>=0);EXPECT_EQ(result,value<0?7:value==0?1:0);
    }
}
TEST(FrustumBoxReference, LegacyAndObbAreDifferentOriginalOperations) {
    const VxFrustum f=FBCamera();VxMatrix m;m.SetIdentity();const VxVector p(100,0,2);const VxBbox b(p,p);
    VxIntersectFrustumDispatchRebuild(false);EXPECT_EQ(VxIntersect::FrustumBox(f,b,m),1);EXPECT_EQ(VxIntersect::FrustumOBB(f,b,m),0);
}
static FBResults FBAxisCases(bool simd) {
    FBResults r;const VxFrustum f=FBCamera();const VxVector axes[]={VxVector(1,0,0),VxVector(-1,0,0),VxVector(0,1,0),VxVector(0,-1,0),VxVector(0,0,1),VxVector(0,0,-1),VxVector(1,2,3),VxVector(-2,1,-3)};
    VxMatrix m;m.SetIdentity();double frustum[8][3];FBFrustumVertices(f,frustum);
    for (const VxVector &axis:axes) for (int x=-14;x<=14;x+=7) for (int y=-8;y<=8;y+=4) for (int z=1;z<=13;z+=3) {
        const VxVector center{float(x),float(y),float(z)};const VxVector half(1,2,3);const VxBbox b(center-half,center+half);double vertices[8][3];FBBoxVertices(b,m,vertices);int value;
#if defined(VX_SIMD_SSE)
        if (simd) value=VxSIMDFrustumOBBAxisTest(VxSIMDLoadVector3(axis),VxSIMDLoadVector3(center),VxSIMDLoadVector3(VxVector(1,0,0)),VxSIMDLoadVector3(VxVector(0,1,0)),VxSIMDLoadVector3(VxVector(0,0,1)),1,2,3,VxSIMDLoadVector3(f.GetUp()),VxSIMDLoadVector3(f.GetRight()),VxSIMDLoadVector3(f.GetDir()),1,2,2,5);
        else
#endif
        value=VxFrustumOBBAxisTest(axis,center,VxVector(1,0,0),VxVector(0,1,0),VxVector(0,0,1),half,f.GetUp(),f.GetRight(),f.GetDir(),1,2,2,5);
        FBObserve(r,value,FBIntervalsOverlap(vertices,frustum,axis));
    }
    return r;
}
TEST(FrustumBoxReference, ScalarAxisHelperMatchesCornerIntervals) { FBCheck(FBAxisCases(false)); }
#if defined(VX_SIMD_SSE)
TEST(FrustumBoxReference, SimdObbMatchesIndependentEightCornerProjections) { FBCheck(FBGeometry(FrustumOBBSIMDCore,false)); }
TEST(FrustumBoxReference, SimdLegacyMatchesOrderedPlaneIntervals) { FBCheck(FBGeometry(FrustumBoxSIMDCore,true)); }
TEST(FrustumBoxReference, SimdObbPreservesEquivalentWorldBoxesAcrossScales) { FBCheck(FBScales(FrustumOBBSIMDCore,false)); }
TEST(FrustumBoxReference, SimdLegacyChecksAll729OrderedPlaneSequences) { FBCheck(FBLegacySequences(FrustumBoxSIMDCore)); }
TEST(FrustumBoxReference, SimdObbHandlesRepresentableWideHalfsizeAndPointCenters) { FBCheck(FBWide(FrustumOBBSIMDCore,false)); }
TEST(FrustumBoxReference, SimdLegacyHandlesRepresentableWideHalfsizeAndPointCenters) { FBCheck(FBWide(FrustumBoxSIMDCore,true)); }
TEST(FrustumBoxReference, SimdAxisAndDotHelpersUseThreeComponents) {
    FBCheck(FBAxisCases(true));EXPECT_FLOAT_EQ(VxSIMDDot3Scalar(_mm_setr_ps(1,2,3,1000),_mm_setr_ps(4,-5,6,-500)),12);
}
TEST(FrustumBoxReference, PublicSimdSlotsMatchBothOriginalContracts) {
    VxIntersectFrustumDispatchRebuild(true);FBCheck(FBGeometry(VxIntersect::FrustumBox,true));EXPECT_EQ(FBGeometry(VxIntersect::FrustumOBB,false).errors,0);VxIntersectFrustumDispatchRebuild(false);
}
#endif
#if defined(_WIN32)
TEST(FrustumBoxReference, ActualCurrentDllObbAndLegacyMatchTheirOracles) {
    HMODULE dll=GetModuleHandleA("VxMath.dll");ASSERT_NE(dll,nullptr);
#if defined(_WIN64)
    const char *obbName="?FrustumOBB@VxIntersect@@SAHAEBVVxFrustum@@AEBUVxBbox@@AEBVVxMatrix@@@Z",*boxName="?FrustumBox@VxIntersect@@SAHAEBVVxFrustum@@AEBUVxBbox@@AEBVVxMatrix@@@Z";
#else
    const char *obbName="?FrustumOBB@VxIntersect@@SAHABVVxFrustum@@ABUVxBbox@@ABVVxMatrix@@@Z",*boxName="?FrustumBox@VxIntersect@@SAHABVVxFrustum@@ABUVxBbox@@ABVVxMatrix@@@Z";
#endif
    const FBFn obb=reinterpret_cast<FBFn>(GetProcAddress(dll,obbName)),box=reinterpret_cast<FBFn>(GetProcAddress(dll,boxName));ASSERT_NE(obb,nullptr);ASSERT_NE(box,nullptr);
    EXPECT_EQ(FBGeometry(obb,false).errors,0);FBCheck(FBGeometry(box,true));EXPECT_EQ(FBScales(obb,false).errors,0);EXPECT_EQ(FBWide(obb,false).errors,0);EXPECT_EQ(FBWide(box,true).errors,0);
}
TEST(FrustumBoxReference, ExactReadonlyFrustumBoxAndMatrixFootprints) {
    SYSTEM_INFO info;GetSystemInfo(&info);const std::size_t page=info.dwPageSize;unsigned char *regions[3]={};
    for (int i=0;i<3;++i) { regions[i]=static_cast<unsigned char *>(VirtualAlloc(nullptr,page*2,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));ASSERT_NE(regions[i],nullptr);DWORD old;ASSERT_NE(VirtualProtect(regions[i]+page,page,PAGE_NOACCESS,&old),0); }
    VxFrustum *f=new(regions[0]+page-sizeof(VxFrustum)) VxFrustum;*f=FBCamera();VxBbox *b=new(regions[1]+page-sizeof(VxBbox)) VxBbox(VxVector(-0.5f,-0.5f,4),VxVector(0.5f,0.5f,6));VxMatrix *m=new(regions[2]+page-sizeof(VxMatrix)) VxMatrix;m->SetIdentity();
    DWORD old;for (unsigned char *region:regions) ASSERT_NE(VirtualProtect(region,page,PAGE_READONLY,&old),0);
    EXPECT_EQ(FrustumOBBScalarCore(*f,*b,*m),1);EXPECT_EQ(FrustumBoxScalarCore(*f,*b,*m),1);EXPECT_EQ(VxIntersect::FrustumAABB(*f,*b),1);
#if defined(VX_SIMD_SSE)
    EXPECT_EQ(FrustumOBBSIMDCore(*f,*b,*m),1);EXPECT_EQ(FrustumBoxSIMDCore(*f,*b,*m),1);
#endif
    for (unsigned char *region:regions) EXPECT_NE(VirtualFree(region,0,MEM_RELEASE),0);
}
#endif
#endif
