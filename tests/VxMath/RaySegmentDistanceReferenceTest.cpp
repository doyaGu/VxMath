#include <cmath>
#include <cfloat>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <new>
#include <initializer_list>
#ifndef VX_RAY_SEGMENT_NATIVE_PROBE
#ifndef VX_LIB
#define VX_LIB 1
#endif
#ifndef VX_RAY_SEGMENT_IMPLEMENTATION
#define VX_RAY_SEGMENT_IMPLEMENTATION "../../src/VxDistance.cpp"
#endif
#include VX_RAY_SEGMENT_IMPLEMENTATION
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef float (*RSFn)(const VxRay &, const VxRay &, float *, float *);
struct RSSamples { int samples=0,errors=0,valueErrors=0,parameterErrors=0,inputErrors=0; unsigned digest=2166136261u; };
static bool RSEqual(float a,float b) { return std::isnan(b)?std::isnan(a):a==b; }
static VxVector RSRotate(float x,float y,float z,int axis) { VxVector v;v[axis]=x;v[(axis+1)%3]=y;v[(axis+2)%3]=z;return v; }
static void RSObserve(RSSamples &r,RSFn fn,const VxVector &o0,const VxVector &u,const VxVector &o1,const VxVector &v,
                      double expectedS,double expectedT,double expectedQ,int out0=12,int out1=13,bool root=false) {
    VxRay a,b;a.m_Origin=o0;a.m_Direction=u;b.m_Origin=o1;b.m_Direction=v;
    float before[]={o0.x,o0.y,o0.z,u.x,u.y,u.z,o1.x,o1.y,o1.z,v.x,v.y,v.z,137,139};
    float *slots[]={&a.m_Origin.x,&a.m_Origin.y,&a.m_Origin.z,&a.m_Direction.x,&a.m_Direction.y,&a.m_Direction.z,
                    &b.m_Origin.x,&b.m_Origin.y,&b.m_Origin.z,&b.m_Direction.x,&b.m_Direction.y,&b.m_Direction.z,before+12,before+13};
    float *s=out0<0?nullptr:slots[out0],*t=out1<0?nullptr:slots[out1];
    const float q=float(expectedQ),expected=root?float(std::sqrt(double(q))):q;
    const float result=fn(a,b,s,t);
    bool changed=false;for(int j=0;j<12;++j)if(j!=out0&&j!=out1)changed=changed||std::memcmp(slots[j],before+j,4)!=0;
    const bool badValue=!RSEqual(result,expected);
    // Native writes t0 first and t1 last, even when both pointers are identical.
    const bool badParameter=(s&&!RSEqual(*s,float(s==t?expectedT:expectedS)))||(t&&!RSEqual(*t,float(expectedT)));
    ++r.samples;r.valueErrors+=badValue;r.parameterErrors+=badParameter;r.inputErrors+=changed;r.errors+=badValue||badParameter||changed;
    unsigned bits;std::memcpy(&bits,&result,4);r.digest=(r.digest^bits)*16777619u;
}
struct RSExpected {double s,t,q;};
static RSExpected RSGramExpected(const VxVector &o0,const VxVector &u,const VxVector &o1,const VxVector &v) {
    double a=0,b=0,c=0,d=0,e=0,f=0;
    for(int j=0;j<3;++j){const double w=double(o0[j])-o1[j];a+=double(u[j])*u[j];b+=double(u[j])*v[j];c+=double(v[j])*v[j];d+=w*u[j];e+=w*v[j];f+=w*w;}
    const double det=a*c-b*b;
    if(det==0){const double t=b>=0?1:0,g=d-b*t,z=f-2*e*t+c*t*t;return {g<0?-g/a:0,t,g<0?(z*a-g*g)/a:z};}
    // Enumerate all boundary and interior candidates, independently of the
    // production region tree. These workloads use exact small dyadic inputs.
    RSExpected best={d<0?-d/a:0,0,d<0?(f*a-d*d)/a:f};
    const double g=d-b,z=f-2*e+c,qEnd=g<0?(z*a-g*g)/a:z;
    if(qEnd<best.q)best={g<0?-g/a:0,1,qEnd};
    const double t=e<=0?0:e>=c?1:e/c,qPoint=e<=0?f:e>=c?z:(f*c-e*e)/c;
    if(qPoint<best.q)best={0,t,qPoint};
    const double first=(b*e-c*d)/det,second=(a*e-b*d)/det;
    if(first>=0&&second>=0&&second<=1){const double q=(f*det-c*d*d+2*b*d*e-a*e*e)/det;if(q<best.q)best={first,second,q};}
    return best;
}
static RSSamples RSGrid(RSFn fn,bool root=false) {
    RSSamples r;
    for(int axis=0;axis<3;++axis)for(float s:{-4.0f,-1.0f,-.25f,0.0f,.25f,1.0f,4.0f})for(float t:{-2.0f,-.25f,0.0f,.25f,.5f,.75f,1.0f,1.25f,4.0f})for(float h:{-2.0f,-.5f,0.0f,.5f,2.0f})for(int mask=0;mask<4;++mask){
        const double clamped=t<0?0:t>1?1:t;
        RSObserve(r,fn,RSRotate(-11,7,5,axis),RSRotate(1,2,2,axis),RSRotate(-11+s-2*t+2*h,7+2*s+t+4*h,5+2*s-5*h,axis),RSRotate(2,-1,0,axis),s<0?0:s,clamped,45*double(h)*h+(s<0?9*double(s)*s:0)+5*std::pow(double(t)-clamped,2),mask&1?12:-1,mask&2?13:-1,root);
    }return r;
}
static RSSamples RSCoupledRegions(RSFn fn) {
    RSSamples r;
    for(int axis=0;axis<3;++axis)for(int sign:{-1,1})for(float s:{-4.0f,-1.0f,-.25f,0.0f,.25f,1.0f,4.0f})for(float t:{-2.0f,-.25f,0.0f,.25f,.5f,.75f,1.0f,1.25f,4.0f})for(float h:{0.0f,1.0f,3.0f})for(int mask=0;mask<4;++mask){
        const VxVector o(0,0,0),u=RSRotate(1,0,0,axis),p=RSRotate(s-sign*t,-t,h,axis),v=RSRotate(float(sign),1,0,axis);
        const RSExpected e=RSGramExpected(o,u,p,v);RSObserve(r,fn,o,u,p,v,e.s,e.t,e.q,mask&1?12:-1,mask&2?13:-1);
    }return r;
}
static RSSamples RSDirectionScales(RSFn fn) {
    RSSamples r;
    for(int axis=0;axis<3;++axis)for(int eu:{-149,-100,-64,-32,0,32,64,100,126})for(int ev:{-149,-100,-64,-32,0,32,64,100,126})for(int su:{-1,1})for(int sv:{-1,1}){
        const float u=std::ldexp(float(su),eu),v=std::ldexp(float(sv),ev);
        RSObserve(r,fn,VxVector(0,0,0),RSRotate(u,0,0,axis),RSRotate(2,-3,1,axis),RSRotate(0,v,0,axis),su<0?0:2/double(u),sv<0?0:v<3?1:3/double(v),1+(su<0?4:0)+(sv<0?9:v<3?std::pow(3-double(v),2):0));
    }return r;
}
static RSSamples RSParallel(RSFn fn) {
    RSSamples r;
    for(int axis=0;axis<3;++axis)for(int e:{-149,-100,-64,-32,0,32,64,100,125})for(int sign:{-1,1})for(float k:{-4,-1,0,1,4})for(float h:{-1,0,1})for(int mask=0;mask<4;++mask){
        const float scale=std::ldexp(1.0f,e);const double along=sign>0?double(k)+scale:k;
        RSObserve(r,fn,VxVector(0,0,0),RSRotate(scale,2*scale,2*scale,axis),RSRotate(k+2*h,2*k-h,2*k,axis),RSRotate(sign*scale,2*sign*scale,2*sign*scale,axis),along>0?along/scale:0,sign>0?1:0,5*double(h)*h+(along<0?9*along*along:0),mask&1?12:-1,mask&2?13:-1);
    }return r;
}
static RSSamples RSNearParallel(RSFn fn) {
    RSSamples r;
    for(int axis=0;axis<3;++axis)for(int e:{-149,-100,-40,-24,-23,-13,-12,-11,-1,0,10,60,100})for(int sign:{-1,1})for(int mask=0;mask<4;++mask){
        const float d=std::ldexp(float(sign),e);
        RSObserve(r,fn,VxVector(0,0,0),RSRotate(1,0,0,axis),RSRotate(2,1,3,axis),RSRotate(1,d,0,axis),d>=0?2:d> -1?3:2-1/double(d),d>=0?0:d> -1?1:-1/double(d),d>=0?10:d> -1?9+std::pow(1+double(d),2):9,mask&1?12:-1,mask&2?13:-1);
    }return r;
}
static RSSamples RSCancellation(RSFn fn) {
    RSSamples r;
    for(int axis=0;axis<3;++axis)for(int e:{0,20,50,80,100,126})for(int sx:{-1,1})for(int sy:{-1,1})for(int mask=0;mask<4;++mask){
        const float x=std::ldexp(float(sx),e),y=std::ldexp(float(sy),e);const double end=double(x)+7;
        RSObserve(r,fn,VxVector(0,0,0),RSRotate(1,0,0,axis),RSRotate(x,y,1,axis),RSRotate(0,1,0,axis),x>0?x:0,y>0?0:y< -1?1:-double(y),1+(x<0?double(x)*x:0)+(y>0?double(y)*y:y< -1?std::pow(double(y)+1,2):0),mask&1?12:-1,mask&2?13:-1);
        RSObserve(r,fn,VxVector(0,0,0),RSRotate(3,0,0,axis),RSRotate(x,1,0,axis),RSRotate(float(sy*7),0,0,axis),sy>0?(end>0?end/3:0):(x>0?double(x)/3:0),sy>0?1:0,1+(sy>0?(end<0?end*end:0):(x<0?double(x)*x:0)),mask&1?12:-1,mask&2?13:-1);
    }
    for(int axis=0;axis<3;++axis){
        RSObserve(r,fn,RSRotate(-FLT_MAX,0,0,axis),RSRotate(FLT_MAX,0,0,axis),RSRotate(FLT_MAX,0,1,axis),RSRotate(0,1,0,axis),2,0,1);
        for(int e:{20,50,60,100,126}){
            const float large=std::ldexp(1.0f,e);
            for(int sign:{-1,1}){
                RSObserve(r,fn,RSRotate(0,float(sign),0,axis),RSRotate(1,0,0,axis),RSRotate(0,large,0,axis),RSRotate(0,-large,0,axis),0,sign<0?1:1-1/double(large),sign<0?1:0);
                // Parallel endpoint is zero; the ray begins one unit on
                // either side. Projection must retain that one-unit offset.
                RSObserve(r,fn,RSRotate(0,float(sign),0,axis),RSRotate(0,large,0,axis),RSRotate(0,-large,0,axis),RSRotate(0,large,0,axis),sign<0?1/double(large):0,1,sign>0?1:0);
            }
            RSObserve(r,fn,RSRotate(0,large,-3,axis),RSRotate(0,1,1,axis),RSRotate(0,large,0,axis),RSRotate(0,1,0,axis),2,1,2);
        }
    }return r;
}
static RSSamples RSAliases(RSFn fn) {
    RSSamples r;
    for(int axis=0;axis<3;++axis)for(int s=-1;s<14;++s)for(int t=-1;t<14;++t){
        RSObserve(r,fn,VxVector(0,0,0),RSRotate(1,2,2,axis),RSRotate(4,10.5f,1,axis),RSRotate(2,-1,0,axis),3,.5,45,s,t);
        RSObserve(r,fn,VxVector(0,0,0),RSRotate(1,2,2,axis),RSRotate(0,2.5f,-7,axis),RSRotate(2,-1,0,axis),0,.5,54,s,t);
        RSObserve(r,fn,VxVector(0,0,0),RSRotate(1,2,2,axis),RSRotate(9,8,1,axis),RSRotate(2,-1,0,axis),3,0,65,s,t);
        RSObserve(r,fn,VxVector(0,0,0),RSRotate(1,2,2,axis),RSRotate(1,12,1,axis),RSRotate(2,-1,0,axis),3,1,50,s,t);
        RSObserve(r,fn,VxVector(0,0,0),RSRotate(1,2,2,axis),RSRotate(5,0,-7,axis),RSRotate(2,-1,0,axis),0,0,74,s,t);
        RSObserve(r,fn,VxVector(0,0,0),RSRotate(1,2,2,axis),RSRotate(-3,4,-7,axis),RSRotate(2,-1,0,axis),0,1,59,s,t);
        RSObserve(r,fn,VxVector(0,0,0),RSRotate(1,2,2,axis),RSRotate(3,1,2,axis),RSRotate(2,4,4,axis),3,1,5,s,t);
        RSObserve(r,fn,VxVector(0,0,0),RSRotate(1,2,2,axis),RSRotate(-3,-1,-2,axis),RSRotate(-2,-4,-4,axis),0,0,14,s,t);
        RSObserve(r,fn,VxVector(0,0,0),RSRotate(1,0,0,axis),RSRotate(-1,-2,1,axis),RSRotate(-1,1,0,axis),0,.5,5.5,s,t);
        RSObserve(r,fn,VxVector(0,0,0),RSRotate(1,0,0,axis),RSRotate(-2,1,1,axis),RSRotate(1,1,0,axis),0,.5,5.5,s,t);
    }return r;
}
static RSSamples RSZero(RSFn fn) {
    RSSamples r;
    for(int axis=0;axis<3;++axis)for(int mask=0;mask<4;++mask){
        RSObserve(r,fn,VxVector(0,0,0),VxVector(0,0,0),RSRotate(4,3,4,axis),RSRotate(1,2,2,axis),0,1,86,mask&1?12:-1,mask&2?13:-1);
        RSObserve(r,fn,VxVector(0,0,0),VxVector(0,0,0),RSRotate(-4,-3,-4,axis),RSRotate(1,2,2,axis),0,1,14,mask&1?12:-1,mask&2?13:-1);
        RSObserve(r,fn,VxVector(0,0,0),RSRotate(1,2,2,axis),RSRotate(4,3,4,axis),VxVector(0,0,0),2,1,5,mask&1?12:-1,mask&2?13:-1);
        RSObserve(r,fn,VxVector(0,0,0),RSRotate(1,2,2,axis),RSRotate(-4,-3,-4,axis),VxVector(0,0,0),0,1,41,mask&1?12:-1,mask&2?13:-1);
        RSObserve(r,fn,VxVector(0,0,0),VxVector(0,0,0),RSRotate(4,3,4,axis),VxVector(0,0,0),0,1,41,mask&1?12:-1,mask&2?13:-1);
        RSObserve(r,fn,VxVector(0,0,0),VxVector(0,0,0),VxVector(0,0,0),RSRotate(1,2,2,axis),0,1,9,mask&1?12:-1,mask&2?13:-1);
        RSObserve(r,fn,VxVector(0,0,0),RSRotate(1,2,2,axis),VxVector(0,0,0),VxVector(0,0,0),0,1,0,mask&1?12:-1,mask&2?13:-1);
    }return r;
}
static RSSamples RSBoundary(RSFn fn) {
    RSSamples r;
    for(int axis=0;axis<3;++axis)for(int ev:{-126,-100,-1,0,1,100,126})for(int ey:{-149,-126,-70,-1,0,70,126})for(int sx:{-1,1})for(int sy:{-1,1})for(int mask=0;mask<4;++mask){
        const float v=std::ldexp(1.0f,ev),x=std::ldexp(float(sx),ey),y=std::ldexp(float(sy),ey);
        RSObserve(r,fn,VxVector(0,0,0),RSRotate(v,0,0,axis),RSRotate(x,y,0,axis),RSRotate(0,v,0,axis),sx>0?double(x)/v:0,sy>0?0:-double(y)>v?1:-double(y)/v,(sx<0?double(x)*x:0)+(sy>0?double(y)*y:-double(y)>v?std::pow(double(y)+v,2):0),mask&1?12:-1,mask&2?13:-1);
    }
    for(int axis=0;axis<3;++axis)for(int ev:{-100,-20,0,20,100})for(int sign:{-1,1})for(float t:{.5f,std::nextafter(1.0f,0.0f),1.0f,std::nextafter(1.0f,INFINITY),2.0f})for(int mask=0;mask<4;++mask){
        const float v=std::ldexp(1.0f,ev);
        RSObserve(r,fn,VxVector(0,0,0),RSRotate(1,0,0,axis),RSRotate(float(2*sign),-v*t,0,axis),RSRotate(0,v,0,axis),sign>0?2:0,t>1?1:t,(sign<0?4:0)+(t>1?std::pow(double(v)*(double(t)-1),2):0),mask&1?12:-1,mask&2?13:-1);
    }return r;
}
static unsigned RSNext(unsigned &seed){seed=seed*1664525u+1013904223u;return seed;}
static RSSamples RSRandom(RSFn fn) {
    RSSamples r;unsigned seed=284;
    while(r.samples<4000){VxVector o,u,p,v;for(int j=0;j<3;++j){o[j]=float(int(RSNext(seed)%33)-16);p[j]=float(int(RSNext(seed)%33)-16);u[j]=float(int(RSNext(seed)%11)-5);v[j]=float(int(RSNext(seed)%11)-5);}const RSExpected e=RSGramExpected(o,u,p,v);RSObserve(r,fn,o,u,p,v,e.s,e.t,e.q,r.samples&1?12:-1,r.samples&2?13:-1);}
    return r;
}
#if defined(_WIN32)
static RSSamples RSFootprint(RSFn fn) {
    RSSamples r;SYSTEM_INFO si;GetSystemInfo(&si);const size_t page=si.dwPageSize;
    char *pages=static_cast<char*>(VirtualAlloc(nullptr,page*8,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));if(!pages){r.errors=1;return r;}
    VxRay *a=new(pages+page-24)VxRay,*b=new(pages+page*3-24)VxRay;
    a->m_Origin=VxVector(0,0,0);a->m_Direction=VxVector(1,0,0);b->m_Origin=VxVector(2,-.5f,4);b->m_Direction=VxVector(0,1,0);
    float *s=new(pages+page*5-4)float(137),*t=new(pages+page*7-4)float(139);
    DWORD old;bool good=true;for(int i=0;i<4;++i)good=VirtualProtect(pages+page*(2*i+1),page,PAGE_NOACCESS,&old)&&good;
    for(int i=0;i<2;++i)good=VirtualProtect(pages+page*2*i,page,PAGE_READONLY,&old)&&good;
    if(good){r.samples=2;r.errors=(fn(*a,*b,s,t)!=16||*s!=2||*t!=.5f)+(fn(*a,*b,nullptr,nullptr)!=16);}else r.errors=1;
    VirtualFree(pages,0,MEM_RELEASE);return r;
}
#endif
#ifndef VX_RAY_SEGMENT_NATIVE_PROBE
static void RSCheck(RSSamples r){EXPECT_EQ(r.errors,0)<<"values="<<r.valueErrors<<" parameters="<<r.parameterErrors<<" changed inputs="<<r.inputErrors;::testing::Test::RecordProperty("samples",r.samples);}
static float RSScalar(const VxRay &a,const VxRay &b,float *s,float *t){g_VxDistanceDispatch.store(&kVxDistanceDispatchScalar,std::memory_order_release);return VxDistance::RaySegmentSquareDistance(a,b,s,t);}
#if defined(VX_SIMD_SSE)
static float RSSimd(const VxRay &a,const VxRay &b,float *s,float *t){g_VxDistanceDispatch.store(&kVxDistanceDispatchSIMD,std::memory_order_release);return VxDistance::RaySegmentSquareDistance(a,b,s,t);}
#endif
#define RS_TESTS(prefix,fn) \
TEST(RaySegmentDistanceReference,prefix##OrthogonalRayAndBothSegmentEnds){RSCheck(RSGrid(fn));} \
TEST(RaySegmentDistanceReference,prefix##CoupledRegionsMatchIndependentCandidates){RSCheck(RSCoupledRegions(fn));} \
TEST(RaySegmentDistanceReference,prefix##IndependentDirectionScales){RSCheck(RSDirectionScales(fn));} \
TEST(RaySegmentDistanceReference,prefix##ParallelEndpointRepresentatives){RSCheck(RSParallel(fn));} \
TEST(RaySegmentDistanceReference,prefix##NonparallelBelowOriginalThreshold){RSCheck(RSNearParallel(fn));} \
TEST(RaySegmentDistanceReference,prefix##SmallDistancesAndEndpointCancellation){RSCheck(RSCancellation(fn));} \
TEST(RaySegmentDistanceReference,prefix##TwoOutputsMayAliasAnyInputOrEachOther){RSCheck(RSAliases(fn));} \
TEST(RaySegmentDistanceReference,prefix##OriginalZeroDirectionEndpointSelection){RSCheck(RSZero(fn));} \
TEST(RaySegmentDistanceReference,prefix##ThreeBoundariesBeforeFloatParameterRounding){RSCheck(RSBoundary(fn));} \
TEST(RaySegmentDistanceReference,prefix##IndependentConstrainedNormalEquationCandidates){RSCheck(RSRandom(fn));}
RS_TESTS(Scalar,RSScalar)
#if defined(VX_SIMD_SSE)
RS_TESTS(Simd,RSSimd)
#endif
#if defined(_WIN32)
TEST(RaySegmentDistanceReference,ScalarTwentyFourAndFourByteFootprints){RSCheck(RSFootprint(RSScalar));}
#if defined(VX_SIMD_SSE)
TEST(RaySegmentDistanceReference,SimdTwentyFourAndFourByteFootprints){RSCheck(RSFootprint(RSSimd));}
#endif
#endif
TEST(RaySegmentDistanceReference,WrapperKeepsSquaredFloatRange){RSCheck(RSGrid(VxDistance::RaySegmentDistance,true));VxRay a,b;a.m_Origin=VxVector(0,0,0);a.m_Direction=VxVector(1,0,0);b.m_Origin=VxVector(0,0,std::ldexp(1.0f,100));b.m_Direction=VxVector(0,1,0);float s,t;EXPECT_EQ(VxDistance::RaySegmentDistance(a,b,&s,&t),INFINITY);EXPECT_EQ(s,0);EXPECT_EQ(t,0);}
#if defined(_WIN32)
TEST(RaySegmentDistanceReference,ActualCurrentDllExport){HMODULE dll=LoadLibraryA("VxMath.dll");ASSERT_NE(dll,nullptr);
#if defined(_WIN64)
    const char *symbol="?RaySegmentSquareDistance@VxDistance@@SAMAEBVVxRay@@0PEAM1@Z";
#else
    const char *symbol="?RaySegmentSquareDistance@VxDistance@@SAMABVVxRay@@0PAM1@Z";
#endif
    RSFn fn=reinterpret_cast<RSFn>(GetProcAddress(dll,symbol));ASSERT_NE(fn,nullptr);RSCheck(RSGrid(fn));RSCheck(RSCoupledRegions(fn));RSCheck(RSDirectionScales(fn));RSCheck(RSParallel(fn));RSCheck(RSNearParallel(fn));RSCheck(RSCancellation(fn));RSCheck(RSAliases(fn));RSCheck(RSZero(fn));RSCheck(RSBoundary(fn));RSCheck(RSRandom(fn));RSCheck(RSFootprint(fn));EXPECT_NE(FreeLibrary(dll),0);}
#endif
#endif
