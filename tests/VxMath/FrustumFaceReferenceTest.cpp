#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>
#ifndef VX_FRUSTUM_FACE_NATIVE_PROBE
// Compile the production kernels in this consumer, including an independent
// VX_SIMD_NONE build. A DLL mode switch cannot select this TU's private table.
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

typedef XBOOL (*FFaceFn)(const VxFrustum &,const VxVector &,const VxVector &,const VxVector &);
struct FFaceSamples { int samples=0,errors=0; unsigned digest=2166136261u; };
static void FFaceObserve(FFaceSamples &s,int value,int expected) {
    if (value!=expected) ++s.errors;
    unsigned bits=unsigned(value);
    for (int i=0;i<4;++i) { s.digest=(s.digest^(bits&255u))*16777619u;bits>>=8; }
    ++s.samples;
}
static VxPlane &FFacePlane(VxFrustum &f,int i) {
    const VxPlane *p[]={&f.GetNearPlane(),&f.GetFarPlane(),&f.GetLeftPlane(),&f.GetRightPlane(),&f.GetBottomPlane(),&f.GetUpPlane()};
    return const_cast<VxPlane &>(*p[i]);
}
static VxFrustum FFaceCamera(int frame=0,float shift=0) {
    VxFrustum f; f.GetOrigin()=VxVector(shift,-2*shift,3*shift);
    f.GetRight()=VxVector(1,0,0); f.GetUp()=VxVector(0,1,0); f.GetDir()=VxVector(0,0,1);
    if (frame==1) { f.GetRight()=VxVector(0,1,0);f.GetUp()=VxVector(-1,0,0); }
    if (frame==2) { f.GetRight()=VxVector(0,0,1);f.GetUp()=VxVector(1,0,0);f.GetDir()=VxVector(0,1,0); }
    f.GetDMin()=2;f.GetDMax()=10;f.GetRBound()=2;f.GetUBound()=1;f.Update();return f;
}
static VxVector FFaceWorld(const VxFrustum &f,const VxVector &p) {
    VxVector out;
    for (int c=0;c<3;++c) out[c]=f.GetOrigin()[c]+p.x*f.GetRight()[c]+p.y*f.GetUp()[c]+p.z*f.GetDir()[c];
    return out;
}
static unsigned FFaceOutsideMask(const VxVector &p) {
    unsigned mask=0;
    if (p.z<2) mask|=1; if (p.z>10) mask|=2;
    if (p.x < -p.z) mask|=4; if (p.x > p.z) mask|=8;
    if (2*p.y < -p.z) mask|=16; if (2*p.y > p.z) mask|=32;
    return mask;
}
static FFaceSamples FFaceGeometryDataset(FFaceFn fn) {
    FFaceSamples s;
    const VxVector p[]={VxVector(0,0,1),VxVector(0,0,3),VxVector(0,0,5),VxVector(0,0,9),VxVector(0,0,11),
        VxVector(-8,0,3),VxVector(8,0,3),VxVector(0,-4,3),VxVector(0,4,3),VxVector(-4,0,9),VxVector(4,0,9),
        VxVector(-8,-8,5),VxVector(-8,8,5),VxVector(8,-8,5),VxVector(8,8,5)};
    for (int frame=0;frame<3;++frame) for (int shift=-2;shift<=2;shift+=2) {
        const VxFrustum f=FFaceCamera(frame,float(shift));
        VxVector world[15]; for (int i=0;i<15;++i) world[i]=FFaceWorld(f,p[i]);
        for (int a=0;a<15;++a) for (int b=0;b<15;++b) for (int c=0;c<15;++c) {
            const int expected=(FFaceOutsideMask(p[a])&FFaceOutsideMask(p[b])&FFaceOutsideMask(p[c]))==0;
            FFaceObserve(s,fn(f,world[a],world[b],world[c]),expected);
        }
    }
    return s;
}
static FFaceSamples FFacePlaneMatrix(FFaceFn fn) {
    FFaceSamples s;const int values[]={-3,-1,0,1,2,3,4};const int powers[]={-90,-60,-30,-10,0,10,30,60,90};
    for (int plane=0;plane<6;++plane) for (int power:powers) {
        VxFrustum f;
        // Isolate each cached plane, including the two non-inlined original
        // tail tests. These are predicate fixtures, not new camera geometry.
        for (int i=0;i<6;++i) FFacePlane(f,i)=VxPlane(VxVector(0,0,0),-1);
        const float scale=std::ldexp(1.0f,power);
        FFacePlane(f,plane)=VxPlane(VxVector(scale,0,0),-2*scale);
        for (int a:values) for (int b:values) for (int c:values) {
            const VxVector p0(float(a+2),7,-5),p1(float(b+2),-3,11),p2(float(c+2),13,2);
            FFaceObserve(s,fn(f,p0,p1,p2),!(a>0&&b>0&&c>0));
        }
    }
    return s;
}
static FFaceSamples FFaceSpecialDataset(FFaceFn fn) {
    FFaceSamples s;const VxFrustum f=FFaceCamera();
    const VxVector corners[]={VxVector(-4,0,4),VxVector(4,0,4),VxVector(0,-2,4),VxVector(0,2,4),VxVector(0,0,2),VxVector(0,0,10)};
    for (const VxVector &p:corners) FFaceObserve(s,fn(f,p,p,p),1);
    const VxVector points[]={VxVector(0,0,4),VxVector(0,0,1),VxVector(0,0,11),VxVector(8,0,4),VxVector(0,-4,4)};
    for (const VxVector &p:points) FFaceObserve(s,fn(f,p,p,p),FFaceOutsideMask(p)==0);
    const VxVector a(6,2,5),b(4,4,5),c(6,4,5);
    // All triangle points have x+y>=8, whereas the z=5 frustum rectangle
    // has x+y<=7.5. They are disjoint, but no one frustum plane rejects all
    // three vertices. The original conservative visibility test returns true.
    const VxVector *p[]={&a,&b,&c};
    for (int first=0;first<3;++first) for (int second=0;second<3;++second) if (first!=second) {
        FFaceObserve(s,fn(f,*p[first],*p[second],*p[3-first-second]),1);
    }
    VxFrustum cached=f;cached.GetOrigin()=VxVector(100,200,300);cached.GetDir()=VxVector(-1,0,0);cached.GetDMin()=20;
    const VxVector inside(0,0,4);FFaceObserve(s,fn(cached,inside,inside,inside),1);
    FFacePlane(cached,5)=VxPlane(VxVector(0,0,0),1);FFaceObserve(s,fn(cached,inside,inside,inside),0);
    return s;
}
typedef float (*FFacePlaneFn)(const VxPlane &,const VxVector &,const VxVector &,const VxVector &);
static FFaceSamples FFaceHelperDataset(FFacePlaneFn fn) {
    FFaceSamples s;const int values[]={-3,-1,0,1,2,3,4};const int powers[]={-90,-60,-30,-10,0,10,30,60,90};
    for (int power:powers) {
        const float scale=std::ldexp(1.0f,power);const VxPlane plane(VxVector(scale,0,0),-2*scale);
        for (int a:values) for (int b:values) for (int c:values) {
            int expected=0;
            if (a>0&&b>0&&c>0) { expected=a; if (b<expected) expected=b;if (c<expected) expected=c; }
            if (a<0&&b<0&&c<0) { expected=a; if (b>expected) expected=b;if (c>expected) expected=c; }
            const float result=fn(plane,VxVector(float(a+2),7,-5),VxVector(float(b+2),-3,11),VxVector(float(c+2),13,2));
            // All distances are exactly representable binary multiples.
            const float target=expected*scale;
            if (result!=target) ++s.errors;
            unsigned bits=0;if (result!=0) std::memcpy(&bits,&result,4);
            for (int i=0;i<4;++i) { s.digest=(s.digest^(bits&255u))*16777619u;bits>>=8; }++s.samples;
        }
    }
    return s;
}
#ifndef VX_FRUSTUM_FACE_NATIVE_PROBE
static void FFaceCheck(FFaceSamples s) {
    EXPECT_EQ(s.errors,0);::testing::Test::RecordProperty("samples",s.samples);::testing::Test::RecordProperty("digest",std::to_string(s.digest));
#if defined(VX_SIMD_SSE2)
    ::testing::Test::RecordProperty("compiled_sse2",1);
#else
    ::testing::Test::RecordProperty("compiled_sse2",0);
#endif
}
TEST(FrustumFaceReference, ScalarCoreMatchesIndependentLocalHalfspaces) { FFaceCheck(FFaceGeometryDataset(FrustumFaceScalarCore)); }
TEST(FrustumFaceReference, ScalarCoreChecksEveryCachedPlaneAndAllOrderedSigns) { FFaceCheck(FFacePlaneMatrix(FrustumFaceScalarCore)); }
TEST(FrustumFaceReference, ScalarCorePreservesBoundaryDegenerateAndConservativeCases) { FFaceCheck(FFaceSpecialDataset(FrustumFaceScalarCore)); }
TEST(FrustumFaceReference, PublicScalarSlotMatchesGeometry) { VxIntersectFrustumDispatchRebuild(false);FFaceCheck(FFaceGeometryDataset(VxIntersect::FrustumFace)); }
TEST(FrustumFaceReference, PublicScalarSlotChecksEveryPlane) { VxIntersectFrustumDispatchRebuild(false);FFaceCheck(FFacePlaneMatrix(VxIntersect::FrustumFace)); }
#if defined(VX_SIMD_SSE)
TEST(FrustumFaceReference, SimdCoreMatchesIndependentLocalHalfspaces) { FFaceCheck(FFaceGeometryDataset(FrustumFaceSIMDCore)); }
TEST(FrustumFaceReference, SimdCoreChecksEveryCachedPlaneAndAllOrderedSigns) { FFaceCheck(FFacePlaneMatrix(FrustumFaceSIMDCore)); }
TEST(FrustumFaceReference, SimdCorePreservesBoundaryDegenerateAndConservativeCases) { FFaceCheck(FFaceSpecialDataset(FrustumFaceSIMDCore)); }
TEST(FrustumFaceReference, SimdPlaneHelperReturnsNearestSignedDistance) { FFaceCheck(FFaceHelperDataset(VxSIMDPlaneClassifyFace)); }
TEST(FrustumFaceReference, PublicSimdSlotAndRepeatedTransitionsMatchGeometry) {
    for (int i=0;i<8;++i) { VxIntersectFrustumDispatchRebuild(i%2!=0);FFaceCheck(FFaceSpecialDataset(VxIntersect::FrustumFace)); }
    FFaceCheck(FFaceGeometryDataset(VxIntersect::FrustumFace));VxIntersectFrustumDispatchRebuild(false);
}
#endif
#if defined(_WIN32)
TEST(FrustumFaceReference, CurrentDllPublicEntryMatchesTheSameIndependentOracle) {
    HMODULE dll=GetModuleHandleA("VxMath.dll");ASSERT_NE(dll,nullptr);
#if defined(_WIN64)
    const char *symbol="?FrustumFace@VxIntersect@@SAHAEBVVxFrustum@@AEBUVxVector@@11@Z";
#else
    const char *symbol="?FrustumFace@VxIntersect@@SAHABVVxFrustum@@ABUVxVector@@11@Z";
#endif
    FFaceFn fn=reinterpret_cast<FFaceFn>(GetProcAddress(dll,symbol));ASSERT_NE(fn,nullptr);FFaceCheck(FFaceGeometryDataset(fn));
}
TEST(FrustumFaceReference, ExactReadOnlyFootprintsAtGuardPages) {
    SYSTEM_INFO info;GetSystemInfo(&info);const std::size_t page=info.dwPageSize;unsigned char *regions[4]={};
    for (int i=0;i<4;++i) { regions[i]=static_cast<unsigned char *>(VirtualAlloc(nullptr,page*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));ASSERT_NE(regions[i],nullptr);DWORD old;ASSERT_NE(VirtualProtect(regions[i]+page,page,PAGE_NOACCESS,&old),0); }
    VxFrustum *f=new(regions[0]+page-sizeof(VxFrustum)) VxFrustum;*f=FFaceCamera();
    VxVector *p[3];for (int i=0;i<3;++i) p[i]=new(regions[i+1]+page-sizeof(VxVector)) VxVector(float(i-1),0,4);
    DWORD old;for (unsigned char *region:regions) ASSERT_NE(VirtualProtect(region,page,PAGE_READONLY,&old),0);
    EXPECT_EQ(FrustumFaceScalarCore(*f,*p[0],*p[1],*p[2]),1);
#if defined(VX_SIMD_SSE)
    EXPECT_EQ(FrustumFaceSIMDCore(*f,*p[0],*p[1],*p[2]),1);
    for (VxVector *point:p) { float lanes[4];_mm_storeu_ps(lanes,VxSIMDLoadVector3(*point));EXPECT_EQ(lanes[0],point->x);EXPECT_EQ(lanes[1],0);EXPECT_EQ(lanes[2],4);EXPECT_EQ(lanes[3],0); }
#endif
    EXPECT_EQ(VxIntersect::FrustumFace(*f,*p[0],*p[1],*p[2]),1);
    for (unsigned char *region:regions) EXPECT_NE(VirtualFree(region,0,MEM_RELEASE),0);
}
#endif
#endif
