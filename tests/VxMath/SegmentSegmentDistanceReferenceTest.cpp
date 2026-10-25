#include <cmath>
#include <cfloat>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <new>
#include <initializer_list>
#ifndef VX_SEGMENT_SEGMENT_NATIVE_PROBE
#ifndef VX_LIB
#define VX_LIB 1
#endif
#ifndef VX_SEGMENT_SEGMENT_IMPLEMENTATION
#define VX_SEGMENT_SEGMENT_IMPLEMENTATION "../../src/VxDistance.cpp"
#endif
#include VX_SEGMENT_SEGMENT_IMPLEMENTATION
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef float (*SSFn)(const VxRay &, const VxRay &, float *, float *);
struct SSSamples { int samples=0,errors=0,valueErrors=0,parameterErrors=0,inputErrors=0; unsigned digest=2166136261u; };
static bool SSEqual(float a,float b) { return std::isnan(b)?std::isnan(a):a==b; }
static VxVector SSRotate(float x,float y,float z,int axis) { VxVector v;v[axis]=x;v[(axis+1)%3]=y;v[(axis+2)%3]=z;return v; }
static void SSObserve(SSSamples &r,SSFn fn,const VxVector &o0,const VxVector &u,const VxVector &o1,const VxVector &v,
                      double expectedS,double expectedT,double expectedQ,int out0=12,int out1=13,bool root=false) {
    VxRay a,b;a.m_Origin=o0;a.m_Direction=u;b.m_Origin=o1;b.m_Direction=v;
    float before[]={o0.x,o0.y,o0.z,u.x,u.y,u.z,o1.x,o1.y,o1.z,v.x,v.y,v.z,137,139};
    float *slots[]={&a.m_Origin.x,&a.m_Origin.y,&a.m_Origin.z,&a.m_Direction.x,&a.m_Direction.y,&a.m_Direction.z,
                    &b.m_Origin.x,&b.m_Origin.y,&b.m_Origin.z,&b.m_Direction.x,&b.m_Direction.y,&b.m_Direction.z,before+12,before+13};
    float *s=out0<0?nullptr:slots[out0],*t=out1<0?nullptr:slots[out1];
    const float q=float(expectedQ),expected=root?float(std::sqrt(double(q))):q;
    const float result=fn(a,b,s,t);
    bool changed=false;for(int j=0;j<12;++j)if(j!=out0&&j!=out1)changed=changed||std::memcmp(slots[j],before+j,4)!=0;
    const bool badValue=!SSEqual(result,expected);
    // Native writes t0 first and t1 last, even when both pointers are identical.
    const bool badParameter=(s&&!SSEqual(*s,float(s==t?expectedT:expectedS)))||(t&&!SSEqual(*t,float(expectedT)));
    if ((badValue||badParameter) && r.errors<2) std::fprintf(stderr,"mismatch q=%g/%g s=%g/%g t=%g/%g O=(%g,%g,%g) U=(%g,%g,%g) P=(%g,%g,%g) V=(%g,%g,%g)\n",result,expected,s?*s:0,float(expectedS),t?*t:0,float(expectedT),o0.x,o0.y,o0.z,u.x,u.y,u.z,o1.x,o1.y,o1.z,v.x,v.y,v.z);
    ++r.samples;r.valueErrors+=badValue;r.parameterErrors+=badParameter;r.inputErrors+=changed;r.errors+=badValue||badParameter||changed;
    unsigned bits;std::memcpy(&bits,&result,4);r.digest=(r.digest^bits)*16777619u;
}

struct SSExpected {double s,t,q;};
static double SSClamp(double v) {return v<0?0:v>1?1:v;}
static SSExpected SSGramExpected(const VxVector &o,const VxVector &u,const VxVector &p,const VxVector &v) {
    double a=0,b=0,c=0,d=0,e=0,f=0;
    for(int j=0;j<3;++j){const double w=double(o[j])-p[j];a+=double(u[j])*u[j];b+=double(u[j])*v[j];c+=double(v[j])*v[j];d+=w*u[j];e+=w*v[j];f+=w*w;}
    const double det=a*c-b*b;
    if(det==0){
        // Native parallel overlap representative, not the old fallback order.
        double s,t;
        if(b<0){if(d>=0){s=t=0;}else if(-d<=a){s=-d/a;t=0;}else{s=1;t=SSClamp((d+a)/b);}}
        else if(-d>=a){s=1;t=0;}
        else if(d<=0){s=-d/a;t=0;}
        else{s=0;t=b<=d?1:d/b;}
        double q=0;for(int j=0;j<3;++j){const double w=double(o[j])-p[j]+s*u[j]-t*v[j];q+=w*w;}
        return {s,t,q};
    }
    // Independent convex quadratic active-set enumeration, using exact small
    // dyadic normal-equation terms; production uses cross products instead.
    SSExpected candidates[5];
    candidates[0]={SSClamp(-d/a),0,0};candidates[1]={SSClamp((b-d)/a),1,0};
    candidates[2]={0,SSClamp(e/c),0};candidates[3]={1,SSClamp((b+e)/c),0};
    int count=4;const double s=(b*e-c*d)/det,t=(a*e-b*d)/det;
    if(s>=0&&s<=1&&t>=0&&t<=1)candidates[count++]={s,t,0};
    SSExpected best={0,0,INFINITY};
    for(int k=0;k<count;++k){const double s=candidates[k].s,t=candidates[k].t;
        candidates[k].q=f+2*d*s-2*e*t+a*s*s-2*b*s*t+c*t*t;
        if(candidates[k].q<best.q)best=candidates[k];}
    return best;
}
static void SSCase(SSSamples &r,SSFn fn,const VxVector &o,const VxVector &u,const VxVector &p,const VxVector &v,int s=12,int t=13,bool root=false){const SSExpected e=SSGramExpected(o,u,p,v);SSObserve(r,fn,o,u,p,v,e.s,e.t,e.q,s,t,root);}
static SSSamples SSGrid(SSFn fn,bool root=false){SSSamples r;
    for(int axis=0;axis<3;++axis)for(float s:{-2.0f,-.25f,0.0f,.25f,.5f,1.0f,1.25f,3.0f})for(float t:{-2.0f,-.25f,0.0f,.25f,.5f,1.0f,1.25f,3.0f})for(float h:{-1,0,1})for(int mask=0;mask<4;++mask){
        const double a=SSClamp(s),b=SSClamp(t);
        SSObserve(r,fn,SSRotate(-11,7,5,axis),SSRotate(1,2,2,axis),SSRotate(-11+s-2*t+2*h,7+2*s+t+4*h,5+2*s-5*h,axis),SSRotate(2,-1,0,axis),a,b,45*double(h)*h+9*std::pow(double(s)-a,2)+5*std::pow(double(t)-b,2),mask&1?12:-1,mask&2?13:-1,root);
    }return r;}
static SSSamples SSCoupledRegions(SSFn fn){SSSamples r;
    for(int axis=0;axis<3;++axis)for(int sign:{-1,1})for(float s:{-4.0f,-1.0f,-.25f,0.0f,.25f,1.0f,4.0f})for(float t:{-2.0f,-.25f,0.0f,.25f,.5f,.75f,1.0f,1.25f,4.0f})for(float h:{0,1,3})for(int mask=0;mask<4;++mask)
        SSCase(r,fn,VxVector(0,0,0),SSRotate(1,0,0,axis),SSRotate(s-sign*t,-t,h,axis),SSRotate(float(sign),1,0,axis),mask&1?12:-1,mask&2?13:-1);
    return r;}
static SSSamples SSDirectionScales(SSFn fn){SSSamples r;
    for(int axis=0;axis<3;++axis)for(int eu:{-149,-100,-64,-32,0,32,64,100,126})for(int ev:{-149,-100,-64,-32,0,32,64,100,126})for(int su:{-1,1})for(int sv:{-1,1}){
        const float u=std::ldexp(float(su),eu),v=std::ldexp(float(sv),ev);const double s=SSClamp(2/double(u)),t=SSClamp(3/double(v));
        SSObserve(r,fn,VxVector(0,0,0),SSRotate(u,0,0,axis),SSRotate(2,-3,1,axis),SSRotate(0,v,0,axis),s,t,1+std::pow(s*u-2,2)+std::pow(3-t*v,2));
    }return r;}
static SSSamples SSParallel(SSFn fn){SSSamples r;
    for(int axis=0;axis<3;++axis)for(int eu:{-64,-12,0,12,64})for(int ev:{-64,-12,0,12,64})for(int sign:{-1,1})for(float k:{-4,-1,0,1,4})for(float h:{-1,0,1}){
        const float u=std::ldexp(1.0f,eu),v=std::ldexp(float(sign),ev);double s,t;
        if(sign>0){if(k>=u){s=1;t=0;}else if(k>=0){s=double(k)/u;t=0;}else{s=0;t=-double(k)>=v?1:-double(k)/v;}}
        else{if(k<=0){s=t=0;}else if(k<=u){s=double(k)/u;t=0;}else{s=1;t=(double(k)-u)>=-double(v)?1:(double(u)-k)/v;}}
        const double w=s*u-k-t*v;
        SSObserve(r,fn,VxVector(0,0,0),SSRotate(u,0,0,axis),SSRotate(k,h,1,axis),SSRotate(v,0,0,axis),s,t,w*w+double(h)*h+1);
    }return r;}
static SSSamples SSNearParallel(SSFn fn){SSSamples r;
    for(int axis=0;axis<3;++axis)for(int e:{-149,-100,-40,-24,-23,-13,-12,-11,-1,0,10,60,100})for(int sign:{-1,1})for(int mask=0;mask<4;++mask){const float d=std::ldexp(float(sign),e);
        SSObserve(r,fn,VxVector(0,0,0),SSRotate(1,0,0,axis),SSRotate(0,-d,1,axis),SSRotate(1,d,0,axis),1,1,1,mask&1?12:-1,mask&2?13:-1);
    }return r;}
static SSSamples SSCancellation(SSFn fn){SSSamples r;
    for(int axis=0;axis<3;++axis)for(int e:{0,20,50,80,100,126})for(int sx:{-1,1})for(int sy:{-1,1})for(int mask=0;mask<4;++mask){
        const float x=std::ldexp(float(sx),e),y=std::ldexp(float(sy),e);const double s=SSClamp(x),t=SSClamp(-double(y));
        SSObserve(r,fn,VxVector(0,0,0),SSRotate(1,0,0,axis),SSRotate(x,y,1,axis),SSRotate(0,1,0,axis),s,t,std::pow(double(x)-s,2)+std::pow(double(y)+t,2)+1,mask&1?12:-1,mask&2?13:-1);
    }
    for(int axis=0;axis<3;++axis)for(int e:{20,30,40,50}){const float h=std::ldexp(1.0f,e);
        // Both endpoints cancel a large extent, leaving a unit offset.
        SSObserve(r,fn,SSRotate(h,-1,0,axis),SSRotate(-h,0,0,axis),SSRotate(0,h,0,axis),SSRotate(0,-h,0,axis),1,1,1);
        SSObserve(r,fn,SSRotate(h,0,1,axis),SSRotate(-h,0,0,axis),SSRotate(-1,0,0,axis),SSRotate(-h,0,0,axis),1,0,2);
    }
    SSObserve(r,fn,VxVector(FLT_MAX,0,0),VxVector(FLT_MAX,0,0),VxVector(-FLT_MAX,0,0),VxVector(-FLT_MAX,0,0),0,0,INFINITY);
    return r;}
static SSSamples SSAliases(SSFn fn){SSSamples r;
    for(int axis=0;axis<3;++axis)for(int s=-1;s<14;++s)for(int t=-1;t<14;++t)for(float x:{-1.0f,.5f,2.0f})for(float y:{-1.0f,.5f,2.0f})
        SSCase(r,fn,VxVector(0,0,0),SSRotate(1,0,0,axis),SSRotate(x-y,-y,1,axis),SSRotate(1,1,0,axis),s,t);
    return r;}
static SSSamples SSZero(SSFn fn){SSSamples r;
    for(int axis=0;axis<3;++axis)for(int mask=0;mask<4;++mask){
        SSObserve(r,fn,VxVector(0,0,0),VxVector(0,0,0),SSRotate(4,3,4,axis),SSRotate(1,2,2,axis),1,0,41,mask&1?12:-1,mask&2?13:-1);
        SSObserve(r,fn,VxVector(0,0,0),SSRotate(1,2,2,axis),SSRotate(4,3,4,axis),VxVector(0,0,0),1,0,14,mask&1?12:-1,mask&2?13:-1);
        SSObserve(r,fn,VxVector(0,0,0),SSRotate(1,2,2,axis),SSRotate(-4,-3,-4,axis),VxVector(0,0,0),0,1,41,mask&1?12:-1,mask&2?13:-1);
        SSObserve(r,fn,VxVector(0,0,0),VxVector(0,0,0),SSRotate(4,3,4,axis),VxVector(0,0,0),1,0,41,mask&1?12:-1,mask&2?13:-1);
    }return r;}
static SSSamples SSBoundary(SSFn fn){SSSamples r;
    for(int axis=0;axis<3;++axis)for(int ev:{-100,-20,0,20,100})for(float s:{std::nextafter(0.0f,-INFINITY),0.0f,.5f,std::nextafter(1.0f,0.0f),1.0f,std::nextafter(1.0f,INFINITY)})for(float t:{0.0f,.5f,std::nextafter(1.0f,0.0f),1.0f,std::nextafter(1.0f,INFINITY)})for(int mask=0;mask<4;++mask){
        const float v=std::ldexp(1.0f,ev),y=-v*t;const double a=SSClamp(s),b=SSClamp(-double(y)/v);
        SSObserve(r,fn,VxVector(0,0,0),SSRotate(1,0,0,axis),SSRotate(s,y,0,axis),SSRotate(0,v,0,axis),a,b,std::pow(double(s)-a,2)+std::pow(double(y)+b*v,2),mask&1?12:-1,mask&2?13:-1);
    }return r;}
static unsigned SSNext(unsigned &seed){seed=seed*1664525u+1013904223u;return seed;}
static SSSamples SSRandom(SSFn fn){SSSamples r;unsigned seed=285;
    while(r.samples<4000){VxVector o,u,p,v;for(int j=0;j<3;++j){o[j]=float(int(SSNext(seed)%33)-16);p[j]=float(int(SSNext(seed)%33)-16);u[j]=float(int(SSNext(seed)%11)-5);v[j]=float(int(SSNext(seed)%11)-5);}
        const double a=double(u.x)*u.x+double(u.y)*u.y+double(u.z)*u.z,c=double(v.x)*v.x+double(v.y)*v.y+double(v.z)*v.z,b=double(u.x)*v.x+double(u.y)*v.y+double(u.z)*v.z;
        if(a*c==b*b)continue;SSCase(r,fn,o,u,p,v,r.samples&1?12:-1,r.samples&2?13:-1);
    }return r;}

#if defined(_WIN32)
static SSSamples SSFootprint(SSFn fn) {
    SSSamples r;SYSTEM_INFO si;GetSystemInfo(&si);const size_t page=si.dwPageSize;
    char *pages=static_cast<char*>(VirtualAlloc(nullptr,page*8,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));if(!pages){r.errors=1;return r;}
    VxRay *a=new(pages+page-24)VxRay,*b=new(pages+page*3-24)VxRay;
    a->m_Origin=VxVector(0,0,0);a->m_Direction=VxVector(1,0,0);b->m_Origin=VxVector(2,-.5f,4);b->m_Direction=VxVector(0,1,0);
    float *s=new(pages+page*5-4)float(137),*t=new(pages+page*7-4)float(139);
    DWORD old;bool good=true;for(int i=0;i<4;++i)good=VirtualProtect(pages+page*(2*i+1),page,PAGE_NOACCESS,&old)&&good;
    for(int i=0;i<2;++i)good=VirtualProtect(pages+page*2*i,page,PAGE_READONLY,&old)&&good;
    if(good){r.samples=2;r.errors=(fn(*a,*b,s,t)!=17||*s!=1||*t!=.5f)+(fn(*a,*b,nullptr,nullptr)!=17);}else r.errors=1;
    VirtualFree(pages,0,MEM_RELEASE);return r;
}
#endif
#ifndef VX_SEGMENT_SEGMENT_NATIVE_PROBE
static void SSCheck(SSSamples r){EXPECT_EQ(r.errors,0)<<"values="<<r.valueErrors<<" parameters="<<r.parameterErrors<<" changed inputs="<<r.inputErrors;::testing::Test::RecordProperty("samples",r.samples);}
static float SSScalar(const VxRay &a,const VxRay &b,float *s,float *t){g_VxDistanceDispatch.store(&kVxDistanceDispatchScalar,std::memory_order_release);return VxDistance::SegmentSegmentSquareDistance(a,b,s,t);}
#if defined(VX_SIMD_SSE)
static float SSSimd(const VxRay &a,const VxRay &b,float *s,float *t){g_VxDistanceDispatch.store(&kVxDistanceDispatchSIMD,std::memory_order_release);return VxDistance::SegmentSegmentSquareDistance(a,b,s,t);}
#endif
#define SS_TESTS(prefix,fn) \
TEST(SegmentSegmentDistanceReference,prefix##AllNineOrthogonalRegions){SSCheck(SSGrid(fn));} \
TEST(SegmentSegmentDistanceReference,prefix##CoupledRegionsMatchIndependentCandidates){SSCheck(SSCoupledRegions(fn));} \
TEST(SegmentSegmentDistanceReference,prefix##IndependentDirectionScales){SSCheck(SSDirectionScales(fn));} \
TEST(SegmentSegmentDistanceReference,prefix##ParallelEndpointRepresentatives){SSCheck(SSParallel(fn));} \
TEST(SegmentSegmentDistanceReference,prefix##NonparallelBelowOriginalThreshold){SSCheck(SSNearParallel(fn));} \
TEST(SegmentSegmentDistanceReference,prefix##SmallDistancesAndEndpointCancellation){SSCheck(SSCancellation(fn));} \
TEST(SegmentSegmentDistanceReference,prefix##TwoOutputsMayAliasAnyInputOrEachOther){SSCheck(SSAliases(fn));} \
TEST(SegmentSegmentDistanceReference,prefix##OriginalZeroDirectionRepresentatives){SSCheck(SSZero(fn));} \
TEST(SegmentSegmentDistanceReference,prefix##FourBoundariesBeforeFloatParameterRounding){SSCheck(SSBoundary(fn));} \
TEST(SegmentSegmentDistanceReference,prefix##IndependentConstrainedNormalEquationCandidates){SSCheck(SSRandom(fn));}
SS_TESTS(Scalar,SSScalar)
#if defined(VX_SIMD_SSE)
SS_TESTS(Simd,SSSimd)
#endif
#if defined(_WIN32)
TEST(SegmentSegmentDistanceReference,ScalarTwentyFourAndFourByteFootprints){SSCheck(SSFootprint(SSScalar));}
#if defined(VX_SIMD_SSE)
TEST(SegmentSegmentDistanceReference,SimdTwentyFourAndFourByteFootprints){SSCheck(SSFootprint(SSSimd));}
#endif
#endif
TEST(SegmentSegmentDistanceReference,WrapperKeepsSquaredFloatRange){SSCheck(SSGrid(VxDistance::SegmentSegmentDistance,true));VxRay a,b;a.m_Origin=VxVector(0,0,0);a.m_Direction=VxVector(1,0,0);b.m_Origin=VxVector(0,0,std::ldexp(1.0f,100));b.m_Direction=VxVector(0,1,0);float s,t;EXPECT_EQ(VxDistance::SegmentSegmentDistance(a,b,&s,&t),INFINITY);EXPECT_EQ(s,0);EXPECT_EQ(t,0);}
#if defined(_WIN32)
TEST(SegmentSegmentDistanceReference,ActualCurrentDllExport){HMODULE dll=LoadLibraryA("VxMath.dll");ASSERT_NE(dll,nullptr);
#if defined(_WIN64)
    const char *symbol="?SegmentSegmentSquareDistance@VxDistance@@SAMAEBVVxRay@@0PEAM1@Z";
#else
    const char *symbol="?SegmentSegmentSquareDistance@VxDistance@@SAMABVVxRay@@0PAM1@Z";
#endif
    SSFn fn=reinterpret_cast<SSFn>(GetProcAddress(dll,symbol));ASSERT_NE(fn,nullptr);SSCheck(SSGrid(fn));SSCheck(SSCoupledRegions(fn));SSCheck(SSDirectionScales(fn));SSCheck(SSParallel(fn));SSCheck(SSNearParallel(fn));SSCheck(SSCancellation(fn));SSCheck(SSAliases(fn));SSCheck(SSZero(fn));SSCheck(SSBoundary(fn));SSCheck(SSRandom(fn));SSCheck(SSFootprint(fn));EXPECT_NE(FreeLibrary(dll),0);}
#endif
#endif
