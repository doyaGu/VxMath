#include <cmath>
#include <cfloat>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <new>
#include <initializer_list>
#ifndef VX_POINT_RAY_NATIVE_PROBE
#ifndef VX_LIB
#define VX_LIB 1
#endif
#ifndef VX_POINT_RAY_IMPLEMENTATION
#define VX_POINT_RAY_IMPLEMENTATION "../../src/VxDistance.cpp"
#endif
#include VX_POINT_RAY_IMPLEMENTATION
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef float (*PRFn)(const VxVector &,const VxRay &,float *);
struct PRSamples {int samples=0,errors=0,valueErrors=0,parameterErrors=0,inputErrors=0;unsigned digest=2166136261u;};
static bool PREqual(float a,float b){return std::isnan(b)?std::isnan(a):a==b;}
static void PRObserve(PRSamples &r,PRFn fn,VxVector point,const VxVector &origin,const VxVector &direction,
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
    const bool badValue=!PREqual(value,expectedValue),badT=parameter&&!PREqual(*parameter,expectedParameter);
    ++r.samples;r.valueErrors+=badValue;r.parameterErrors+=badT;r.inputErrors+=changed;r.errors+=badValue||badT||changed;
    unsigned bits;std::memcpy(&bits,&value,4);r.digest=(r.digest^bits)*16777619u;
}
static VxVector PRRotate(float x,float y,float z,int axis){VxVector v;v[axis]=x;v[(axis+1)%3]=y;v[(axis+2)%3]=z;return v;}
static PRSamples PRGrid(PRFn fn,bool root=false) {
    PRSamples r;const float parameters[]={-4,-1,-.25f,0,.25f,1,4};
    for(int axis=0;axis<3;++axis)for(int sign:{-1,1})for(float t:parameters)for(float d:parameters)for(int output:{-1,9}){
        const VxVector origin=PRRotate(-11,7,5,axis),direction=PRRotate(float(sign),float(2*sign),float(2*sign),axis);
        const VxVector point=PRRotate(-11+t+2*d,7+2*t-d,5+2*t,axis);
        // (1,2,2) is perpendicular to (2,-1,0), whose squared length is five.
        PRObserve(r,fn,point,origin,direction,t*sign>0?t*sign:0,5*double(d)*d+(t*sign>0?0:9*double(t)*t),output,root);
    }return r;
}
static PRSamples PRDirectionScales(PRFn fn) {
    PRSamples r;
    for(int axis=0;axis<3;++axis)for(int e:{-149,-140,-100,-80,-64,-32,-1,0,1,32,64,80,100,120,126})for(int sign:{-1,1})for(int output:{-1,9}){
        const float scale=std::ldexp(float(sign),e);
        PRObserve(r,fn,PRRotate(4,3,4,axis),VxVector(0,0,0),PRRotate(scale,scale*2,scale*2,axis),sign>0?std::ldexp(2.0,-e):0,sign>0?5:41,output);
    }return r;
}
static PRSamples PRGeometryScales(PRFn fn) {
    PRSamples r;
    for(int axis=0;axis<3;++axis)for(int e:{-70,-40,-10,0,10,40,60})for(int sign:{-1,1})for(int output:{-1,9}){
        const float s=std::ldexp(1.0f,e);
        PRObserve(r,fn,PRRotate(4*s,3*s,4*s,axis),VxVector(0,0,0),PRRotate(float(sign),float(2*sign),float(2*sign),axis),sign>0?2*double(s):0,(sign>0?5:41)*double(s)*s,output);
    }return r;
}
static PRSamples PRCancellation(PRFn fn) {
    PRSamples r;
    for(int axis=0;axis<3;++axis)for(int e:{0,20,50,80,100})for(int d:{3,7,11,41})for(int sign:{-1,1})for(int output:{-1,9}){
        const float x=std::ldexp(float(sign),e);
        PRObserve(r,fn,PRRotate(x,1,0,axis),VxVector(0,0,0),PRRotate(float(d),0,0,axis),sign>0?double(x)/d:0,sign>0?1:double(x)*x+1,output);
        PRObserve(r,fn,PRRotate(x,0,0,axis),VxVector(0,0,0),PRRotate(float(d),0,0,axis),sign>0?double(x)/d:0,sign>0?0:double(x)*x,output);
    }
    for(int axis=0;axis<3;++axis)for(int output:{-1,9}){
        // The point-origin subtraction overflows float, but the perpendicular
        // distance is exactly one and the true parameter is exactly two.
        PRObserve(r,fn,PRRotate(FLT_MAX,1,0,axis),PRRotate(-FLT_MAX,0,0,axis),PRRotate(FLT_MAX,0,0,axis),2,1,output);
    }return r;
}
static PRSamples PRAliases(PRFn fn) {
    PRSamples r;for(int axis=0;axis<3;++axis)for(int output=0;output<9;++output) {
        PRObserve(r,fn,PRRotate(4,3,4,axis),VxVector(0,0,0),PRRotate(1,2,2,axis),2,5,output);
        PRObserve(r,fn,PRRotate(4,3,4,axis),VxVector(0,0,0),PRRotate(-1,-2,-2,axis),0,41,output);
        PRObserve(r,fn,PRRotate(2,-1,0,axis),VxVector(0,0,0),PRRotate(1,2,2,axis),0,5,output);
        PRObserve(r,fn,PRRotate(4,3,4,axis),VxVector(0,0,0),VxVector(0,0,0),0,41,output);
    }return r;
}
static PRSamples PRZero(PRFn fn) {
    PRSamples r;for(int axis=0;axis<3;++axis)for(int output:{-1,9}) {
        PRObserve(r,fn,PRRotate(4,3,4,axis),VxVector(0,0,0),VxVector(0,0,0),0,41,output);
        PRObserve(r,fn,VxVector(7,11,-2),VxVector(7,11,-2),VxVector(0,0,0),0,0,output);
    }return r;
}
static PRSamples PRBoundary(PRFn fn) {
    PRSamples r;
    // No epsilon around the origin: every positive projection is accepted.
    for(int axis=0;axis<3;++axis)for(int e:{-149,-140,-100,-50,-1,0,1})for(int sign:{-1,1})for(int output:{-1,9}) {
        const float t=std::ldexp(float(sign),e);
        PRObserve(r,fn,PRRotate(t,1,0,axis),VxVector(0,0,0),PRRotate(1,0,0,axis),sign>0?t:0,sign>0?1:1+double(t)*t,output);
    }
    for(int axis=0;axis<3;++axis)for(int sign:{-1,1})for(int output:{-1,9}) {
        const float tiny=std::ldexp(1.0f,-149);
        PRObserve(r,fn,PRRotate(tiny*sign,1,0,axis),VxVector(0,0,0),PRRotate(tiny,0,0,axis),sign>0?1:0,1,output);
    }
    // Large opposing products leave a signed unit dot product. The branch
    // and parameter must not be decided by a rounded float dot product.
    for(int axis=0;axis<3;++axis)for(int e:{24,30,50})for(int sign:{-1,1})for(int output:{-1,9}) {
        const float large=std::ldexp(1.0f,e);
        PRObserve(r,fn,PRRotate(large,float(sign),-large,axis),VxVector(0,0,0),VxVector(1,1,1),sign>0?1.0/3:0,std::ldexp(2.0,2*e)+(sign>0?2.0/3:1.0),output);
    }return r;
}
static unsigned PRNext(unsigned &seed){seed=seed*1664525u+1013904223u;return seed;}
static PRSamples PRRandom(PRFn fn) {
    PRSamples r;unsigned seed=277;
    while(r.samples<4000){VxVector p,o,d;for(int j=0;j<3;++j){p[j]=float(int(PRNext(seed)%33)-16);o[j]=float(int(PRNext(seed)%33)-16);d[j]=float(int(PRNext(seed)%11)-5);}
        double length=0,dot=0,magnitude=0;for(int j=0;j<3;++j){const double diff=double(p[j])-o[j];length+=double(d[j])*d[j];dot+=diff*d[j];magnitude+=diff*diff;}
        // Gram determinant, independent of the production cross-product form.
        PRObserve(r,fn,p,o,d,dot>0?dot/length:0,dot>0?(magnitude*length-dot*dot)/length:magnitude,r.samples&1?-1:9);
    }return r;
}
#if defined(_WIN32)
static PRSamples PRFootprint(PRFn fn) {
    PRSamples r;SYSTEM_INFO si;GetSystemInfo(&si);const size_t page=si.dwPageSize;
    char *pages=static_cast<char*>(VirtualAlloc(nullptr,page*6,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));if(!pages){r.errors=1;return r;}
    VxVector *point=new(pages+page-12)VxVector(2,3,4);VxRay *line=new(pages+page*3-24)VxRay;
    line->m_Origin=VxVector(0,0,0);line->m_Direction=VxVector(1,0,0);float *t=new(pages+page*5-4)float(137);
    DWORD old;bool good=true;for(int i=0;i<3;++i)good=VirtualProtect(pages+page*(2*i+1),page,PAGE_NOACCESS,&old)&&good;
    for(int i=0;i<2;++i)good=VirtualProtect(pages+page*2*i,page,PAGE_READONLY,&old)&&good;
    if(good){r.samples=2;r.errors=(fn(*point,*line,t)!=25||*t!=2)+(fn(*point,*line,nullptr)!=25);}else r.errors=1;
    VirtualFree(pages,0,MEM_RELEASE);return r;
}
#endif
#ifndef VX_POINT_RAY_NATIVE_PROBE
static void PRCheck(PRSamples r){EXPECT_EQ(r.errors,0)<<"values="<<r.valueErrors<<" parameters="<<r.parameterErrors<<" changed inputs="<<r.inputErrors;::testing::Test::RecordProperty("samples",r.samples);::testing::Test::RecordProperty("digest",std::to_string(r.digest));}
#define PR_TESTS(prefix,fn) \
TEST(PointRayDistanceReference,prefix##AffineGridAndOptionalParameter){PRCheck(PRGrid(fn));} \
TEST(PointRayDistanceReference,prefix##NonzeroDirectionAcrossFloatRange){PRCheck(PRDirectionScales(fn));} \
TEST(PointRayDistanceReference,prefix##FiniteGeometryScales){PRCheck(PRGeometryScales(fn));} \
TEST(PointRayDistanceReference,prefix##PerpendicularDistanceSurvivesCancellation){PRCheck(PRCancellation(fn));} \
TEST(PointRayDistanceReference,prefix##OutputMayAliasInputComponents){PRCheck(PRAliases(fn));} \
TEST(PointRayDistanceReference,prefix##ZeroDirectionIsPointQuery){PRCheck(PRZero(fn));} \
TEST(PointRayDistanceReference,prefix##OriginBoundaryAndSignedDot){PRCheck(PRBoundary(fn));} \
TEST(PointRayDistanceReference,prefix##IndependentGramOracle){PRCheck(PRRandom(fn));}
PR_TESTS(Scalar,PointRaySquareDistanceScalarImpl)
#if defined(VX_SIMD_SSE)
PR_TESTS(Simd,PointRaySquareDistanceSIMDImpl)
#endif
#if defined(_WIN32)
TEST(PointRayDistanceReference,ScalarTwelveTwentyFourAndFourByteFootprints){PRCheck(PRFootprint(PointRaySquareDistanceScalarImpl));}
#if defined(VX_SIMD_SSE)
TEST(PointRayDistanceReference,SimdTwelveTwentyFourAndFourByteFootprints){PRCheck(PRFootprint(PointRaySquareDistanceSIMDImpl));}
#endif
#endif
TEST(PointRayDistanceReference,PublicForwarderUsesDistanceDispatch){const auto *saved=GetVxDistanceDispatchTable();for(int i=0;i<4;++i){const auto *table=&kVxDistanceDispatchScalar;
#if defined(VX_SIMD_SSE)
    if(i&1)table=&kVxDistanceDispatchSIMD;
#endif
    g_VxDistanceDispatch.store(table,std::memory_order_release);PRCheck(PRDirectionScales(VxDistance::PointRaySquareDistance));}g_VxDistanceDispatch.store(saved,std::memory_order_release);}
TEST(PointRayDistanceReference,PublicDistanceWrapperKeepsSquaredFloatContract){PRCheck(PRGrid(VxDistance::PointRayDistance,true));float t;
    VxRay line;line.m_Origin=VxVector(0,0,0);line.m_Direction=VxVector(1,0,0);
    const VxVector p(2,std::ldexp(1.0f,100),0);EXPECT_EQ(VxDistance::PointRayDistance(p,line,&t),INFINITY);EXPECT_EQ(t,2);
}
#if defined(_WIN32)
TEST(PointRayDistanceReference,ActualCurrentDllExport){HMODULE dll=LoadLibraryA("VxMath.dll");ASSERT_NE(dll,nullptr);
#if defined(_WIN64)
    const char *name="?PointRaySquareDistance@VxDistance@@SAMAEBUVxVector@@AEBVVxRay@@PEAM@Z";
#else
    const char *name="?PointRaySquareDistance@VxDistance@@SAMABUVxVector@@ABVVxRay@@PAM@Z";
#endif
    PRFn fn=reinterpret_cast<PRFn>(GetProcAddress(dll,name));ASSERT_NE(fn,nullptr);PRCheck(PRGrid(fn));PRCheck(PRDirectionScales(fn));PRCheck(PRGeometryScales(fn));PRCheck(PRCancellation(fn));PRCheck(PRAliases(fn));PRCheck(PRZero(fn));PRCheck(PRBoundary(fn));PRCheck(PRRandom(fn));PRCheck(PRFootprint(fn));EXPECT_NE(FreeLibrary(dll),0);}
#endif
#endif
