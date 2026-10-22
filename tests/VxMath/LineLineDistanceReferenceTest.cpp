#include <cmath>
#include <cfloat>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <new>
#include <initializer_list>
#ifndef VX_LINE_LINE_NATIVE_PROBE
#ifndef VX_LIB
#define VX_LIB 1
#endif
#ifndef VX_LINE_LINE_IMPLEMENTATION
#define VX_LINE_LINE_IMPLEMENTATION "../../src/VxDistance.cpp"
#endif
#include VX_LINE_LINE_IMPLEMENTATION
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef float (*LLFn)(const VxRay &, const VxRay &, float *, float *);
struct LLSamples { int samples=0,errors=0,valueErrors=0,parameterErrors=0,inputErrors=0; unsigned digest=2166136261u; };
static bool LLEqual(float a,float b) { return std::isnan(b)?std::isnan(a):a==b; }
static VxVector LLRotate(float x,float y,float z,int axis) { VxVector v;v[axis]=x;v[(axis+1)%3]=y;v[(axis+2)%3]=z;return v; }
static void LLObserve(LLSamples &r,LLFn fn,const VxVector &o0,const VxVector &u,const VxVector &o1,const VxVector &v,
                      double expectedS,double expectedT,double expectedQ,int out0=12,int out1=13,bool root=false) {
    VxRay a,b;a.m_Origin=o0;a.m_Direction=u;b.m_Origin=o1;b.m_Direction=v;
    float before[]={o0.x,o0.y,o0.z,u.x,u.y,u.z,o1.x,o1.y,o1.z,v.x,v.y,v.z,137,139};
    float *slots[]={&a.m_Origin.x,&a.m_Origin.y,&a.m_Origin.z,&a.m_Direction.x,&a.m_Direction.y,&a.m_Direction.z,
                    &b.m_Origin.x,&b.m_Origin.y,&b.m_Origin.z,&b.m_Direction.x,&b.m_Direction.y,&b.m_Direction.z,before+12,before+13};
    float *s=out0<0?nullptr:slots[out0],*t=out1<0?nullptr:slots[out1];
    const float q=float(expectedQ),expected=root?float(std::sqrt(double(q))):q;
    const float result=fn(a,b,s,t);
    bool changed=false;for(int j=0;j<12;++j)if(j!=out0&&j!=out1)changed=changed||std::memcmp(slots[j],before+j,4)!=0;
    const bool badValue=!LLEqual(result,expected);
    // Native writes t0 first and t1 last, even when both pointers are identical.
    const bool badParameter=(s&&!LLEqual(*s,float(s==t?expectedT:expectedS)))||(t&&!LLEqual(*t,float(expectedT)));
    ++r.samples;r.valueErrors+=badValue;r.parameterErrors+=badParameter;r.inputErrors+=changed;r.errors+=badValue||badParameter||changed;
    unsigned bits;std::memcpy(&bits,&result,4);r.digest=(r.digest^bits)*16777619u;
}
static LLSamples LLGrid(LLFn fn,bool root=false) {
    LLSamples r;
    for(int axis=0;axis<3;++axis)for(float s:{-4,-1,0,1,4})for(float t:{-4,-1,0,1,4})for(float h:{-2.0f,-.5f,0.0f,.5f,2.0f})for(int mask=0;mask<4;++mask) {
        // Orthogonal (1,2,2), (2,-1,0); their common normal (2,4,-5) has norm squared45.
        LLObserve(r,fn,LLRotate(-11,7,5,axis),LLRotate(1,2,2,axis),
                  LLRotate(-11+s-2*t+2*h,7+2*s+t+4*h,5+2*s-5*h,axis),LLRotate(2,-1,0,axis),s,t,45*double(h)*h,mask&1?12:-1,mask&2?13:-1,root);
    }return r;
}
static LLSamples LLDirectionScales(LLFn fn) {
    LLSamples r;
    for(int axis=0;axis<3;++axis)for(int e0:{-149,-100,-64,-32,0,32,64,100,126})for(int e1:{-149,-100,-64,-32,0,32,64,100,126})for(int sign:{-1,1}) {
        const float u=std::ldexp(float(sign),e0),v=std::ldexp(1.0f,e1);
        LLObserve(r,fn,VxVector(0,0,0),LLRotate(u,0,0,axis),LLRotate(2,-3,1,axis),LLRotate(0,v,0,axis),2/double(u),3/double(v),1);
    }return r;
}
static LLSamples LLParallel(LLFn fn) {
    LLSamples r;
    for(int axis=0;axis<3;++axis)for(int e:{-149,-100,-64,-32,0,32,64,100,125})for(int sign:{-1,1})for(int mask=0;mask<4;++mask) {
        const float s=std::ldexp(1.0f,e);
        LLObserve(r,fn,VxVector(0,0,0),LLRotate(s,2*s,2*s,axis),LLRotate(3,1,2,axis),LLRotate(sign*s,2*sign*s,2*sign*s,axis),1/double(s),0,5,mask&1?12:-1,mask&2?13:-1);
    }return r;
}
static LLSamples LLNearParallel(LLFn fn) {
    LLSamples r;
    for(int axis=0;axis<3;++axis)for(int e:{-149,-100,-40,-24,-23,-13,-12,-11,-1,0,10,60,100})for(int sign:{-1,1})for(int mask=0;mask<4;++mask) {
        const float d=std::ldexp(float(sign),e);
        LLObserve(r,fn,VxVector(0,0,0),LLRotate(1,0,0,axis),LLRotate(2,1,3,axis),LLRotate(1,d,0,axis),2-1/double(d),-1/double(d),9,mask&1?12:-1,mask&2?13:-1);
    }return r;
}
static LLSamples LLCancellation(LLFn fn) {
    LLSamples r;
    for(int axis=0;axis<3;++axis)for(int e:{0,20,50,80,100,126})for(int sign:{-1,1})for(int mask=0;mask<4;++mask) {
        const float x=std::ldexp(float(sign),e);
        LLObserve(r,fn,VxVector(0,0,0),LLRotate(1,0,0,axis),LLRotate(x,x,1,axis),LLRotate(0,1,0,axis),x,-double(x),1,mask&1?12:-1,mask&2?13:-1);
        LLObserve(r,fn,VxVector(0,0,0),LLRotate(3,0,0,axis),LLRotate(x,1,0,axis),LLRotate(-7,0,0,axis),double(x)/3,0,1,mask&1?12:-1,mask&2?13:-1);
    }
    for(int axis=0;axis<3;++axis) {
        // Relative origin overflows binary32, but both line parameters and distance remain finite.
        LLObserve(r,fn,LLRotate(-FLT_MAX,0,0,axis),LLRotate(FLT_MAX,0,0,axis),LLRotate(FLT_MAX,0,1,axis),LLRotate(0,1,0,axis),2,0,1);
    }return r;
}
static LLSamples LLAliases(LLFn fn) {
    LLSamples r;
    for(int axis=0;axis<3;++axis)for(int s=-1;s<14;++s)for(int t=-1;t<14;++t) {
        LLObserve(r,fn,VxVector(0,0,0),LLRotate(1,2,2,axis),LLRotate(9,8,1,axis),LLRotate(2,-1,0,axis),3,-2,45,s,t);
        LLObserve(r,fn,VxVector(0,0,0),LLRotate(1,2,2,axis),LLRotate(3,1,2,axis),LLRotate(-2,-4,-4,axis),1,0,5,s,t);
    }return r;
}
static LLSamples LLZero(LLFn fn) {
    LLSamples r;
    for(int axis=0;axis<3;++axis)for(int mask=0;mask<4;++mask) {
        LLObserve(r,fn,VxVector(0,0,0),VxVector(0,0,0),LLRotate(3,1,2,axis),LLRotate(1,2,2,axis),NAN,0,NAN,mask&1?12:-1,mask&2?13:-1);
        LLObserve(r,fn,VxVector(0,0,0),LLRotate(1,2,2,axis),LLRotate(3,1,2,axis),VxVector(0,0,0),1,0,5,mask&1?12:-1,mask&2?13:-1);
        LLObserve(r,fn,VxVector(0,0,0),VxVector(0,0,0),LLRotate(3,1,2,axis),VxVector(0,0,0),NAN,0,NAN,mask&1?12:-1,mask&2?13:-1);
    }return r;
}
static unsigned LLNext(unsigned &seed) { seed=seed*1664525u+1013904223u;return seed; }
static LLSamples LLRandom(LLFn fn) {
    LLSamples r;unsigned seed=280;
    while(r.samples<4000) {
        VxVector o0,u,o1,v;for(int j=0;j<3;++j) {o0[j]=float(int(LLNext(seed)%33)-16);o1[j]=float(int(LLNext(seed)%33)-16);u[j]=float(int(LLNext(seed)%11)-5);v[j]=float(int(LLNext(seed)%11)-5);}
        // Independent two-by-two normal equations. All products/sums below
        // are exact small integers, unlike the production triple products.
        double a=0,b=0,c=0,d=0,e=0,f=0;
        for(int j=0;j<3;++j) {const double w=double(o0[j])-o1[j];a+=double(u[j])*u[j];b+=double(u[j])*v[j];c+=double(v[j])*v[j];d+=w*u[j];e+=w*v[j];f+=w*w;}
        const double det=a*c-b*b;double s,t,q;
        if(det==0) {t=0;s=-d/a;q=(f*a-d*d)/a;}
        else {s=(b*e-c*d)/det;t=(a*e-b*d)/det;q=(f*det-c*d*d+2*b*d*e-a*e*e)/det;}
        LLObserve(r,fn,o0,u,o1,v,s,t,q,r.samples&1?12:-1,r.samples&2?13:-1);
    }return r;
}
#if defined(_WIN32)
static LLSamples LLFootprint(LLFn fn) {
    LLSamples r;SYSTEM_INFO si;GetSystemInfo(&si);const size_t page=si.dwPageSize;
    char *pages=static_cast<char*>(VirtualAlloc(nullptr,page*8,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));if(!pages){r.errors=1;return r;}
    VxRay *a=new(pages+page-24)VxRay,*b=new(pages+page*3-24)VxRay;
    a->m_Origin=VxVector(0,0,0);a->m_Direction=VxVector(1,0,0);b->m_Origin=VxVector(2,3,4);b->m_Direction=VxVector(0,1,0);
    float *s=new(pages+page*5-4)float(137),*t=new(pages+page*7-4)float(139);
    DWORD old;bool good=true;for(int i=0;i<4;++i)good=VirtualProtect(pages+page*(2*i+1),page,PAGE_NOACCESS,&old)&&good;
    for(int i=0;i<2;++i)good=VirtualProtect(pages+page*2*i,page,PAGE_READONLY,&old)&&good;
    if(good){r.samples=2;r.errors=(fn(*a,*b,s,t)!=16||*s!=2||*t!=-3)+(fn(*a,*b,nullptr,nullptr)!=16);}else r.errors=1;
    VirtualFree(pages,0,MEM_RELEASE);return r;
}
#endif
#ifndef VX_LINE_LINE_NATIVE_PROBE
static void LLCheck(LLSamples r) { EXPECT_EQ(r.errors,0)<<"values="<<r.valueErrors<<" parameters="<<r.parameterErrors<<" changed inputs="<<r.inputErrors;::testing::Test::RecordProperty("samples",r.samples); }
// The before implementation has no dedicated line kernels. This adapter lets
// the identical geometry fixture exercise its real old dispatch for red runs.
static float LLScalar(const VxRay &a,const VxRay &b,float *s,float *t) {g_VxDistanceDispatch.store(&kVxDistanceDispatchScalar,std::memory_order_release);return VxDistance::LineLineSquareDistance(a,b,s,t);}
#if defined(VX_SIMD_SSE)
static float LLSimd(const VxRay &a,const VxRay &b,float *s,float *t) {g_VxDistanceDispatch.store(&kVxDistanceDispatchSIMD,std::memory_order_release);return VxDistance::LineLineSquareDistance(a,b,s,t);}
#endif
#define LL_TESTS(prefix,fn) \
TEST(LineLineDistanceReference,prefix##SkewAffineGridAndOptionalOutputs){LLCheck(LLGrid(fn));} \
TEST(LineLineDistanceReference,prefix##IndependentDirectionScales){LLCheck(LLDirectionScales(fn));} \
TEST(LineLineDistanceReference,prefix##ParallelParameterConvention){LLCheck(LLParallel(fn));} \
TEST(LineLineDistanceReference,prefix##NonparallelBelowOriginalAbsoluteThreshold){LLCheck(LLNearParallel(fn));} \
TEST(LineLineDistanceReference,prefix##SmallDistanceSurvivesCancellation){LLCheck(LLCancellation(fn));} \
TEST(LineLineDistanceReference,prefix##BothOutputsMayAliasEitherInputOrEachOther){LLCheck(LLAliases(fn));} \
TEST(LineLineDistanceReference,prefix##OriginalZeroDirectionConvention){LLCheck(LLZero(fn));} \
TEST(LineLineDistanceReference,prefix##IndependentNormalEquationOracle){LLCheck(LLRandom(fn));}
LL_TESTS(Scalar,LLScalar)
#if defined(VX_SIMD_SSE)
LL_TESTS(Simd,LLSimd)
#endif
#if defined(_WIN32)
TEST(LineLineDistanceReference,ScalarTwentyFourAndFourByteFootprints){LLCheck(LLFootprint(LLScalar));}
#if defined(VX_SIMD_SSE)
TEST(LineLineDistanceReference,SimdTwentyFourAndFourByteFootprints){LLCheck(LLFootprint(LLSimd));}
#endif
#endif
TEST(LineLineDistanceReference,PublicDistanceWrapperKeepsSquaredFloatRange) {
    LLCheck(LLGrid(VxDistance::LineLineDistance,true));VxRay a,b;
    a.m_Origin=VxVector(0,0,0);a.m_Direction=VxVector(1,0,0);b.m_Origin=VxVector(0,0,std::ldexp(1.0f,100));b.m_Direction=VxVector(0,1,0);
    float s,t;EXPECT_EQ(VxDistance::LineLineDistance(a,b,&s,&t),INFINITY);EXPECT_EQ(s,0);EXPECT_EQ(t,0);
}
#if defined(_WIN32)
TEST(LineLineDistanceReference,ActualCurrentDllExport) {
    HMODULE dll=LoadLibraryA("VxMath.dll");ASSERT_NE(dll,nullptr);
#if defined(_WIN64)
    const char *symbol="?LineLineSquareDistance@VxDistance@@SAMAEBVVxRay@@0PEAM1@Z";
#else
    const char *symbol="?LineLineSquareDistance@VxDistance@@SAMABVVxRay@@0PAM1@Z";
#endif
    LLFn fn=reinterpret_cast<LLFn>(GetProcAddress(dll,symbol));ASSERT_NE(fn,nullptr);
    LLCheck(LLGrid(fn));LLCheck(LLDirectionScales(fn));LLCheck(LLParallel(fn));LLCheck(LLNearParallel(fn));LLCheck(LLCancellation(fn));LLCheck(LLAliases(fn));LLCheck(LLZero(fn));LLCheck(LLRandom(fn));LLCheck(LLFootprint(fn));EXPECT_NE(FreeLibrary(dll),0);
}
#endif
#endif
