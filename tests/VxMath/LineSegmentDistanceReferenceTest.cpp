#include <cmath>
#include <cfloat>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <new>
#include <initializer_list>
#ifndef VX_LINE_SEGMENT_NATIVE_PROBE
#ifndef VX_LIB
#define VX_LIB 1
#endif
#ifndef VX_LINE_SEGMENT_IMPLEMENTATION
#define VX_LINE_SEGMENT_IMPLEMENTATION "../../src/VxDistance.cpp"
#endif
#include VX_LINE_SEGMENT_IMPLEMENTATION
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef float (*LSFn)(const VxRay &, const VxRay &, float *, float *);
struct LSSamples { int samples=0,errors=0,valueErrors=0,parameterErrors=0,inputErrors=0; unsigned digest=2166136261u; };
static bool LSEqual(float a,float b) { return std::isnan(b)?std::isnan(a):a==b; }
static VxVector LSRotate(float x,float y,float z,int axis) { VxVector v;v[axis]=x;v[(axis+1)%3]=y;v[(axis+2)%3]=z;return v; }
static void LSObserve(LSSamples &r,LSFn fn,const VxVector &o0,const VxVector &u,const VxVector &o1,const VxVector &v,
                      double expectedS,double expectedT,double expectedQ,int out0=12,int out1=13,bool root=false) {
    VxRay a,b;a.m_Origin=o0;a.m_Direction=u;b.m_Origin=o1;b.m_Direction=v;
    float before[]={o0.x,o0.y,o0.z,u.x,u.y,u.z,o1.x,o1.y,o1.z,v.x,v.y,v.z,137,139};
    float *slots[]={&a.m_Origin.x,&a.m_Origin.y,&a.m_Origin.z,&a.m_Direction.x,&a.m_Direction.y,&a.m_Direction.z,
                    &b.m_Origin.x,&b.m_Origin.y,&b.m_Origin.z,&b.m_Direction.x,&b.m_Direction.y,&b.m_Direction.z,before+12,before+13};
    float *s=out0<0?nullptr:slots[out0],*t=out1<0?nullptr:slots[out1];
    const float q=float(expectedQ),expected=root?float(std::sqrt(double(q))):q;
    const float result=fn(a,b,s,t);
    bool changed=false;for(int j=0;j<12;++j)if(j!=out0&&j!=out1)changed=changed||std::memcmp(slots[j],before+j,4)!=0;
    const bool badValue=!LSEqual(result,expected);
    // Native writes t0 first and t1 last, even when both pointers are identical.
    const bool badParameter=(s&&!LSEqual(*s,float(s==t?expectedT:expectedS)))||(t&&!LSEqual(*t,float(expectedT)));
    ++r.samples;r.valueErrors+=badValue;r.parameterErrors+=badParameter;r.inputErrors+=changed;r.errors+=badValue||badParameter||changed;
    unsigned bits;std::memcpy(&bits,&result,4);r.digest=(r.digest^bits)*16777619u;
}
static LSSamples LSGrid(LSFn fn,bool root=false) {
    LSSamples r;
    for(int axis=0;axis<3;++axis)for(float s:{-4,-1,0,1,4})for(float t:{-2.0f,-.25f,0.0f,.25f,.5f,.75f,1.0f,1.25f,4.0f})for(float h:{-2.0f,-.5f,0.0f,.5f,2.0f})for(int mask=0;mask<4;++mask) {
        // Orthogonal (1,2,2), (2,-1,0); their common normal (2,4,-5) has norm squared45.
        LSObserve(r,fn,LSRotate(-11,7,5,axis),LSRotate(1,2,2,axis),
                  LSRotate(-11+s-2*t+2*h,7+2*s+t+4*h,5+2*s-5*h,axis),LSRotate(2,-1,0,axis),s,t<0?0:t>1?1:t,45*double(h)*h+5*std::pow(double(t)-(t<0?0:t>1?1:t),2),mask&1?12:-1,mask&2?13:-1,root);
    }return r;
}
static LSSamples LSDirectionScales(LSFn fn) {
    LSSamples r;
    for(int axis=0;axis<3;++axis)for(int e0:{-149,-100,-64,-32,0,32,64,100,126})for(int e1:{-149,-100,-64,-32,0,32,64,100,126})for(int sign:{-1,1})for(int raySign:{-1,1}) {
        const float u=std::ldexp(float(sign),e0),v=std::ldexp(float(raySign),e1);
        LSObserve(r,fn,VxVector(0,0,0),LSRotate(u,0,0,axis),LSRotate(2,-3,1,axis),LSRotate(0,v,0,axis),2/double(u),raySign<0?0:v<3?1:3/double(v),raySign<0?10:v<3?1+std::pow(3-double(v),2):1);
    }return r;
}
static LSSamples LSParallel(LSFn fn) {
    LSSamples r;
    for(int axis=0;axis<3;++axis)for(int e:{-149,-100,-64,-32,0,32,64,100,125})for(int sign:{-1,1})for(int mask=0;mask<4;++mask) {
        const float s=std::ldexp(1.0f,e);
        LSObserve(r,fn,VxVector(0,0,0),LSRotate(s,2*s,2*s,axis),LSRotate(3,1,2,axis),LSRotate(sign*s,2*sign*s,2*sign*s,axis),1/double(s),0,5,mask&1?12:-1,mask&2?13:-1);
    }return r;
}
static LSSamples LSNearParallel(LSFn fn) {
    LSSamples r;
    for(int axis=0;axis<3;++axis)for(int e:{-149,-100,-40,-24,-23,-13,-12,-11,-1,0,10,60,100})for(int sign:{-1,1})for(int mask=0;mask<4;++mask) {
        const float d=std::ldexp(float(sign),e);
        LSObserve(r,fn,VxVector(0,0,0),LSRotate(1,0,0,axis),LSRotate(2,1,3,axis),LSRotate(1,d,0,axis),d>=0?2:d> -1?3:2-1/double(d),d>=0?0:d> -1?1:-1/double(d),d>=0?10:d> -1?9+std::pow(1+double(d),2):9,mask&1?12:-1,mask&2?13:-1);
    }return r;
}
static LSSamples LSCancellation(LSFn fn) {
    LSSamples r;
    for(int axis=0;axis<3;++axis)for(int e:{0,20,50,80,100,126})for(int sign:{-1,1})for(int mask=0;mask<4;++mask) {
        const float x=std::ldexp(float(sign),e);
        LSObserve(r,fn,VxVector(0,0,0),LSRotate(1,0,0,axis),LSRotate(x,x,1,axis),LSRotate(0,1,0,axis),x,x>=0?0:x< -1?1:-double(x),x>=0?double(x)*x+1:x< -1?std::pow(double(x)+1,2)+1:1,mask&1?12:-1,mask&2?13:-1);
        LSObserve(r,fn,VxVector(0,0,0),LSRotate(3,0,0,axis),LSRotate(x,1,0,axis),LSRotate(-7,0,0,axis),double(x)/3,0,1,mask&1?12:-1,mask&2?13:-1);
    }
    for(int axis=0;axis<3;++axis) {
        // Relative origin overflows binary32, but both line parameters and distance remain finite.
        LSObserve(r,fn,LSRotate(-FLT_MAX,0,0,axis),LSRotate(FLT_MAX,0,0,axis),LSRotate(FLT_MAX,0,1,axis),LSRotate(0,1,0,axis),2,0,1);
    }
    for(int axis=0;axis<3;++axis)for(int e:{20,50,60,100,126}) {
        const float large=std::ldexp(1.0f,e);
        for(int sign:{-1,1}) {
            // End is exactly zero. An offset of one from that end must not
            // disappear when subtracting the much larger segment origin.
            LSObserve(r,fn,LSRotate(0,float(sign),0,axis),LSRotate(1,0,0,axis),LSRotate(0,large,0,axis),LSRotate(0,-large,0,axis),0,sign<0?1:1-1/double(large),sign<0?1:0);
        }
        // End is large+1. Adding its two binary32 components first can lose
        // the unit offset even in double; the endpoint displacement is (-1,-3).
        LSObserve(r,fn,LSRotate(0,large,-3,axis),LSRotate(0,1,1,axis),LSRotate(0,large,0,axis),LSRotate(0,1,0,axis),2,1,2);
    }return r;
}
static LSSamples LSAliases(LSFn fn) {
    LSSamples r;
    for(int axis=0;axis<3;++axis)for(int s=-1;s<14;++s)for(int t=-1;t<14;++t) {
        LSObserve(r,fn,VxVector(0,0,0),LSRotate(1,2,2,axis),LSRotate(9,8,1,axis),LSRotate(2,-1,0,axis),3,0,65,s,t);
        LSObserve(r,fn,VxVector(0,0,0),LSRotate(1,2,2,axis),LSRotate(4,10.5f,1,axis),LSRotate(2,-1,0,axis),3,.5,45,s,t);
        LSObserve(r,fn,VxVector(0,0,0),LSRotate(1,2,2,axis),LSRotate(1,12,1,axis),LSRotate(2,-1,0,axis),3,1,50,s,t);
        LSObserve(r,fn,VxVector(0,0,0),LSRotate(1,2,2,axis),LSRotate(3,11,1,axis),LSRotate(2,-1,0,axis),3,1,45,s,t);
        LSObserve(r,fn,VxVector(0,0,0),LSRotate(1,2,2,axis),LSRotate(5,10,1,axis),LSRotate(2,-1,0,axis),3,0,45,s,t);
        LSObserve(r,fn,VxVector(0,0,0),LSRotate(1,2,2,axis),LSRotate(3,1,2,axis),LSRotate(-2,-4,-4,axis),1,0,5,s,t);
    }return r;
}
static LSSamples LSZero(LSFn fn) {
    LSSamples r;
    for(int axis=0;axis<3;++axis)for(int mask=0;mask<4;++mask) {
        LSObserve(r,fn,VxVector(0,0,0),VxVector(0,0,0),LSRotate(3,1,2,axis),LSRotate(1,2,2,axis),NAN,0,NAN,mask&1?12:-1,mask&2?13:-1);
        LSObserve(r,fn,VxVector(0,0,0),LSRotate(1,2,2,axis),LSRotate(3,1,2,axis),VxVector(0,0,0),1,0,5,mask&1?12:-1,mask&2?13:-1);
        LSObserve(r,fn,VxVector(0,0,0),VxVector(0,0,0),LSRotate(3,1,2,axis),VxVector(0,0,0),NAN,0,NAN,mask&1?12:-1,mask&2?13:-1);
    }return r;
}
static LSSamples LSBoundary(LSFn fn) {
    LSSamples r;
    for(int axis=0;axis<3;++axis)for(int ev:{-126,-100,-1,0,1,100,126})for(int ey:{-149,-126,-70,-1,0,70,126})for(int sign:{-1,1})for(int mask=0;mask<4;++mask) {
        const float v=std::ldexp(1.0f,ev),y=std::ldexp(float(sign),ey);
        // Positive parameters remain interior even when their float output
        // rounds to zero. A tiny negative parameter still selects the origin.
        LSObserve(r,fn,VxVector(0,0,0),LSRotate(1,0,0,axis),LSRotate(2,y,0,axis),LSRotate(0,v,0,axis),2,sign>0?0:-double(y)>v?1:-double(y)/v,sign>0?double(y)*y:-double(y)>v?std::pow(double(y)+v,2):0,mask&1?12:-1,mask&2?13:-1);
    }
    for(int axis=0;axis<3;++axis)for(int ev:{-100,-20,0,20,100})
        for(float t:{.5f,std::nextafter(1.0f,0.0f),1.0f,std::nextafter(1.0f,INFINITY),2.0f})for(int mask=0;mask<4;++mask) {
            const float v=std::ldexp(1.0f,ev);
            LSObserve(r,fn,VxVector(0,0,0),LSRotate(1,0,0,axis),LSRotate(2,-v*t,0,axis),LSRotate(0,v,0,axis),2,t>1?1:t,t>1?std::pow(double(v)*(double(t)-1),2):0,mask&1?12:-1,mask&2?13:-1);
        }
    return r;
}
static unsigned LSNext(unsigned &seed) { seed=seed*1664525u+1013904223u;return seed; }
static LSSamples LSRandom(LSFn fn) {
    LSSamples r;unsigned seed=280;
    while(r.samples<4000) {
        VxVector o0,u,o1,v;for(int j=0;j<3;++j) {o0[j]=float(int(LSNext(seed)%33)-16);o1[j]=float(int(LSNext(seed)%33)-16);u[j]=float(int(LSNext(seed)%11)-5);v[j]=float(int(LSNext(seed)%11)-5);}
        // Independent two-by-two normal equations. All products/sums below
        // are exact small integers, unlike the production triple products.
        double a=0,b=0,c=0,d=0,e=0,f=0;
        for(int j=0;j<3;++j) {const double w=double(o0[j])-o1[j];a+=double(u[j])*u[j];b+=double(u[j])*v[j];c+=double(v[j])*v[j];d+=w*u[j];e+=w*v[j];f+=w*w;}
        const double det=a*c-b*b;double s,t,q;
        if(det==0||a*e-b*d<0) {t=0;s=-d/a;q=(f*a-d*d)/a;}
        else if(a*e-b*d>det) {t=1;s=(b-d)/a;q=((f-2*e+c)*a-(d-b)*(d-b))/a;}
        else {s=(b*e-c*d)/det;t=(a*e-b*d)/det;q=(f*det-c*d*d+2*b*d*e-a*e*e)/det;}
        LSObserve(r,fn,o0,u,o1,v,s,t,q,r.samples&1?12:-1,r.samples&2?13:-1);
    }return r;
}
#if defined(_WIN32)
static LSSamples LSFootprint(LSFn fn) {
    LSSamples r;SYSTEM_INFO si;GetSystemInfo(&si);const size_t page=si.dwPageSize;
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
#ifndef VX_LINE_SEGMENT_NATIVE_PROBE
static void LSCheck(LSSamples r) { EXPECT_EQ(r.errors,0)<<"values="<<r.valueErrors<<" parameters="<<r.parameterErrors<<" changed inputs="<<r.inputErrors;::testing::Test::RecordProperty("samples",r.samples); }
// The before implementation has no dedicated line kernels. This adapter lets
// the identical geometry fixture exercise its real old dispatch for red runs.
static float LSScalar(const VxRay &a,const VxRay &b,float *s,float *t) {g_VxDistanceDispatch.store(&kVxDistanceDispatchScalar,std::memory_order_release);return VxDistance::LineSegmentSquareDistance(a,b,s,t);}
#if defined(VX_SIMD_SSE)
static float LSSimd(const VxRay &a,const VxRay &b,float *s,float *t) {g_VxDistanceDispatch.store(&kVxDistanceDispatchSIMD,std::memory_order_release);return VxDistance::LineSegmentSquareDistance(a,b,s,t);}
#endif
#define LS_TESTS(prefix,fn) \
TEST(LineSegmentDistanceReference,prefix##SkewAffineGridAndOptionalOutputs){LSCheck(LSGrid(fn));} \
TEST(LineSegmentDistanceReference,prefix##IndependentDirectionScales){LSCheck(LSDirectionScales(fn));} \
TEST(LineSegmentDistanceReference,prefix##ParallelParameterConvention){LSCheck(LSParallel(fn));} \
TEST(LineSegmentDistanceReference,prefix##NonparallelBelowOriginalAbsoluteThreshold){LSCheck(LSNearParallel(fn));} \
TEST(LineSegmentDistanceReference,prefix##SmallDistanceSurvivesCancellation){LSCheck(LSCancellation(fn));} \
TEST(LineSegmentDistanceReference,prefix##BothOutputsMayAliasEitherInputOrEachOther){LSCheck(LSAliases(fn));} \
TEST(LineSegmentDistanceReference,prefix##OriginalZeroDirectionConvention){LSCheck(LSZero(fn));} \
TEST(LineSegmentDistanceReference,prefix##BothEndpointBoundariesBeforeParameterRounding){LSCheck(LSBoundary(fn));} \
TEST(LineSegmentDistanceReference,prefix##IndependentNormalEquationOracle){LSCheck(LSRandom(fn));}
LS_TESTS(Scalar,LSScalar)
#if defined(VX_SIMD_SSE)
LS_TESTS(Simd,LSSimd)
#endif
#if defined(_WIN32)
TEST(LineSegmentDistanceReference,ScalarTwentyFourAndFourByteFootprints){LSCheck(LSFootprint(LSScalar));}
#if defined(VX_SIMD_SSE)
TEST(LineSegmentDistanceReference,SimdTwentyFourAndFourByteFootprints){LSCheck(LSFootprint(LSSimd));}
#endif
#endif
TEST(LineSegmentDistanceReference,PublicDistanceWrapperKeepsSquaredFloatRange) {
    LSCheck(LSGrid(VxDistance::LineSegmentDistance,true));VxRay a,b;
    a.m_Origin=VxVector(0,0,0);a.m_Direction=VxVector(1,0,0);b.m_Origin=VxVector(0,0,std::ldexp(1.0f,100));b.m_Direction=VxVector(0,1,0);
    float s,t;EXPECT_EQ(VxDistance::LineSegmentDistance(a,b,&s,&t),INFINITY);EXPECT_EQ(s,0);EXPECT_EQ(t,0);
}
#if defined(_WIN32)
TEST(LineSegmentDistanceReference,ActualCurrentDllExport) {
    HMODULE dll=LoadLibraryA("VxMath.dll");ASSERT_NE(dll,nullptr);
#if defined(_WIN64)
    const char *symbol="?LineSegmentSquareDistance@VxDistance@@SAMAEBVVxRay@@0PEAM1@Z";
#else
    const char *symbol="?LineSegmentSquareDistance@VxDistance@@SAMABVVxRay@@0PAM1@Z";
#endif
    LSFn fn=reinterpret_cast<LSFn>(GetProcAddress(dll,symbol));ASSERT_NE(fn,nullptr);
    LSCheck(LSGrid(fn));LSCheck(LSDirectionScales(fn));LSCheck(LSParallel(fn));LSCheck(LSNearParallel(fn));LSCheck(LSCancellation(fn));LSCheck(LSAliases(fn));LSCheck(LSZero(fn));LSCheck(LSBoundary(fn));LSCheck(LSRandom(fn));LSCheck(LSFootprint(fn));EXPECT_NE(FreeLibrary(dll),0);
}
#endif
#endif
