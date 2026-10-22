#include <cmath>
#include <cfloat>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <new>
#include <initializer_list>
#ifndef VX_POINT_SEGMENT_NATIVE_PROBE
#ifndef VX_LIB
#define VX_LIB 1
#endif
#ifndef VX_POINT_SEGMENT_IMPLEMENTATION
#define VX_POINT_SEGMENT_IMPLEMENTATION "../../src/VxDistance.cpp"
#endif
#include VX_POINT_SEGMENT_IMPLEMENTATION
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef float (*PSFn)(const VxVector &,const VxRay &,float *);
struct PSSamples {int samples=0,errors=0,valueErrors=0,parameterErrors=0,inputErrors=0;unsigned digest=2166136261u;};
static bool PSEqual(float a,float b){return std::isnan(b)?std::isnan(a):a==b;}
static void PSObserve(PSSamples &r,PSFn fn,VxVector point,const VxVector &origin,const VxVector &direction,
                      double expectedT,double expectedQ,int output=9,bool root=false) {
    VxRay line;line.m_Origin=origin;line.m_Direction=direction;
    const float before[]={point.x,point.y,point.z,origin.x,origin.y,origin.z,direction.x,direction.y,direction.z};
    float outside=137,*parameter=&outside;
    if(output<0)parameter=nullptr;
    else if(output<3)parameter=&point[output];
    else if(output<6)parameter=&line.m_Origin[output-3];
    else if(output<9)parameter=&line.m_Direction[output-6];
    const float expectedParameter=float(expectedT),roundedQ=float(expectedQ);
    const float expectedValue=root?float(std::sqrt(double(roundedQ))):roundedQ;
    const float value=fn(point,line,parameter);
    const float after[]={point.x,point.y,point.z,line.m_Origin.x,line.m_Origin.y,line.m_Origin.z,line.m_Direction.x,line.m_Direction.y,line.m_Direction.z};
    bool changed=false;for(int i=0;i<9;++i)if(i!=output)changed=changed||std::memcmp(after+i,before+i,4)!=0;
    const bool badValue=!PSEqual(value,expectedValue),badT=parameter&&!PSEqual(*parameter,expectedParameter);
    ++r.samples;r.valueErrors+=badValue;r.parameterErrors+=badT;r.inputErrors+=changed;r.errors+=badValue||badT||changed;
    unsigned bits;std::memcpy(&bits,&value,4);r.digest=(r.digest^bits)*16777619u;
}
static VxVector PSRotate(float x,float y,float z,int axis){VxVector v;v[axis]=x;v[(axis+1)%3]=y;v[(axis+2)%3]=z;return v;}
static PSSamples PSGrid(PSFn fn,bool root=false) {
    PSSamples r;const float parameters[]={-4,-1,-.25f,0,.25f,1,4};
    for(int axis=0;axis<3;++axis)for(int sign:{-1,1})for(float t:parameters)for(float d:parameters)for(int output:{-1,9}){
        const VxVector origin=PSRotate(-11,7,5,axis),direction=PSRotate(float(sign),float(2*sign),float(2*sign),axis);
        const VxVector point=PSRotate(-11+t+2*d,7+2*t-d,5+2*t,axis);
        // (1,2,2) is perpendicular to (2,-1,0), whose squared length is five.
        PSObserve(r,fn,point,origin,direction,t*sign<=0?0:t*sign>=1?1:t*sign,5*double(d)*d+9*std::pow(double(t*sign)-(t*sign<=0?0:t*sign>=1?1:t*sign),2),output,root);
    }return r;
}
static PSSamples PSDirectionScales(PSFn fn) {
    PSSamples r;
    for(int axis=0;axis<3;++axis)for(int e:{-149,-140,-100,-80,-64,-32,-1,0,1,32,64,80,100,120,126})for(int sign:{-1,1})for(int output:{-1,9}){
        const float scale=std::ldexp(float(sign),e);
        PSObserve(r,fn,PSRotate(4,3,4,axis),VxVector(0,0,0),PSRotate(scale,scale*2,scale*2,axis),sign<0?0:e<=1?1:std::ldexp(2.0,-e),sign<0?41:e>=1?5:41-36*double(scale)+9*double(scale)*scale,output);
    }return r;
}
static PSSamples PSGeometryScales(PSFn fn) {
    PSSamples r;
    for(int axis=0;axis<3;++axis)for(int e:{-70,-40,-10,0,10,40,60})for(int sign:{-1,1})for(int output:{-1,9}){
        const float s=std::ldexp(1.0f,e);
        PSObserve(r,fn,PSRotate(4*s,3*s,4*s,axis),VxVector(0,0,0),PSRotate(float(sign),float(2*sign),float(2*sign),axis),sign<0?0:e>=0?1:2*double(s),sign<0?41*double(s)*s:e<0?5*double(s)*s:41*double(s)*s-36*double(s)+9,output);
    }return r;
}
static PSSamples PSCancellation(PSFn fn) {
    PSSamples r;
    for(int axis=0;axis<3;++axis)for(int e:{0,20,50,80,100})for(int d:{3,7,11,41})for(int sign:{-1,1})for(int output:{-1,9}){
        const float x=std::ldexp(float(sign),e);
        PSObserve(r,fn,PSRotate(x,1,0,axis),VxVector(0,0,0),PSRotate(std::ldexp(float(d),e),0,0,axis),sign>0?1.0/d:0,sign>0?1:double(x)*x+1,output);
        PSObserve(r,fn,PSRotate(x,0,0,axis),VxVector(0,0,0),PSRotate(std::ldexp(float(d),e),0,0,axis),sign>0?1.0/d:0,sign>0?0:double(x)*x,output);
    }
    for(int axis=0;axis<3;++axis)for(int output:{-1,9}){
        // A finite midpoint of a very long segment; float dot/length overflows.
        PSObserve(r,fn,PSRotate(-FLT_MAX*.5f,1,0,axis),PSRotate(-FLT_MAX,0,0,axis),PSRotate(FLT_MAX,0,0,axis),.5,1,output);
    }return r;
}
static PSSamples PSAliases(PSFn fn) {
    PSSamples r;for(int axis=0;axis<3;++axis)for(int output=0;output<9;++output) {
        PSObserve(r,fn,PSRotate(2.5f,0,1,axis),VxVector(0,0,0),PSRotate(1,2,2,axis),.5,5,output);
        PSObserve(r,fn,PSRotate(4,3,4,axis),VxVector(0,0,0),PSRotate(1,2,2,axis),1,14,output);
        PSObserve(r,fn,PSRotate(3,1,2,axis),VxVector(0,0,0),PSRotate(1,2,2,axis),1,5,output);
        PSObserve(r,fn,PSRotate(4,3,4,axis),VxVector(0,0,0),PSRotate(-1,-2,-2,axis),0,41,output);
        PSObserve(r,fn,PSRotate(2,-1,0,axis),VxVector(0,0,0),PSRotate(1,2,2,axis),0,5,output);
        PSObserve(r,fn,PSRotate(4,3,4,axis),VxVector(0,0,0),VxVector(0,0,0),0,41,output);
    }return r;
}
static PSSamples PSZero(PSFn fn) {
    PSSamples r;for(int axis=0;axis<3;++axis)for(int output:{-1,9}) {
        PSObserve(r,fn,PSRotate(4,3,4,axis),VxVector(0,0,0),VxVector(0,0,0),0,41,output);
        PSObserve(r,fn,VxVector(7,11,-2),VxVector(7,11,-2),VxVector(0,0,0),0,0,output);
    }return r;
}
static PSSamples PSBoundary(PSFn fn) {
    PSSamples r;
    // No epsilon around the origin: every positive projection is accepted.
    for(int axis=0;axis<3;++axis)for(int e:{-149,-140,-100,-50,-1,0,1})for(int sign:{-1,1})for(int output:{-1,9}) {
        const float t=std::ldexp(float(sign),e);
        PSObserve(r,fn,PSRotate(t,1,0,axis),VxVector(0,0,0),PSRotate(1,0,0,axis),sign<0?0:t>=1?1:t,sign<0?1+double(t)*t:t>1?1+(double(t)-1)*(double(t)-1):1,output);
    }
    for(int axis=0;axis<3;++axis)for(int sign:{-1,1})for(int output:{-1,9}) {
        const float tiny=std::ldexp(1.0f,-149);
        PSObserve(r,fn,PSRotate(tiny*sign,1,0,axis),VxVector(0,0,0),PSRotate(tiny,0,0,axis),sign>0?1:0,1,output);
    }
    // Large opposing products leave a signed unit dot product. The branch
    // and parameter must not be decided by a rounded float dot product.
    for(int axis=0;axis<3;++axis)for(int e:{24,30,50})for(int sign:{-1,1})for(int output:{-1,9}) {
        const float large=std::ldexp(1.0f,e);
        PSObserve(r,fn,PSRotate(large,float(sign),-large,axis),VxVector(0,0,0),VxVector(1,1,1),sign>0?1.0/3:0,std::ldexp(2.0,2*e)+(sign>0?2.0/3:1.0),output);
    }
    // Adjacent binary32 parameters on either side of the endpoint, tested at
    // several direction scales. Equality selects the original end branch.
    for(int axis=0;axis<3;++axis)for(int e:{-100,-50,0,50,100})
        for(float t:{std::nextafter(1.0f,0.0f),1.0f,std::nextafter(1.0f,INFINITY)})for(int output:{-1,9}) {
            const float scale=std::ldexp(1.0f,e);
            const double residual=t>1?double(scale)*(double(t)-1):0;
            PSObserve(r,fn,PSRotate(scale*t,1,0,axis),VxVector(0,0,0),PSRotate(scale,0,0,axis),t<1?t:1,1+residual*residual,output);
    }
    for(int axis=0;axis<3;++axis)for(int e:{24,30,50})for(int middle:{2,4})for(int output:{-1,9}) {
        const float large=std::ldexp(1.0f,e);
        const double magnitude=std::ldexp(2.0,2*e)+middle*middle;
        PSObserve(r,fn,PSRotate(large,float(middle),-large,axis),VxVector(0,0,0),VxVector(1,1,1),middle<3?double(middle)/3:1,middle<3?magnitude-double(middle*middle)/3:magnitude-2*middle+3,output);
    }return r;
}
static unsigned PSNext(unsigned &seed){seed=seed*1664525u+1013904223u;return seed;}
static PSSamples PSRandom(PSFn fn) {
    PSSamples r;unsigned seed=277;
    while(r.samples<4000){VxVector p,o,d;for(int j=0;j<3;++j){p[j]=float(int(PSNext(seed)%33)-16);o[j]=float(int(PSNext(seed)%33)-16);d[j]=float(int(PSNext(seed)%11)-5);}
        double length=0,dot=0,magnitude=0;for(int j=0;j<3;++j){const double diff=double(p[j])-o[j];length+=double(d[j])*d[j];dot+=diff*d[j];magnitude+=diff*diff;}
        // Constrained Gram form, independent of the production cross product
        // and endpoint residual. All intermediates are exact small integers.
        PSObserve(r,fn,p,o,d,dot<=0?0:dot>=length?1:dot/length,dot<=0?magnitude:dot>=length?magnitude-2*dot+length:(magnitude*length-dot*dot)/length,r.samples&1?-1:9);
    }return r;
}
#if defined(_WIN32)
static PSSamples PSFootprint(PSFn fn) {
    PSSamples r;SYSTEM_INFO si;GetSystemInfo(&si);const size_t page=si.dwPageSize;
    char *pages=static_cast<char*>(VirtualAlloc(nullptr,page*6,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));if(!pages){r.errors=1;return r;}
    VxVector *point=new(pages+page-12)VxVector(2,3,4);VxRay *line=new(pages+page*3-24)VxRay;
    line->m_Origin=VxVector(0,0,0);line->m_Direction=VxVector(1,0,0);float *t=new(pages+page*5-4)float(137);
    DWORD old;bool good=true;for(int i=0;i<3;++i)good=VirtualProtect(pages+page*(2*i+1),page,PAGE_NOACCESS,&old)&&good;
    for(int i=0;i<2;++i)good=VirtualProtect(pages+page*2*i,page,PAGE_READONLY,&old)&&good;
    if(good){r.samples=2;r.errors=(fn(*point,*line,t)!=26||*t!=1)+(fn(*point,*line,nullptr)!=26);}else r.errors=1;
    VirtualFree(pages,0,MEM_RELEASE);return r;
}
#endif
#ifndef VX_POINT_SEGMENT_NATIVE_PROBE
static void PSCheck(PSSamples r){EXPECT_EQ(r.errors,0)<<"values="<<r.valueErrors<<" parameters="<<r.parameterErrors<<" changed inputs="<<r.inputErrors;::testing::Test::RecordProperty("samples",r.samples);::testing::Test::RecordProperty("digest",std::to_string(r.digest));}
#define PS_TESTS(prefix,fn) \
TEST(PointSegmentDistanceReference,prefix##AffineGridAndOptionalParameter){PSCheck(PSGrid(fn));} \
TEST(PointSegmentDistanceReference,prefix##NonzeroDirectionAcrossFloatRange){PSCheck(PSDirectionScales(fn));} \
TEST(PointSegmentDistanceReference,prefix##FiniteGeometryScales){PSCheck(PSGeometryScales(fn));} \
TEST(PointSegmentDistanceReference,prefix##PerpendicularDistanceSurvivesCancellation){PSCheck(PSCancellation(fn));} \
TEST(PointSegmentDistanceReference,prefix##OutputMayAliasInputComponents){PSCheck(PSAliases(fn));} \
TEST(PointSegmentDistanceReference,prefix##ZeroDirectionIsPointQuery){PSCheck(PSZero(fn));} \
TEST(PointSegmentDistanceReference,prefix##BothEndpointBoundariesAndSignedDot){PSCheck(PSBoundary(fn));} \
TEST(PointSegmentDistanceReference,prefix##IndependentGramOracle){PSCheck(PSRandom(fn));}
PS_TESTS(Scalar,PointSegmentSquareDistanceScalarImpl)
#if defined(VX_SIMD_SSE)
PS_TESTS(Simd,PointSegmentSquareDistanceSIMDImpl)
#endif
#if defined(_WIN32)
TEST(PointSegmentDistanceReference,ScalarTwelveTwentyFourAndFourByteFootprints){PSCheck(PSFootprint(PointSegmentSquareDistanceScalarImpl));}
#if defined(VX_SIMD_SSE)
TEST(PointSegmentDistanceReference,SimdTwelveTwentyFourAndFourByteFootprints){PSCheck(PSFootprint(PointSegmentSquareDistanceSIMDImpl));}
#endif
#endif
TEST(PointSegmentDistanceReference,PublicForwarderUsesDistanceDispatch){const auto *saved=GetVxDistanceDispatchTable();for(int i=0;i<4;++i){const auto *table=&kVxDistanceDispatchScalar;
#if defined(VX_SIMD_SSE)
    if(i&1)table=&kVxDistanceDispatchSIMD;
#endif
    g_VxDistanceDispatch.store(table,std::memory_order_release);PSCheck(PSDirectionScales(VxDistance::PointSegmentSquareDistance));}g_VxDistanceDispatch.store(saved,std::memory_order_release);}
TEST(PointSegmentDistanceReference,PublicDistanceWrapperKeepsSquaredFloatContract){PSCheck(PSGrid(VxDistance::PointSegmentDistance,true));float t;
    VxRay line;line.m_Origin=VxVector(0,0,0);line.m_Direction=VxVector(1,0,0);
    const VxVector p(2,std::ldexp(1.0f,100),0);EXPECT_EQ(VxDistance::PointSegmentDistance(p,line,&t),INFINITY);EXPECT_EQ(t,1);
}
#if defined(_WIN32)
TEST(PointSegmentDistanceReference,ActualCurrentDllExport){HMODULE dll=LoadLibraryA("VxMath.dll");ASSERT_NE(dll,nullptr);
#if defined(_WIN64)
    const char *name="?PointSegmentSquareDistance@VxDistance@@SAMAEBUVxVector@@AEBVVxRay@@PEAM@Z";
#else
    const char *name="?PointSegmentSquareDistance@VxDistance@@SAMABUVxVector@@ABVVxRay@@PAM@Z";
#endif
    PSFn fn=reinterpret_cast<PSFn>(GetProcAddress(dll,name));ASSERT_NE(fn,nullptr);PSCheck(PSGrid(fn));PSCheck(PSDirectionScales(fn));PSCheck(PSGeometryScales(fn));PSCheck(PSCancellation(fn));PSCheck(PSAliases(fn));PSCheck(PSZero(fn));PSCheck(PSBoundary(fn));PSCheck(PSRandom(fn));PSCheck(PSFootprint(fn));EXPECT_NE(FreeLibrary(dll),0);}
#endif
#endif
