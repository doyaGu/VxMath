#include <cmath>
#include <cfloat>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <new>
#include <initializer_list>
#ifndef VX_LINE_RAY_NATIVE_PROBE
#ifndef VX_LIB
#define VX_LIB 1
#endif
#ifndef VX_LINE_RAY_IMPLEMENTATION
#define VX_LINE_RAY_IMPLEMENTATION "../../src/VxDistance.cpp"
#endif
#include VX_LINE_RAY_IMPLEMENTATION
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef float (*LRFn)(const VxRay &, const VxRay &, float *, float *);
struct LRSamples { int samples=0,errors=0,valueErrors=0,parameterErrors=0,inputErrors=0; unsigned digest=2166136261u; };
static bool LREqual(float a,float b) { return std::isnan(b)?std::isnan(a):a==b; }
static VxVector LRRotate(float x,float y,float z,int axis) { VxVector v;v[axis]=x;v[(axis+1)%3]=y;v[(axis+2)%3]=z;return v; }
static void LRObserve(LRSamples &r,LRFn fn,const VxVector &o0,const VxVector &u,const VxVector &o1,const VxVector &v,
                      double expectedS,double expectedT,double expectedQ,int out0=12,int out1=13,bool root=false) {
    VxRay a,b;a.m_Origin=o0;a.m_Direction=u;b.m_Origin=o1;b.m_Direction=v;
    float before[]={o0.x,o0.y,o0.z,u.x,u.y,u.z,o1.x,o1.y,o1.z,v.x,v.y,v.z,137,139};
    float *slots[]={&a.m_Origin.x,&a.m_Origin.y,&a.m_Origin.z,&a.m_Direction.x,&a.m_Direction.y,&a.m_Direction.z,
                    &b.m_Origin.x,&b.m_Origin.y,&b.m_Origin.z,&b.m_Direction.x,&b.m_Direction.y,&b.m_Direction.z,before+12,before+13};
    float *s=out0<0?nullptr:slots[out0],*t=out1<0?nullptr:slots[out1];
    const float q=float(expectedQ),expected=root?float(std::sqrt(double(q))):q;
    const float result=fn(a,b,s,t);
    bool changed=false;for(int j=0;j<12;++j)if(j!=out0&&j!=out1)changed=changed||std::memcmp(slots[j],before+j,4)!=0;
    const bool badValue=!LREqual(result,expected);
    // Native writes t0 first and t1 last, even when both pointers are identical.
    const bool badParameter=(s&&!LREqual(*s,float(s==t?expectedT:expectedS)))||(t&&!LREqual(*t,float(expectedT)));
    ++r.samples;r.valueErrors+=badValue;r.parameterErrors+=badParameter;r.inputErrors+=changed;r.errors+=badValue||badParameter||changed;
    unsigned bits;std::memcpy(&bits,&result,4);r.digest=(r.digest^bits)*16777619u;
}
static LRSamples LRGrid(LRFn fn,bool root=false) {
    LRSamples r;
    for(int axis=0;axis<3;++axis)for(float s:{-4,-1,0,1,4})for(float t:{-4,-1,0,1,4})for(float h:{-2.0f,-.5f,0.0f,.5f,2.0f})for(int mask=0;mask<4;++mask) {
        // Orthogonal (1,2,2), (2,-1,0); their common normal (2,4,-5) has norm squared45.
        LRObserve(r,fn,LRRotate(-11,7,5,axis),LRRotate(1,2,2,axis),
                  LRRotate(-11+s-2*t+2*h,7+2*s+t+4*h,5+2*s-5*h,axis),LRRotate(2,-1,0,axis),s,t<0?0:t,45*double(h)*h+(t<0?5*double(t)*t:0),mask&1?12:-1,mask&2?13:-1,root);
    }return r;
}
static LRSamples LRDirectionScales(LRFn fn) {
    LRSamples r;
    for(int axis=0;axis<3;++axis)for(int e0:{-149,-100,-64,-32,0,32,64,100,126})for(int e1:{-149,-100,-64,-32,0,32,64,100,126})for(int sign:{-1,1})for(int raySign:{-1,1}) {
        const float u=std::ldexp(float(sign),e0),v=std::ldexp(float(raySign),e1);
        LRObserve(r,fn,VxVector(0,0,0),LRRotate(u,0,0,axis),LRRotate(2,-3,1,axis),LRRotate(0,v,0,axis),2/double(u),raySign>0?3/double(v):0,raySign>0?1:10);
    }return r;
}
static LRSamples LRParallel(LRFn fn) {
    LRSamples r;
    for(int axis=0;axis<3;++axis)for(int e:{-149,-100,-64,-32,0,32,64,100,125})for(int sign:{-1,1})for(int mask=0;mask<4;++mask) {
        const float s=std::ldexp(1.0f,e);
        LRObserve(r,fn,VxVector(0,0,0),LRRotate(s,2*s,2*s,axis),LRRotate(3,1,2,axis),LRRotate(sign*s,2*sign*s,2*sign*s,axis),1/double(s),0,5,mask&1?12:-1,mask&2?13:-1);
    }return r;
}
static LRSamples LRNearParallel(LRFn fn) {
    LRSamples r;
    for(int axis=0;axis<3;++axis)for(int e:{-149,-100,-40,-24,-23,-13,-12,-11,-1,0,10,60,100})for(int sign:{-1,1})for(int mask=0;mask<4;++mask) {
        const float d=std::ldexp(float(sign),e);
        LRObserve(r,fn,VxVector(0,0,0),LRRotate(1,0,0,axis),LRRotate(2,1,3,axis),LRRotate(1,d,0,axis),d<0?2-1/double(d):2,d<0?-1/double(d):0,d<0?9:10,mask&1?12:-1,mask&2?13:-1);
    }return r;
}
static LRSamples LRCancellation(LRFn fn) {
    LRSamples r;
    for(int axis=0;axis<3;++axis)for(int e:{0,20,50,80,100,126})for(int sign:{-1,1})for(int mask=0;mask<4;++mask) {
        const float x=std::ldexp(float(sign),e);
        LRObserve(r,fn,VxVector(0,0,0),LRRotate(1,0,0,axis),LRRotate(x,x,1,axis),LRRotate(0,1,0,axis),x,x<0?-double(x):0,x<0?1:double(x)*x+1,mask&1?12:-1,mask&2?13:-1);
        LRObserve(r,fn,VxVector(0,0,0),LRRotate(3,0,0,axis),LRRotate(x,1,0,axis),LRRotate(-7,0,0,axis),double(x)/3,0,1,mask&1?12:-1,mask&2?13:-1);
    }
    for(int axis=0;axis<3;++axis) {
        // Relative origin overflows binary32, but both line parameters and distance remain finite.
        LRObserve(r,fn,LRRotate(-FLT_MAX,0,0,axis),LRRotate(FLT_MAX,0,0,axis),LRRotate(FLT_MAX,0,1,axis),LRRotate(0,1,0,axis),2,0,1);
    }return r;
}
static LRSamples LRAliases(LRFn fn) {
    LRSamples r;
    for(int axis=0;axis<3;++axis)for(int s=-1;s<14;++s)for(int t=-1;t<14;++t) {
        LRObserve(r,fn,VxVector(0,0,0),LRRotate(1,2,2,axis),LRRotate(9,8,1,axis),LRRotate(2,-1,0,axis),3,0,65,s,t);
        LRObserve(r,fn,VxVector(0,0,0),LRRotate(1,2,2,axis),LRRotate(1,12,1,axis),LRRotate(2,-1,0,axis),3,2,45,s,t);
        LRObserve(r,fn,VxVector(0,0,0),LRRotate(1,2,2,axis),LRRotate(5,10,1,axis),LRRotate(2,-1,0,axis),3,0,45,s,t);
        LRObserve(r,fn,VxVector(0,0,0),LRRotate(1,2,2,axis),LRRotate(3,1,2,axis),LRRotate(-2,-4,-4,axis),1,0,5,s,t);
    }return r;
}
static LRSamples LRZero(LRFn fn) {
    LRSamples r;
    for(int axis=0;axis<3;++axis)for(int mask=0;mask<4;++mask) {
        LRObserve(r,fn,VxVector(0,0,0),VxVector(0,0,0),LRRotate(3,1,2,axis),LRRotate(1,2,2,axis),NAN,0,NAN,mask&1?12:-1,mask&2?13:-1);
        LRObserve(r,fn,VxVector(0,0,0),LRRotate(1,2,2,axis),LRRotate(3,1,2,axis),VxVector(0,0,0),1,0,5,mask&1?12:-1,mask&2?13:-1);
        LRObserve(r,fn,VxVector(0,0,0),VxVector(0,0,0),LRRotate(3,1,2,axis),VxVector(0,0,0),NAN,0,NAN,mask&1?12:-1,mask&2?13:-1);
    }return r;
}
static LRSamples LRBoundary(LRFn fn) {
    LRSamples r;
    for(int axis=0;axis<3;++axis)for(int ev:{-126,-100,-1,0,1,100,126})for(int ey:{-149,-126,-70,-1,0,70,126})for(int sign:{-1,1})for(int mask=0;mask<4;++mask) {
        const float v=std::ldexp(1.0f,ev),y=std::ldexp(float(sign),ey);
        // Positive parameters remain interior even when their float output
        // rounds to zero. A tiny negative parameter still selects the origin.
        LRObserve(r,fn,VxVector(0,0,0),LRRotate(1,0,0,axis),LRRotate(2,y,0,axis),LRRotate(0,v,0,axis),2,sign<0?-double(y)/v:0,sign<0?0:double(y)*y,mask&1?12:-1,mask&2?13:-1);
    }return r;
}
static unsigned LRNext(unsigned &seed) { seed=seed*1664525u+1013904223u;return seed; }
static LRSamples LRRandom(LRFn fn) {
    LRSamples r;unsigned seed=280;
    while(r.samples<4000) {
        VxVector o0,u,o1,v;for(int j=0;j<3;++j) {o0[j]=float(int(LRNext(seed)%33)-16);o1[j]=float(int(LRNext(seed)%33)-16);u[j]=float(int(LRNext(seed)%11)-5);v[j]=float(int(LRNext(seed)%11)-5);}
        // Independent two-by-two normal equations. All products/sums below
        // are exact small integers, unlike the production triple products.
        double a=0,b=0,c=0,d=0,e=0,f=0;
        for(int j=0;j<3;++j) {const double w=double(o0[j])-o1[j];a+=double(u[j])*u[j];b+=double(u[j])*v[j];c+=double(v[j])*v[j];d+=w*u[j];e+=w*v[j];f+=w*w;}
        const double det=a*c-b*b;double s,t,q;
        if(det==0||a*e-b*d<0) {t=0;s=-d/a;q=(f*a-d*d)/a;}
        else {s=(b*e-c*d)/det;t=(a*e-b*d)/det;q=(f*det-c*d*d+2*b*d*e-a*e*e)/det;}
        LRObserve(r,fn,o0,u,o1,v,s,t,q,r.samples&1?12:-1,r.samples&2?13:-1);
    }return r;
}
#if defined(_WIN32)
static LRSamples LRFootprint(LRFn fn) {
    LRSamples r;SYSTEM_INFO si;GetSystemInfo(&si);const size_t page=si.dwPageSize;
    char *pages=static_cast<char*>(VirtualAlloc(nullptr,page*8,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));if(!pages){r.errors=1;return r;}
    VxRay *a=new(pages+page-24)VxRay,*b=new(pages+page*3-24)VxRay;
    a->m_Origin=VxVector(0,0,0);a->m_Direction=VxVector(1,0,0);b->m_Origin=VxVector(2,3,4);b->m_Direction=VxVector(0,1,0);
    float *s=new(pages+page*5-4)float(137),*t=new(pages+page*7-4)float(139);
    DWORD old;bool good=true;for(int i=0;i<4;++i)good=VirtualProtect(pages+page*(2*i+1),page,PAGE_NOACCESS,&old)&&good;
    for(int i=0;i<2;++i)good=VirtualProtect(pages+page*2*i,page,PAGE_READONLY,&old)&&good;
    if(good){r.samples=2;r.errors=(fn(*a,*b,s,t)!=25||*s!=2||*t!=0)+(fn(*a,*b,nullptr,nullptr)!=25);}else r.errors=1;
    VirtualFree(pages,0,MEM_RELEASE);return r;
}
#endif
#ifndef VX_LINE_RAY_NATIVE_PROBE
static void LRCheck(LRSamples r) { EXPECT_EQ(r.errors,0)<<"values="<<r.valueErrors<<" parameters="<<r.parameterErrors<<" changed inputs="<<r.inputErrors;::testing::Test::RecordProperty("samples",r.samples); }
// The before implementation has no dedicated line kernels. This adapter lets
// the identical geometry fixture exercise its real old dispatch for red runs.
static float LRScalar(const VxRay &a,const VxRay &b,float *s,float *t) {g_VxDistanceDispatch.store(&kVxDistanceDispatchScalar,std::memory_order_release);return VxDistance::LineRaySquareDistance(a,b,s,t);}
#if defined(VX_SIMD_SSE)
static float LRSimd(const VxRay &a,const VxRay &b,float *s,float *t) {g_VxDistanceDispatch.store(&kVxDistanceDispatchSIMD,std::memory_order_release);return VxDistance::LineRaySquareDistance(a,b,s,t);}
#endif
#define LR_TESTS(prefix,fn) \
TEST(LineRayDistanceReference,prefix##SkewAffineGridAndOptionalOutputs){LRCheck(LRGrid(fn));} \
TEST(LineRayDistanceReference,prefix##IndependentDirectionScales){LRCheck(LRDirectionScales(fn));} \
TEST(LineRayDistanceReference,prefix##ParallelParameterConvention){LRCheck(LRParallel(fn));} \
TEST(LineRayDistanceReference,prefix##NonparallelBelowOriginalAbsoluteThreshold){LRCheck(LRNearParallel(fn));} \
TEST(LineRayDistanceReference,prefix##SmallDistanceSurvivesCancellation){LRCheck(LRCancellation(fn));} \
TEST(LineRayDistanceReference,prefix##BothOutputsMayAliasEitherInputOrEachOther){LRCheck(LRAliases(fn));} \
TEST(LineRayDistanceReference,prefix##OriginalZeroDirectionConvention){LRCheck(LRZero(fn));} \
TEST(LineRayDistanceReference,prefix##StrictOriginBoundaryBeforeParameterRounding){LRCheck(LRBoundary(fn));} \
TEST(LineRayDistanceReference,prefix##IndependentNormalEquationOracle){LRCheck(LRRandom(fn));}
LR_TESTS(Scalar,LRScalar)
#if defined(VX_SIMD_SSE)
LR_TESTS(Simd,LRSimd)
#endif
#if defined(_WIN32)
TEST(LineRayDistanceReference,ScalarTwentyFourAndFourByteFootprints){LRCheck(LRFootprint(LRScalar));}
#if defined(VX_SIMD_SSE)
TEST(LineRayDistanceReference,SimdTwentyFourAndFourByteFootprints){LRCheck(LRFootprint(LRSimd));}
#endif
#endif
TEST(LineRayDistanceReference,PublicDistanceWrapperKeepsSquaredFloatRange) {
    LRCheck(LRGrid(VxDistance::LineRayDistance,true));VxRay a,b;
    a.m_Origin=VxVector(0,0,0);a.m_Direction=VxVector(1,0,0);b.m_Origin=VxVector(0,0,std::ldexp(1.0f,100));b.m_Direction=VxVector(0,1,0);
    float s,t;EXPECT_EQ(VxDistance::LineRayDistance(a,b,&s,&t),INFINITY);EXPECT_EQ(s,0);EXPECT_EQ(t,0);
}
#if defined(_WIN32)
TEST(LineRayDistanceReference,ActualCurrentDllExport) {
    HMODULE dll=LoadLibraryA("VxMath.dll");ASSERT_NE(dll,nullptr);
#if defined(_WIN64)
    const char *symbol="?LineRaySquareDistance@VxDistance@@SAMAEBVVxRay@@0PEAM1@Z";
#else
    const char *symbol="?LineRaySquareDistance@VxDistance@@SAMABVVxRay@@0PAM1@Z";
#endif
    LRFn fn=reinterpret_cast<LRFn>(GetProcAddress(dll,symbol));ASSERT_NE(fn,nullptr);
    LRCheck(LRGrid(fn));LRCheck(LRDirectionScales(fn));LRCheck(LRParallel(fn));LRCheck(LRNearParallel(fn));LRCheck(LRCancellation(fn));LRCheck(LRAliases(fn));LRCheck(LRZero(fn));LRCheck(LRBoundary(fn));LRCheck(LRRandom(fn));LRCheck(LRFootprint(fn));EXPECT_NE(FreeLibrary(dll),0);
}
#endif
#endif
