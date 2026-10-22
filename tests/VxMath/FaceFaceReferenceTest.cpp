#include <cmath>
#include <cfloat>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <new>
#include <initializer_list>
#ifndef VX_FACE_FACE_NATIVE_PROBE
#ifndef VX_LIB
#define VX_LIB 1
#endif
#ifndef VX_FACE_FACE_IMPLEMENTATION
#define VX_FACE_FACE_IMPLEMENTATION "../../src/VxIntersect_FaceFace.cpp"
#endif
#include VX_FACE_FACE_IMPLEMENTATION
#include <gtest/gtest.h>
void VxIntersectPlaneDispatchRebuild(bool) { FAIL() << "Unrelated plane rebuild"; }
void VxIntersectSphereDispatchRebuild(bool) { FAIL() << "Unrelated sphere rebuild"; }
void VxIntersectFrustumDispatchRebuild(bool) { FAIL() << "Unrelated frustum rebuild"; }
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef int (*FFFn)(const VxVector &,const VxVector &,const VxVector &,const VxVector &,
                    const VxVector &,const VxVector &,const VxVector &,const VxVector &);
struct FFSamples { int samples=0, errors=0, inputErrors=0; unsigned digest=2166136261u; };
static const int FFPermutations[][3]={{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
static void FFObserve(FFSamples &r,FFFn fn,const VxVector a[3],const VxVector &na,const VxVector b[3],const VxVector &nb,bool expected) {
    const VxVector before[]={a[0],a[1],a[2],na,b[0],b[1],b[2],nb};
    const int value=fn(a[0],a[1],a[2],na,b[0],b[1],b[2],nb);
    const bool changed=std::memcmp(a,before,36)||std::memcmp(&na,before+3,12)||std::memcmp(b,before+4,36)||std::memcmp(&nb,before+7,12);
    ++r.samples;r.errors+=(value!=int(expected))||changed;r.inputErrors+=changed;r.digest=(r.digest^unsigned(value))*16777619u;
}
static VxVector FFMap(float x,float y,float z,int axis,float scale=1,float offset=0) {
    VxVector p; p[axis]=x*scale+offset;p[(axis+1)%3]=y*scale+offset;p[(axis+2)%3]=z*scale+offset;return p;
}
// Unit right triangle against axis-plane sections with literal intervals.
static FFSamples FFSections(FFFn fn) {
    FFSamples r;
    for(int axis=0;axis<3;++axis)for(const auto &ap:FFPermutations)for(const auto &bp:FFPermutations)
        for(int x=-2;x<=5;++x)for(int y=-2;y<=5;++y)for(int sign:{-1,1}) {
            const VxVector a[]={FFMap(0,0,0,axis),FFMap(4,0,0,axis),FFMap(0,4,0,axis)};
            // Intersection with z=0 is [x,x+1] at y; 0<=y<=4 and x<=4-y, x+1>=0.
            const VxVector b[]={FFMap(float(x),float(y),-1,axis),FFMap(float(x),float(y),1,axis),FFMap(float(x+2),float(y),1,axis)};
            const VxVector aa[]={a[ap[0]],a[ap[1]],a[ap[2]]},bb[]={b[bp[0]],b[bp[1]],b[bp[2]]};
            FFObserve(r,fn,aa,FFMap(0,0,float(sign),axis),bb,FFMap(0,float(sign),0,axis),y>=0&&y<=4&&x<=4-y&&x+1>=0);
        }
    return r;
}
static FFSamples FFCoplanar(FFFn fn) {
    FFSamples r;
    for(int axis=0;axis<3;++axis)for(const auto &ap:FFPermutations)for(const auto &bp:FFPermutations)
        for(int x=-3;x<=5;++x)for(int y=-3;y<=5;++y) {
            const VxVector a[]={FFMap(0,0,0,axis),FFMap(4,0,0,axis),FFMap(0,4,0,axis)};
            const VxVector b[]={FFMap(float(x),float(y),0,axis),FFMap(float(x+2),float(y),0,axis),FFMap(float(x),float(y+2),0,axis)};
            const VxVector aa[]={a[ap[0]],a[ap[1]],a[ap[2]]},bb[]={b[bp[0]],b[bp[1]],b[bp[2]]};
            // Minkowski difference: x>=-2,y>=-2,-2<=x+y<=4 plus x<=4,y<=4.
            const bool hit=x>=-2&&y>=-2&&x<=4&&y<=4&&x+y>=-2&&x+y<=4;
            FFObserve(r,fn,aa,FFMap(0,0,1,axis),bb,FFMap(0,0,-1,axis),hit);
        }
    return r;
}
static FFSamples FFScales(FFFn fn) {
    FFSamples r;
    for(int exponent:{-100,-40,-10,0,10,40,100})for(int ne:{-20,0,20,100})for(int axis=0;axis<3;++axis)
        for(int x:{-2,-1,0,3,4,5})for(int y:{0,1,4,5}) {
            const float s=std::ldexp(1.0f,exponent),n=std::ldexp(1.0f,ne-exponent);
            if(!std::isfinite(n)||n==0)continue;
            const VxVector a[]={FFMap(0,0,0,axis,s),FFMap(4,0,0,axis,s),FFMap(0,4,0,axis,s)};
            const VxVector b[]={FFMap(float(x),float(y),-1,axis,s),FFMap(float(x),float(y),1,axis,s),FFMap(float(x+2),float(y),1,axis,s)};
            FFObserve(r,fn,a,FFMap(0,0,n,axis),b,FFMap(0,n,0,axis),y<=4&&x<=4-y&&x+1>=0);
        }
    return r;
}
static FFSamples FFCoplanarScales(FFFn fn) {
    FFSamples r;
    for(int exponent:{-140,-100,-40,0,40,100,120})for(int axis=0;axis<3;++axis)for(int x:{-3,-2,-1,0,2,4,5})for(int y:{-3,-2,-1,0,2,4,5}) {
        const float s=std::ldexp(1.0f,exponent);
        const VxVector a[]={FFMap(0,0,0,axis,s),FFMap(4,0,0,axis,s),FFMap(0,4,0,axis,s)};
        const VxVector b[]={FFMap(float(x),float(y),0,axis,s),FFMap(float(x+2),float(y),0,axis,s),FFMap(float(x),float(y+2),0,axis,s)};
        FFObserve(r,fn,a,FFMap(0,0,1,axis),b,FFMap(0,0,1,axis),x>=-2&&y>=-2&&x<=4&&y<=4&&x+y>=-2&&x+y<=4);
    }
    return r;
}
static FFSamples FFTolerance(FFFn fn) {
    FFSamples r;
    for(int axis=0;axis<3;++axis)for(int sign:{-1,1})for(int n:{1,2,4})for(float gap:{0.0f,std::ldexp(1.0f,-25),std::ldexp(1.0f,-24),std::ldexp(1.0f,-23),std::ldexp(1.0f,-22)}) {
        const VxVector a[]={FFMap(0,0,0,axis),FFMap(4,0,0,axis),FFMap(0,4,0,axis)};
        const VxVector b[]={FFMap(.25f,.25f,gap,axis),FFMap(1,.25f,gap,axis),FFMap(.25f,1,gap,axis)};
        FFObserve(r,fn,a,FFMap(0,0,float(n*sign),axis),b,FFMap(0,0,float(n*sign),axis),gap*n<std::ldexp(1.0f,-23));
    }
    return r;
}
static FFSamples FFTranslated(FFFn fn) {
    FFSamples r;
    // Normal is not normalised, and all vertices are exactly representable.
    for(int axis=0;axis<3;++axis)for(int e:{20,60,100,124})for(int x:{-2,-1,0,3,4,5})for(int y:{0,1,4,5}) {
        const float o=std::ldexp(1.0f,e),s=std::ldexp(1.0f,e-20),n=std::ldexp(1.0f,20-e);
        const VxVector a[]={FFMap(0,0,0,axis,s,o),FFMap(4,0,0,axis,s,o),FFMap(0,4,0,axis,s,o)};
        const VxVector b[]={FFMap(float(x),float(y),-1,axis,s,o),FFMap(float(x),float(y),1,axis,s,o),FFMap(float(x+2),float(y),1,axis,s,o)};
        FFObserve(r,fn,a,FFMap(0,0,n,axis),b,FFMap(0,n,0,axis),y<=4&&x<=4-y&&x+1>=0);
    }
    return r;
}
// Independent separating-axis oracle: projects onto both face normals,
// all nine crossed edges, and in-plane edge normals for coplanar triangles.
// It does not construct plane/triangle intersection intervals.
struct FFDouble { double v[3]; };
static FFDouble FFSub(const VxVector &a,const VxVector &b){return {{double(a.x)-b.x,double(a.y)-b.y,double(a.z)-b.z}};}
static FFDouble FFCross(FFDouble a,FFDouble b){return {{a.v[1]*b.v[2]-a.v[2]*b.v[1],a.v[2]*b.v[0]-a.v[0]*b.v[2],a.v[0]*b.v[1]-a.v[1]*b.v[0]}};}
static bool FFAxisSeparates(const VxVector a[3],const VxVector b[3],FFDouble axis) {
    double lo[2]={0,0},hi[2]={0,0};
    for(int t=0;t<2;++t)for(int i=0;i<3;++i){const VxVector &p=t?b[i]:a[i];double d=0;for(int j=0;j<3;++j)d+=double(p[j])*axis.v[j];if(i==0||d<lo[t])lo[t]=d;if(i==0||d>hi[t])hi[t]=d;}
    return hi[0]<lo[1]||hi[1]<lo[0];
}
static bool FFSat(const VxVector a[3],const VxVector b[3]) {
    FFDouble edges[2][3];for(int i=0;i<3;++i){edges[0][i]=FFSub(a[(i+1)%3],a[i]);edges[1][i]=FFSub(b[(i+1)%3],b[i]);}
    const FFDouble na=FFCross(edges[0][0],edges[0][1]),nb=FFCross(edges[1][0],edges[1][1]);
    if(FFAxisSeparates(a,b,na)||FFAxisSeparates(a,b,nb))return false;
    for(int i=0;i<3;++i)for(int j=0;j<3;++j)if(FFAxisSeparates(a,b,FFCross(edges[0][i],edges[1][j])))return false;
    for(int i=0;i<3;++i)if(FFAxisSeparates(a,b,FFCross(na,edges[0][i]))||FFAxisSeparates(a,b,FFCross(nb,edges[1][i])))return false;
    return true;
}
static unsigned FFNext(unsigned &seed){seed=seed*1664525u+1013904223u;return seed;}
static FFSamples FFRandom(FFFn fn) {
    FFSamples r;unsigned seed=276;
    while(r.samples<4000){VxVector a[3],b[3];for(int i=0;i<3;++i)for(int j=0;j<3;++j){a[i][j]=float(int(FFNext(seed)%17)-8);b[i][j]=float(int(FFNext(seed)%17)-8);}
        if(r.samples&1)for(int i=0;i<3;++i){a[i].z=0;b[i].z=0;}
        const FFDouble na=FFCross(FFSub(a[1],a[0]),FFSub(a[2],a[0])),nb=FFCross(FFSub(b[1],b[0]),FFSub(b[2],b[0]));
        if((na.v[0]==0&&na.v[1]==0&&na.v[2]==0)||(nb.v[0]==0&&nb.v[1]==0&&nb.v[2]==0))continue;
        FFObserve(r,fn,a,VxVector(float(na.v[0]),float(na.v[1]),float(na.v[2])),b,VxVector(float(nb.v[0]),float(nb.v[1]),float(nb.v[2])),FFSat(a,b));
    }return r;
}
#if defined(_WIN32)
static FFSamples FFFootprint(FFFn fn) {
    FFSamples r;SYSTEM_INFO si;GetSystemInfo(&si);const size_t size=si.dwPageSize;
    char *pages=static_cast<char*>(VirtualAlloc(nullptr,size*16,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));if(!pages){r.errors=1;return r;}
    const VxVector values[]={VxVector(0,0,0),VxVector(4,0,0),VxVector(0,4,0),VxVector(0,0,1),VxVector(1,1,-1),VxVector(1,1,1),VxVector(3,1,1),VxVector(0,1,0)};
    VxVector *v[8];DWORD old;bool good=true;for(int i=0;i<8;++i){v[i]=new(pages+size*(2*i+1)-12)VxVector(values[i]);good=VirtualProtect(pages+size*(2*i+1),size,PAGE_NOACCESS,&old)&&good;good=VirtualProtect(pages+size*2*i,size,PAGE_READONLY,&old)&&good;}
    if(good){r.samples=1;r.errors=fn(*v[0],*v[1],*v[2],*v[3],*v[4],*v[5],*v[6],*v[7])!=1;}else r.errors=1;
    VirtualFree(pages,0,MEM_RELEASE);return r;
}
#endif
#ifndef VX_FACE_FACE_NATIVE_PROBE
static void FFCheck(FFSamples r){EXPECT_EQ(r.errors,0)<<"samples="<<r.samples<<" changed inputs="<<r.inputErrors;::testing::Test::RecordProperty("samples",r.samples);::testing::Test::RecordProperty("digest",std::to_string(r.digest));}
#define FF_TESTS(prefix,fn) \
TEST(FaceFaceReference,prefix##PlaneSectionsAndPermutations){FFCheck(FFSections(fn));} \
TEST(FaceFaceReference,prefix##CoplanarGridAndPermutations){FFCheck(FFCoplanar(fn));} \
TEST(FaceFaceReference,prefix##NormalAndGeometryScales){FFCheck(FFScales(fn));} \
TEST(FaceFaceReference,prefix##CoplanarTinyAndLargeCoordinates){FFCheck(FFCoplanarScales(fn));} \
TEST(FaceFaceReference,prefix##StrictEpsilonUsesSuppliedNormal){FFCheck(FFTolerance(fn));} \
TEST(FaceFaceReference,prefix##TranslatedPlaneSections){FFCheck(FFTranslated(fn));} \
TEST(FaceFaceReference,prefix##IndependentSatOracle){FFCheck(FFRandom(fn));}
FF_TESTS(Scalar,FaceFaceScalarCore)
#if defined(VX_SIMD_SSE)
FF_TESTS(Simd,FaceFaceSIMDCore)
#endif
#if defined(_WIN32)
TEST(FaceFaceReference,ScalarReadonlyTwelveByteInputs){FFCheck(FFFootprint(FaceFaceScalarCore));}
#if defined(VX_SIMD_SSE)
TEST(FaceFaceReference,SimdReadonlyTwelveByteInputs){FFCheck(FFFootprint(FaceFaceSIMDCore));}
#endif
#endif
TEST(FaceFaceReference,PublicForwarderUsesDispatch){const auto *saved=GetVxIntersectFaceDispatchTable();for(int i=0;i<4;++i){const auto *t=&kVxIntersectFaceDispatchScalar;
#if defined(VX_SIMD_SSE)
    if(i&1)t=&kVxIntersectFaceDispatchSIMD;
#endif
    g_VxIntersectFaceDispatch.store(t,std::memory_order_release);FFCheck(FFScales(VxIntersect::FaceFace));}g_VxIntersectFaceDispatch.store(saved,std::memory_order_release);}
#if defined(_WIN32)
TEST(FaceFaceReference,ActualCurrentDllExport){HMODULE dll=LoadLibraryA("VxMath.dll");ASSERT_NE(dll,nullptr);
#if defined(_WIN64)
    const char *name="?FaceFace@VxIntersect@@SAHAEBUVxVector@@0000000@Z";
#else
    const char *name="?FaceFace@VxIntersect@@SAHABUVxVector@@0000000@Z";
#endif
    FFFn fn=reinterpret_cast<FFFn>(GetProcAddress(dll,name));ASSERT_NE(fn,nullptr);FFCheck(FFSections(fn));FFCheck(FFCoplanar(fn));FFCheck(FFScales(fn));FFCheck(FFCoplanarScales(fn));FFCheck(FFTolerance(fn));FFCheck(FFTranslated(fn));FFCheck(FFRandom(fn));FFCheck(FFFootprint(fn));EXPECT_NE(FreeLibrary(dll),0);}
#endif
#endif
