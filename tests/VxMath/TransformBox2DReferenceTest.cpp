#include <float.h>
#include <math.h>
#include <string.h>
#include <new>
#ifndef VX_TRANSFORM_BOX_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#else
#include <xmmintrin.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef decltype(&VxTransformBox2D) TBFn;
struct TBResult { int calls=0, errors=0, faults=0; };
static unsigned TBBits(float value) { unsigned bits; memcpy(&bits,&value,4); return bits; }
static float TBFloat(unsigned bits) { float value; memcpy(&value,&bits,4); return value; }
static bool TBEqual(float a,float b) { return TBBits(a)==TBBits(b) || (a!=a && b!=b); }
static bool TBEqualRect(const VxRect &a,const VxRect &b) {
    return TBEqual(a.left,b.left) && TBEqual(a.top,b.top) && TBEqual(a.right,b.right) && TBEqual(a.bottom,b.bottom);
}
static void TBIdentity(VxMatrix &m) { memset(&m,0,sizeof(m)); for(int i=0;i<4;++i) m[i][i]=1; }
#include "TransformBox2DTestModel.h"

static int TBCall(TBFn fn,const VxMatrix &m,const VxBbox &b,VxRect *screen,VxRect *out,VXCLIP_FLAGS &combined,VXCLIP_FLAGS &common,bool &fault) {
#if defined(_WIN32)
    __try { return fn(m,b,screen,out,combined,common); }
    __except(EXCEPTION_EXECUTE_HANDLER) { fault=true; return -99; }
#else
    (void)fault; return fn(m,b,screen,out,combined,common);
#endif
}
static void TBCheck(TBResult &r,TBFn fn,const VxMatrix &m,const VxBbox &b,VxRect *screen,VxRect *out,int mode,bool shared=false) {
    VxRect expected,view;
    if(out) memcpy(&expected,out,sizeof(expected)); if(screen) memcpy(&view,screen,sizeof(view));
    VxRect *expectedView=screen ? (screen==out ? &expected : &view) : nullptr;
    const TBFlags flags=TBModel(m,b,expectedView,out ? &expected : nullptr,mode);
    VXCLIP_FLAGS combined=VXCLIP_FLAGS(0x12345678),common=VXCLIP_FLAGS(0x87654321u);
    bool fault=false; const int visible=TBCall(fn,m,b,screen,out,combined,shared ? combined : common,fault); ++r.calls; r.faults+=fault;
    r.errors+=fault || visible!=flags.visible || unsigned(combined)!=(shared ? flags.common : flags.combined) || (!shared && unsigned(common)!=flags.common) || (out && !TBEqualRect(*out,expected));
}
static TBResult TBClipBoundaries(TBFn fn,int mode) {
    TBResult r;
    for(int plane=0;plane<6;++plane) for(int side=0;side<3;++side) {
        VxMatrix m; TBIdentity(m); VxBbox b(VxVector(0,0,0),VxVector(0,0,0)); m[3][2]=0.5f;
        const float boundary=plane==0 || plane==2 ? -1.0f : plane==4 ? 0.0f : 1.0f;
        float value=boundary;
        if(side) {
            const unsigned bits=TBBits(boundary);
            value=boundary==0 ? TBFloat(side==1 ? 0x80000001u : 1u) : TBFloat(bits+(side==1 ? (boundary<0 ? 1 : -1) : (boundary<0 ? -1 : 1)));
        }
        m[3][plane<2 ? 0 : plane<4 ? 1 : 2]=value;
        VXCLIP_FLAGS combined=VXCLIP_FLAGS(0),common=VXCLIP_FLAGS(0); bool fault=false;
        const int visible=TBCall(fn,m,b,nullptr,nullptr,combined,common,fault); ++r.calls; r.faults+=fault;
        const unsigned flag=plane==0?0x10:plane==1?0x20:plane==2?0x80:plane==3?0x40:plane==4?0x100:0x200;
        const bool clipped=(plane==0||plane==2||plane==4) ? side==1 : side==2;
        r.errors+=fault || visible!=(clipped ? 0 : 1) || unsigned(combined)!=(clipped ? flag : 0) || combined!=common;
    }
    (void)mode; return r;
}
static TBResult TBClipMatrix(TBFn fn,int mode) {
    TBResult r; unsigned random=320;
    for(int i=0;i<768;++i) {
        VxMatrix m;
        for(int row=0;row<4;++row)for(int col=0;col<4;++col){random=random*1664525u+1013904223u;m[row][col]=float(int((random>>16)%9)-4);}
        VxBbox b(VxVector(-3,-2,-1),VxVector(4,3,5)); if(i%3==0)b.Max.z=b.Min.z;
        TBCheck(r,fn,m,b,nullptr,nullptr,mode);
    }
    return r;
}
static VxRect TBViewport(int kind) {
    if(kind==0)return VxRect(0,0,800,600);
    if(kind==1)return VxRect(-64,32,192,160);
    if(kind==2)return VxRect(128,64,-128,-64);
    return VxRect(7,11,7,11);
}
static TBResult TBProjection(TBFn fn,int mode) {
    TBResult r;
    for(int power=0;power<4;++power)for(int view=0;view<4;++view){
        VxMatrix m;TBIdentity(m);m[3][3]=float(1<<power);
        VxBbox b(VxVector(-0.5f,-0.25f,0.25f),VxVector(0.5f,0.25f,0.75f));
        VxRect screen=TBViewport(view),out(-37,-41,43,47);TBCheck(r,fn,m,b,&screen,&out,mode);
    }
    return r;
}
static void TBCase(int kind,VxMatrix &m,VxBbox &b) {
    TBIdentity(m); b=VxBbox(VxVector(-0.5f,-0.25f,0.5f),VxVector(0.5f,0.25f,0.5f));
    if(kind==1){b.Min.x=2;b.Max.x=3;}
    if(kind==2||kind==3){b.Min.x=b.Min.y=-2;b.Max.x=b.Max.y=2;b.Min.z=-1;b.Max.z=kind==2 ? -0.5f : 0.5f;}
    if(kind==4)m[3][3]=-1;
    if(kind==5){m[2][0]=2;m[2][3]=-1;m[3][2]=1;m[3][3]=0;b=VxBbox(VxVector(-0.25f,-0.25f,-0.5f),VxVector(0.75f,0.25f,0.5f));}
    if(kind==6){m[0][3]=1;m[3][3]=0;b.Min.x=0;b.Max.x=1;}
    if(kind==7){b.Max.z=b.Min.z+FLT_EPSILON*0.5f;m[2][0]=8388608.0f;m[3][0]=-4194304.0f;}
    if(kind==8){b.Min.x=-1;b.Max.x=1;b.Min.y=-1;b.Max.y=1;}
    if(kind==9){b=VxBbox(VxVector(0,0,0),VxVector(0,0,0));m[3][3]=0;}
    if(kind==10)b=VxBbox(VxVector(1,-1,1),VxVector(1,-1,1));
    if(kind==11){b=VxBbox(VxVector(0,0,0),VxVector(0,0,0));m[3][0]=TBFloat(0x7fc12345u);m[3][1]=0.25f;m[3][2]=0.5f;}
}
static TBResult TBCases(TBFn fn,int mode,int group) {
    TBResult r;
    for(int kind=0;kind<12;++kind)for(int view=0;view<(group==1 ? 4 : 1);++view){
        VxMatrix m;VxBbox b;TBCase(kind,m,b);VxRect screen=TBViewport(view),out(-37,-41,43,47);
        TBCheck(r,fn,m,b,&screen,group==1 ? &screen : &out,mode,group==2);
    }
    return r;
}
static TBResult TBNonfinite(TBFn fn,int mode) {
    TBResult r;const float special[]={TBFloat(0x7fc12345u),HUGE_VALF,-HUGE_VALF};
    for(int col=0;col<4;++col)for(float value:special){
        VxMatrix m;TBIdentity(m);m[3][2]=0.5f;m[3][col]=value;
        VxBbox b(VxVector(0,0,0),VxVector(0,0,0));VxRect screen=TBViewport(1),out(-37,-41,43,47);
        TBCheck(r,fn,m,b,&screen,&out,mode);
    }
    return r;
}
static void TBWideCase(int kind,VxMatrix &m,VxBbox &b,VxRect &screen,VxMatrix &oracle) {
    TBIdentity(m);b=VxBbox(VxVector(0,0,0),VxVector(0,0,0));screen=VxRect(-1,-1,1,1);m[3][2]=0.5f;
    if(kind==0){b=VxBbox(VxVector(-FLT_MAX,-0.25f,0.25f),VxVector(FLT_MAX,0.25f,0.75f));m[3][3]=FLT_MAX;}
    if(kind==1){b.Min.x=b.Max.x=FLT_MAX;m[0][0]=m[0][3]=2;m[3][3]=0;}
    if(kind==2){b.Min.x=b.Max.x=TBFloat(1);m[0][0]=m[0][3]=TBFloat(1);m[3][3]=0;m[3][2]=0;}
    if(kind==3||kind==4){b=VxBbox(VxVector(1,1,1),VxVector(1,1,1));m[3][0]=0.5f;m[0][0]=0;m[3][1]=-0.25f;m[1][1]=0;m[2][2]=0;m[0][3]=0x1p30f;m[1][3]=kind==3?1:-1;m[2][3]=-0x1p30f;m[3][3]=0;}
    if(kind==5){b=VxBbox(VxVector(-1,-1,0.5f),VxVector(1,1,0.5f));screen=VxRect(-FLT_MAX,-FLT_MAX,FLT_MAX,FLT_MAX);}
    if(kind==6){b=VxBbox(VxVector(1,1,1),VxVector(1,1,1));m[0][0]=0x1p30f;m[1][0]=1;m[2][0]=-0x1p30f;m[2][2]=0;}
    if(kind==7){b=VxBbox(VxVector(1,1,1),VxVector(1,1,1));m[3][3]=FLT_MAX;m[0][0]=2;m[1][0]=-2;b.Min.x=b.Max.x=FLT_MAX;b.Min.y=b.Max.y=FLT_MAX;m[3][0]=FLT_MAX;}
    memcpy(&oracle,&m,sizeof(m));
    // The cancelled homogeneous coordinate is analytically +/-1, independent
    // of the runtime's summation algorithm.
    if(kind==3||kind==4){for(int row=0;row<3;++row)oracle[row][3]=0;oracle[3][3]=kind==3?1:-1;}
    if(kind==6){for(int row=0;row<3;++row)oracle[row][0]=0;oracle[3][0]=1;}
}
static TBResult TBWide(TBFn fn) {
    TBResult r;
    for(int kind=0;kind<8;++kind){
        VxMatrix m,oracle;VxBbox b;VxRect screen;TBWideCase(kind,m,b,screen,oracle);
        VxRect expected(-37,-41,43,47),view=screen,out=expected;
        const TBFlags flags=TBModel(oracle,b,&view,&expected,0);
        VXCLIP_FLAGS combined=VXCLIP_FLAGS(0),common=VXCLIP_FLAGS(0);bool fault=false;
        const int visible=TBCall(fn,m,b,&screen,&out,combined,common,fault);++r.calls;r.faults+=fault;
        r.errors+=fault||visible!=flags.visible||unsigned(combined)!=flags.combined||unsigned(common)!=flags.common||!TBEqualRect(out,expected);
    }
    return r;
}
static TBResult TBUnused(TBFn fn,int mode) {
    TBResult r;
#if defined(_WIN32)
    SYSTEM_INFO info;GetSystemInfo(&info);void *bad=VirtualAlloc(nullptr,info.dwPageSize,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS);
    if(!bad){++r.errors;return r;}
    for(int kind=0;kind<4;++kind){
        VxMatrix m;VxBbox b;TBCase(kind,m,b);const TBFlags flags=TBModel(m,b,nullptr,nullptr,mode);
        VXCLIP_FLAGS combined=VXCLIP_FLAGS(0),common=VXCLIP_FLAGS(0);bool fault=false;
        const int visible=TBCall(fn,m,b,static_cast<VxRect *>(bad),nullptr,combined,common,fault);++r.calls;r.faults+=fault;
        r.errors+=fault||visible!=flags.visible||unsigned(combined)!=flags.combined||unsigned(common)!=flags.common;
    }
    VirtualFree(bad,0,MEM_RELEASE);
#endif
    return r;
}
static TBResult TBFootprint(TBFn fn,int mode) {
    TBResult r;
#if defined(_WIN32)
    SYSTEM_INFO info;GetSystemInfo(&info);const size_t page=info.dwPageSize;unsigned char *memory[6]={};
    for(int i=0;i<6;++i){memory[i]=static_cast<unsigned char *>(VirtualAlloc(nullptr,3*page,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS));if(!memory[i]){++r.errors;for(int j=0;j<i;++j)VirtualFree(memory[j],0,MEM_RELEASE);return r;}}
    for(int kind=0;kind<4;++kind)for(unsigned edges=0;edges<64;++edges){
        DWORD old;for(int i=0;i<6;++i)r.errors+=!VirtualProtect(memory[i]+page,page,PAGE_READWRITE,&old);
        VxMatrix m;VxBbox b;TBCase(kind,m,b);VxRect view=TBViewport(1),expected(-37,-41,43,47),modelView=view;
        const TBFlags flags=TBModel(m,b,&modelView,&expected,mode);
        const size_t sizes[]={sizeof(m),sizeof(b),sizeof(view),sizeof(expected),4,4};unsigned char *at[6];for(int i=0;i<6;++i)at[i]=memory[i]+page+((edges&(1u<<i)) ? page-sizes[i] : 0);
        VxMatrix *matrix=::new(at[0])VxMatrix(m);VxBbox *box=::new(at[1])VxBbox(b);VxRect *screen=::new(at[2])VxRect(view),*out=::new(at[3])VxRect(-37,-41,43,47);
        VXCLIP_FLAGS *combined=::new(at[4])VXCLIP_FLAGS(VXCLIP_FLAGS(0)),*common=::new(at[5])VXCLIP_FLAGS(VXCLIP_FLAGS(0));
        for(int i=0;i<3;++i)r.errors+=!VirtualProtect(memory[i]+page,page,PAGE_READONLY,&old);
        bool fault=false;const int visible=TBCall(fn,*matrix,*box,screen,out,*combined,*common,fault);++r.calls;r.faults+=fault;
        r.errors+=fault||visible!=flags.visible||unsigned(*combined)!=flags.combined||unsigned(*common)!=flags.common||!TBEqualRect(*out,expected)||memcmp(matrix,&m,sizeof(m))||memcmp(box,&b,sizeof(b))||memcmp(screen,&view,sizeof(view));
    }
    for(int i=0;i<6;++i)VirtualFree(memory[i],0,MEM_RELEASE);
#endif
    return r;
}
#ifndef VX_TRANSFORM_BOX_PROBE
#if defined(VX_SIMD_SSE)
static XBOOL TBRaw(const VxMatrix &m,const VxBbox &b,VxRect *screen,VxRect *out,VXCLIP_FLAGS &combined,VXCLIP_FLAGS &common){return VxSIMDTransformBox2D(&m,&b,out,screen,&combined,&common);}
#endif
class TransformBox2DReference:public testing::TestWithParam<int> {
protected:
    int previous=VX_SIMD_MODE_AUTO;
    void SetUp()override{previous=VxGetSIMDOverride();int wanted=GetParam()==0?VX_SIMD_MODE_NONE:VX_SIMD_MODE_SSE2;ASSERT_TRUE(VxSetSIMDOverride(wanted));ASSERT_EQ(VxGetSIMDEffectiveBackend(),wanted);}
    void TearDown()override{EXPECT_TRUE(VxSetSIMDOverride(previous));}
    TBFn Function()const{
#if defined(VX_SIMD_SSE)
        if(GetParam()==2)return TBRaw;
#endif
        return VxTransformBox2D;
    }
    void Check(TBResult r,int count){EXPECT_EQ(r.calls,count);EXPECT_EQ(r.errors+r.faults,0);}
};
TEST_P(TransformBox2DReference, ClipStrictlyAtAllSixPlanes){Check(TBClipBoundaries(Function(),0),18);}
TEST_P(TransformBox2DReference, ClassifyIndependentIntegerCorners){Check(TBClipMatrix(Function(),0),768);}
TEST_P(TransformBox2DReference, ProjectExactlyAcrossViewportOrientations){Check(TBProjection(Function(),0),16);}
TEST_P(TransformBox2DReference, KeepCullingNegativeWAndNearPlaneRules){Check(TBCases(Function(),0,0),12);}
TEST_P(TransformBox2DReference, ReadViewportBeforeWritingTheSameRectangle){Check(TBCases(Function(),0,1),48);}
TEST_P(TransformBox2DReference, KeepReturnValueWhenFlagOutputsAlias){Check(TBCases(Function(),0,2),12);}
TEST_P(TransformBox2DReference, UseOrderedClipComparisonsForNonfiniteCoordinates){Check(TBNonfinite(Function(),0),12);}
TEST_P(TransformBox2DReference, PreserveFiniteGeometryAndViewportRange){Check(TBWide(Function()),8);}
#if defined(_WIN32)
TEST_P(TransformBox2DReference, AvoidUnusedViewportReads){Check(TBUnused(Function(),0),4);}
TEST_P(TransformBox2DReference, RespectGuardedReadonlyInputsAndExactOutputs){Check(TBFootprint(Function(),0),256);}
#endif
TEST_P(TransformBox2DReference, KeepOptionalViewportAndInvalidBoxExtensions){
    for(int axis=0;axis<3;++axis){VxMatrix m;TBIdentity(m);VxBbox b(VxVector(0,0,0),VxVector(1,1,1));(&b.Min.x)[axis]=2;VxRect screen=TBViewport(0),out(-37,-41,43,47),saved=out;VXCLIP_FLAGS combined=VXCLIP_FLAGS(0),common=VXCLIP_FLAGS(0);EXPECT_FALSE(Function()(m,b,&screen,&out,combined,common));EXPECT_EQ(combined,VXCLIP_ALL);EXPECT_EQ(common,VXCLIP_ALL);EXPECT_TRUE(TBEqualRect(out,saved));}
    for(int kind=0;kind<3;++kind){VxMatrix m;VxBbox b;TBCase(kind,m,b);VxRect out(-37,-41,43,47);TBResult r;TBCheck(r,Function(),m,b,nullptr,&out,0);Check(r,1);}
}
#if defined(VX_SIMD_SSE)
INSTANTIATE_TEST_SUITE_P(ScalarSSE2AndRaw,TransformBox2DReference,testing::Values(0,1,2));
#else
INSTANTIATE_TEST_SUITE_P(ScalarOnly,TransformBox2DReference,testing::Values(0));
#endif
#endif
