#include <cmath>
#include <cfloat>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <new>
#include <initializer_list>
#ifndef VX_RAY_RAY_NATIVE_PROBE
#ifndef VX_LIB
#define VX_LIB 1
#endif
#ifndef VX_RAY_RAY_IMPLEMENTATION
#define VX_RAY_RAY_IMPLEMENTATION "../../src/VxDistance.cpp"
#endif
#include VX_RAY_RAY_IMPLEMENTATION
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef float (*RRFn)(const VxRay &, const VxRay &, float *, float *);
struct RRSamples { int samples=0,errors=0,valueErrors=0,parameterErrors=0,inputErrors=0; unsigned digest=2166136261u; };
static bool RREqual(float a,float b) { return std::isnan(b)?std::isnan(a):a==b; }
static VxVector RRRotate(float x,float y,float z,int axis) { VxVector v;v[axis]=x;v[(axis+1)%3]=y;v[(axis+2)%3]=z;return v; }
static void RRObserve(RRSamples &r,RRFn fn,const VxVector &o0,const VxVector &u,const VxVector &o1,const VxVector &v,
                      double expectedS,double expectedT,double expectedQ,int out0=12,int out1=13,bool root=false) {
    VxRay a,b;a.m_Origin=o0;a.m_Direction=u;b.m_Origin=o1;b.m_Direction=v;
    float before[]={o0.x,o0.y,o0.z,u.x,u.y,u.z,o1.x,o1.y,o1.z,v.x,v.y,v.z,137,139};
    float *slots[]={&a.m_Origin.x,&a.m_Origin.y,&a.m_Origin.z,&a.m_Direction.x,&a.m_Direction.y,&a.m_Direction.z,
                    &b.m_Origin.x,&b.m_Origin.y,&b.m_Origin.z,&b.m_Direction.x,&b.m_Direction.y,&b.m_Direction.z,before+12,before+13};
    float *s=out0<0?nullptr:slots[out0],*t=out1<0?nullptr:slots[out1];
    const float q=float(expectedQ),expected=root?float(std::sqrt(double(q))):q;
    const float result=fn(a,b,s,t);
    bool changed=false;for(int j=0;j<12;++j)if(j!=out0&&j!=out1)changed=changed||std::memcmp(slots[j],before+j,4)!=0;
    const bool badValue=!RREqual(result,expected);
    // Native writes t0 first and t1 last, even when both pointers are identical.
    const bool badParameter=(s&&!RREqual(*s,float(s==t?expectedT:expectedS)))||(t&&!RREqual(*t,float(expectedT)));
    ++r.samples;r.valueErrors+=badValue;r.parameterErrors+=badParameter;r.inputErrors+=changed;r.errors+=badValue||badParameter||changed;
    unsigned bits;std::memcpy(&bits,&result,4);r.digest=(r.digest^bits)*16777619u;
}
struct RRExpected { double s,t,q; };
static RRExpected RRGramExpected(const VxVector &o0,const VxVector &u,const VxVector &o1,const VxVector &v) {
    double a=0,b=0,c=0,d=0,e=0,f=0;
    for(int j=0;j<3;++j) {const double w=double(o0[j])-o1[j];a+=double(u[j])*u[j];b+=double(u[j])*v[j];c+=double(v[j])*v[j];d+=w*u[j];e+=w*v[j];f+=w*w;}
    const double det=a*c-b*b;
    // Preserve the original representative when parallel rays overlap, and
    // its undefined zero-direction outputs. The nonparallel oracle enumerates
    // all active sets, independently of the production region decision tree.
    if(det==0) {
        if(d<0)return {-d/a,0,(f*a-d*d)/a};
        if(b<0)return {0,0,f};
        return {0,e/c,(f*c-e*e)/c};
    }
    RRExpected best={0,0,f};
    if(d<0) {const double q=(f*a-d*d)/a;if(q<best.q)best={-d/a,0,q};}
    if(e>0) {const double q=(f*c-e*e)/c;if(q<best.q)best={0,e/c,q};}
    const double s=(b*e-c*d)/det,t=(a*e-b*d)/det;
    if(s>=0&&t>=0) {const double q=(f*det-c*d*d+2*b*d*e-a*e*e)/det;if(q<best.q)best={s,t,q};}
    return best;
}
static RRSamples RRGrid(RRFn fn,bool root=false) {
    RRSamples r;
    for(int axis=0;axis<3;++axis)for(float s:{-4.0f,-1.0f,-.25f,0.0f,.25f,1.0f,4.0f})for(float t:{-4.0f,-1.0f,-.25f,0.0f,.25f,1.0f,4.0f})for(float h:{-2.0f,-.5f,0.0f,.5f,2.0f})for(int mask=0;mask<4;++mask) {
        // Orthogonal directions, with a common normal of squared length45.
        RRObserve(r,fn,RRRotate(-11,7,5,axis),RRRotate(1,2,2,axis),RRRotate(-11+s-2*t+2*h,7+2*s+t+4*h,5+2*s-5*h,axis),RRRotate(2,-1,0,axis),s<0?0:s,t<0?0:t,45*double(h)*h+(s<0?9*double(s)*s:0)+(t<0?5*double(t)*t:0),mask&1?12:-1,mask&2?13:-1,root);
    }return r;
}
static RRSamples RRCoupledRegions(RRFn fn) {
    RRSamples r;
    for(int axis=0;axis<3;++axis)for(int sign:{-1,1})for(float s:{-4.0f,-1.0f,-.25f,0.0f,.25f,1.0f,4.0f})for(float t:{-4.0f,-1.0f,-.25f,0.0f,.25f,1.0f,4.0f})for(float h:{0.0f,1.0f,3.0f})for(int mask=0;mask<4;++mask) {
        const VxVector o(0,0,0),u=RRRotate(1,0,0,axis),p=RRRotate(s-sign*t,-t,h,axis),v=RRRotate(float(sign),1,0,axis);
        const RRExpected e=RRGramExpected(o,u,p,v);RRObserve(r,fn,o,u,p,v,e.s,e.t,e.q,mask&1?12:-1,mask&2?13:-1);
    }return r;
}
static RRSamples RRDirectionScales(RRFn fn) {
    RRSamples r;
    for(int axis=0;axis<3;++axis)for(int eu:{-149,-100,-64,-32,0,32,64,100,126})for(int ev:{-149,-100,-64,-32,0,32,64,100,126})for(int su:{-1,1})for(int sv:{-1,1}) {
        const float u=std::ldexp(float(su),eu),v=std::ldexp(float(sv),ev);
        RRObserve(r,fn,VxVector(0,0,0),RRRotate(u,0,0,axis),RRRotate(2,-3,1,axis),RRRotate(0,v,0,axis),su<0?0:2/double(u),sv<0?0:3/double(v),1+(su<0?4:0)+(sv<0?9:0));
    }return r;
}
static RRSamples RRParallel(RRFn fn) {
    RRSamples r;
    for(int axis=0;axis<3;++axis)for(int e:{-149,-100,-64,-32,0,32,64,100,125})for(int sign:{-1,1})for(float k:{-4,-1,0,1,4})for(float h:{-1,0,1})for(int mask=0;mask<4;++mask) {
        const float scale=std::ldexp(1.0f,e);
        RRObserve(r,fn,VxVector(0,0,0),RRRotate(scale,2*scale,2*scale,axis),RRRotate(k+2*h,2*k-h,2*k,axis),RRRotate(sign*scale,2*sign*scale,2*sign*scale,axis),k>0?double(k)/scale:0,k<=0&&sign>0?-double(k)/scale:0,5*double(h)*h+(k<=0&&sign<0?9*double(k)*k:0),mask&1?12:-1,mask&2?13:-1);
    }return r;
}
static RRSamples RRNearParallel(RRFn fn) {
    RRSamples r;
    for(int axis=0;axis<3;++axis)for(int e:{-149,-100,-40,-24,-23,-13,-12,-11,-1,0,10,60,100})for(int sign:{-1,1})for(int mask=0;mask<4;++mask) {
        const float d=std::ldexp(float(sign),e);
        RRObserve(r,fn,VxVector(0,0,0),RRRotate(1,0,0,axis),RRRotate(2,1,3,axis),RRRotate(1,d,0,axis),d<0?2-1/double(d):2,d<0?-1/double(d):0,d<0?9:10,mask&1?12:-1,mask&2?13:-1);
    }return r;
}
static RRSamples RRCancellation(RRFn fn) {
    RRSamples r;
    for(int axis=0;axis<3;++axis)for(int e:{0,20,50,80,100,126})for(int sx:{-1,1})for(int sy:{-1,1})for(int mask=0;mask<4;++mask) {
        const float x=std::ldexp(float(sx),e),y=std::ldexp(float(sy),e);
        RRObserve(r,fn,VxVector(0,0,0),RRRotate(1,0,0,axis),RRRotate(x,y,1,axis),RRRotate(0,1,0,axis),x>0?x:0,y<0?-double(y):0,1+(x<0?double(x)*x:0)+(y>0?double(y)*y:0),mask&1?12:-1,mask&2?13:-1);
        RRObserve(r,fn,VxVector(0,0,0),RRRotate(3,0,0,axis),RRRotate(x,1,0,axis),RRRotate(float(sy*7),0,0,axis),x>0?double(x)/3:0,x<0&&sy>0?-double(x)/7:0,x<0&&sy<0?double(x)*x+1:1,mask&1?12:-1,mask&2?13:-1);
    }
    for(int axis=0;axis<3;++axis)RRObserve(r,fn,RRRotate(-FLT_MAX,0,0,axis),RRRotate(FLT_MAX,0,0,axis),RRRotate(FLT_MAX,0,1,axis),RRRotate(0,1,0,axis),2,0,1);
    return r;
}
static RRSamples RRAliases(RRFn fn) {
    RRSamples r;
    for(int axis=0;axis<3;++axis)for(int s=-1;s<14;++s)for(int t=-1;t<14;++t) {
        RRObserve(r,fn,VxVector(0,0,0),RRRotate(1,2,2,axis),RRRotate(1,12,1,axis),RRRotate(2,-1,0,axis),3,2,45,s,t);
        RRObserve(r,fn,VxVector(0,0,0),RRRotate(1,2,2,axis),RRRotate(-3,4,-7,axis),RRRotate(2,-1,0,axis),0,2,54,s,t);
        RRObserve(r,fn,VxVector(0,0,0),RRRotate(1,2,2,axis),RRRotate(9,8,1,axis),RRRotate(2,-1,0,axis),3,0,65,s,t);
        RRObserve(r,fn,VxVector(0,0,0),RRRotate(1,2,2,axis),RRRotate(5,0,-7,axis),RRRotate(2,-1,0,axis),0,0,74,s,t);
        RRObserve(r,fn,VxVector(0,0,0),RRRotate(1,2,2,axis),RRRotate(3,1,2,axis),RRRotate(2,4,4,axis),1,0,5,s,t);
        RRObserve(r,fn,VxVector(0,0,0),RRRotate(1,2,2,axis),RRRotate(-3,-1,-2,axis),RRRotate(2,4,4,axis),0,.5,5,s,t);
        RRObserve(r,fn,VxVector(0,0,0),RRRotate(1,2,2,axis),RRRotate(-3,-1,-2,axis),RRRotate(-2,-4,-4,axis),0,0,14,s,t);
        RRObserve(r,fn,VxVector(0,0,0),RRRotate(1,0,0,axis),RRRotate(1,-2,1,axis),RRRotate(-1,1,0,axis),0,1.5,1.5,s,t);
    }return r;
}
static RRSamples RRZero(RRFn fn) {
    RRSamples r;
    for(int axis=0;axis<3;++axis)for(int mask=0;mask<4;++mask) {
        RRObserve(r,fn,VxVector(0,0,0),VxVector(0,0,0),RRRotate(4,3,4,axis),RRRotate(1,2,2,axis),0,-2,5,mask&1?12:-1,mask&2?13:-1);
        RRObserve(r,fn,VxVector(0,0,0),VxVector(0,0,0),RRRotate(-4,-3,-4,axis),RRRotate(1,2,2,axis),0,2,5,mask&1?12:-1,mask&2?13:-1);
        RRObserve(r,fn,VxVector(0,0,0),RRRotate(1,2,2,axis),RRRotate(4,3,4,axis),VxVector(0,0,0),2,0,5,mask&1?12:-1,mask&2?13:-1);
        RRObserve(r,fn,VxVector(0,0,0),RRRotate(1,2,2,axis),RRRotate(-4,-3,-4,axis),VxVector(0,0,0),0,NAN,NAN,mask&1?12:-1,mask&2?13:-1);
        RRObserve(r,fn,VxVector(0,0,0),VxVector(0,0,0),RRRotate(4,3,4,axis),VxVector(0,0,0),0,NAN,NAN,mask&1?12:-1,mask&2?13:-1);
        RRObserve(r,fn,VxVector(0,0,0),VxVector(0,0,0),VxVector(0,0,0),RRRotate(1,2,2,axis),0,0,0,mask&1?12:-1,mask&2?13:-1);
        RRObserve(r,fn,VxVector(0,0,0),RRRotate(1,2,2,axis),VxVector(0,0,0),VxVector(0,0,0),0,NAN,NAN,mask&1?12:-1,mask&2?13:-1);
    }return r;
}
static RRSamples RRBoundary(RRFn fn) {
    RRSamples r;
    for(int axis=0;axis<3;++axis)for(int ev:{-126,-100,-1,0,1,100,126})for(int ey:{-149,-126,-70,-1,0,70,126})for(int sx:{-1,1})for(int sy:{-1,1})for(int mask=0;mask<4;++mask) {
        const float v=std::ldexp(1.0f,ev),x=std::ldexp(float(sx),ey),y=std::ldexp(float(sy),ey);
        RRObserve(r,fn,VxVector(0,0,0),RRRotate(v,0,0,axis),RRRotate(x,y,0,axis),RRRotate(0,v,0,axis),sx>0?double(x)/v:0,sy<0?-double(y)/v:0,(sx<0?double(x)*x:0)+(sy>0?double(y)*y:0),mask&1?12:-1,mask&2?13:-1);
    }return r;
}
static unsigned RRNext(unsigned &seed) {seed=seed*1664525u+1013904223u;return seed;}
static RRSamples RRRandom(RRFn fn) {
    RRSamples r;unsigned seed=283;
    while(r.samples<4000) {
        VxVector o0,u,o1,v;for(int j=0;j<3;++j){o0[j]=float(int(RRNext(seed)%33)-16);o1[j]=float(int(RRNext(seed)%33)-16);u[j]=float(int(RRNext(seed)%11)-5);v[j]=float(int(RRNext(seed)%11)-5);}
        const RRExpected e=RRGramExpected(o0,u,o1,v);RRObserve(r,fn,o0,u,o1,v,e.s,e.t,e.q,r.samples&1?12:-1,r.samples&2?13:-1);
    }return r;
}
#if defined(_WIN32)
static RRSamples RRFootprint(RRFn fn) {
    RRSamples r;SYSTEM_INFO si;GetSystemInfo(&si);const size_t page=si.dwPageSize;
    char *pages=static_cast<char*>(VirtualAlloc(nullptr,page*8,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));if(!pages){r.errors=1;return r;}
    VxRay *a=new(pages+page-24)VxRay,*b=new(pages+page*3-24)VxRay;
    a->m_Origin=VxVector(0,0,0);a->m_Direction=VxVector(1,0,0);b->m_Origin=VxVector(2,-3,4);b->m_Direction=VxVector(0,1,0);
    float *s=new(pages+page*5-4)float(137),*t=new(pages+page*7-4)float(139);
    DWORD old;bool good=true;for(int i=0;i<4;++i)good=VirtualProtect(pages+page*(2*i+1),page,PAGE_NOACCESS,&old)&&good;
    for(int i=0;i<2;++i)good=VirtualProtect(pages+page*2*i,page,PAGE_READONLY,&old)&&good;
    if(good){r.samples=2;r.errors=(fn(*a,*b,s,t)!=16||*s!=2||*t!=3)+(fn(*a,*b,nullptr,nullptr)!=16);}else r.errors=1;
    VirtualFree(pages,0,MEM_RELEASE);return r;
}
#endif
#ifndef VX_RAY_RAY_NATIVE_PROBE
static void RRCheck(RRSamples r){EXPECT_EQ(r.errors,0)<<"values="<<r.valueErrors<<" parameters="<<r.parameterErrors<<" changed inputs="<<r.inputErrors;::testing::Test::RecordProperty("samples",r.samples);}
static float RRScalar(const VxRay &a,const VxRay &b,float *s,float *t){g_VxDistanceDispatch.store(&kVxDistanceDispatchScalar,std::memory_order_release);return VxDistance::RayRaySquareDistance(a,b,s,t);}
#if defined(VX_SIMD_SSE)
static float RRSimd(const VxRay &a,const VxRay &b,float *s,float *t){g_VxDistanceDispatch.store(&kVxDistanceDispatchSIMD,std::memory_order_release);return VxDistance::RayRaySquareDistance(a,b,s,t);}
#endif
#define RR_TESTS(prefix,fn) \
TEST(RayRayDistanceReference,prefix##AllOrthogonalActiveSets){RRCheck(RRGrid(fn));} \
TEST(RayRayDistanceReference,prefix##CoupledRegionsMatchIndependentActiveSetOracle){RRCheck(RRCoupledRegions(fn));} \
TEST(RayRayDistanceReference,prefix##IndependentDirectionScales){RRCheck(RRDirectionScales(fn));} \
TEST(RayRayDistanceReference,prefix##SameAndOpposedParallelRepresentatives){RRCheck(RRParallel(fn));} \
TEST(RayRayDistanceReference,prefix##NonparallelBelowOriginalThreshold){RRCheck(RRNearParallel(fn));} \
TEST(RayRayDistanceReference,prefix##SmallDistancesAndOpposingOriginExtremes){RRCheck(RRCancellation(fn));} \
TEST(RayRayDistanceReference,prefix##TwoOutputsMayAliasInputsOrEachOther){RRCheck(RRAliases(fn));} \
TEST(RayRayDistanceReference,prefix##OriginalUndefinedZeroDirectionConvention){RRCheck(RRZero(fn));} \
TEST(RayRayDistanceReference,prefix##BothOriginsBeforeFloatParameterRounding){RRCheck(RRBoundary(fn));} \
TEST(RayRayDistanceReference,prefix##IndependentNormalEquationCandidates){RRCheck(RRRandom(fn));}
RR_TESTS(Scalar,RRScalar)
#if defined(VX_SIMD_SSE)
RR_TESTS(Simd,RRSimd)
#endif
#if defined(_WIN32)
TEST(RayRayDistanceReference,ScalarTwentyFourAndFourByteFootprints){RRCheck(RRFootprint(RRScalar));}
#if defined(VX_SIMD_SSE)
TEST(RayRayDistanceReference,SimdTwentyFourAndFourByteFootprints){RRCheck(RRFootprint(RRSimd));}
#endif
#endif
TEST(RayRayDistanceReference,DistanceWrapperKeepsSquaredFloatRange){RRCheck(RRGrid(VxDistance::RayRayDistance,true));VxRay a,b;a.m_Origin=VxVector(0,0,0);a.m_Direction=VxVector(1,0,0);b.m_Origin=VxVector(0,0,std::ldexp(1.0f,100));b.m_Direction=VxVector(0,1,0);float s,t;EXPECT_EQ(VxDistance::RayRayDistance(a,b,&s,&t),INFINITY);EXPECT_EQ(s,0);EXPECT_EQ(t,0);}
#if defined(_WIN32)
TEST(RayRayDistanceReference,ActualCurrentDllExport){HMODULE dll=LoadLibraryA("VxMath.dll");ASSERT_NE(dll,nullptr);
#if defined(_WIN64)
    const char *symbol="?RayRaySquareDistance@VxDistance@@SAMAEBVVxRay@@0PEAM1@Z";
#else
    const char *symbol="?RayRaySquareDistance@VxDistance@@SAMABVVxRay@@0PAM1@Z";
#endif
    RRFn fn=reinterpret_cast<RRFn>(GetProcAddress(dll,symbol));ASSERT_NE(fn,nullptr);RRCheck(RRGrid(fn));RRCheck(RRCoupledRegions(fn));RRCheck(RRDirectionScales(fn));RRCheck(RRParallel(fn));RRCheck(RRNearParallel(fn));RRCheck(RRCancellation(fn));RRCheck(RRAliases(fn));RRCheck(RRZero(fn));RRCheck(RRBoundary(fn));RRCheck(RRRandom(fn));RRCheck(RRFootprint(fn));EXPECT_NE(FreeLibrary(dll),0);}
#endif
#endif
