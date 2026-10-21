#include <climits>
#include <cstring>
#include <cstdio>
#include <cstddef>
#include <new>
#include <type_traits>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#ifndef VX_INTEGER_REGION_NATIVE_PROBE
#include "VxMath.h"
#include "VxSIMDMode.h"
#include <gtest/gtest.h>
#else
#include "C:/Users/kakut/Works/Virtools/Virtools-SDK-2.1/Include/VxMathDefines.h"
#endif

typedef XBOOL (*IntegerPointCall)(CKRECT *,CKPOINT *);
struct IntegerRegionSamples { int calls=0,errors=0; unsigned digest=2166136261u; };

static void ObserveIntegerRegion(IntegerPointCall call,CKRECT &rect,CKPOINT &point,int expected,IntegerRegionSamples &r) {
    const CKRECT before=rect;const CKPOINT old=point;
    const int actual=call(&rect,&point);
    if (actual!=expected || std::memcmp(&before,&rect,sizeof(rect)) || std::memcmp(&old,&point,sizeof(point))) ++r.errors;
    unsigned value=unsigned(actual);
    for (int i=0;i<4;++i) { r.digest=(r.digest^(value&255u))*16777619u;value>>=8; }
    ++r.calls;
}

// Literal closed-interval membership on nine ordered coordinate ranks.
// Lower bounds after upper bounds have an empty membership mask.
static const unsigned IntegerRegionMembership[9][9]={
    {1,3,7,15,31,63,127,255,511},
    {0,2,6,14,30,62,126,254,510},
    {0,0,4,12,28,60,124,252,508},
    {0,0,0,8,24,56,120,248,504},
    {0,0,0,0,16,48,112,240,496},
    {0,0,0,0,0,32,96,224,480},
    {0,0,0,0,0,0,64,192,448},
    {0,0,0,0,0,0,0,128,384},
    {0,0,0,0,0,0,0,0,256}
};

static IntegerRegionSamples IntegerRegionMatrix(IntegerPointCall call) {
    IntegerRegionSamples result;
    const int coordinate[]={INT_MIN,INT_MIN+1,-16777217,-1,0,1,16777217,INT_MAX-1,INT_MAX};
    for (int l=0;l<9;++l) for (int t=0;t<9;++t)
    for (int r=0;r<9;++r) for (int b=0;b<9;++b) {
        CKRECT rect={coordinate[l],coordinate[t],coordinate[r],coordinate[b]};
        for (int x=0;x<9;++x) for (int y=0;y<9;++y) {
            CKPOINT point={coordinate[x],coordinate[y]};
            const int expected=bool(IntegerRegionMembership[l][r]&(1u<<x)) && bool(IntegerRegionMembership[t][b]&(1u<<y));
            ObserveIntegerRegion(call,rect,point,expected,result);
        }
    }
    return result;
}

static IntegerRegionSamples IntegerRegionLiterals(IntegerPointCall call) {
    IntegerRegionSamples result;CKRECT rect={10,20,30,40};
    CKPOINT points[]={{10,20},{30,20},{10,40},{30,40},{20,30},{9,30},{31,30},{20,19},{20,41}};
    for (int i=0;i<9;++i) ObserveIntegerRegion(call,rect,points[i],i<5,result);
    rect={10,20,10,40};CKPOINT vertical={10,30};ObserveIntegerRegion(call,rect,vertical,TRUE,result);
    rect={10,20,30,20};CKPOINT horizontal={20,20};ObserveIntegerRegion(call,rect,horizontal,TRUE,result);
    rect={10,20,10,20};CKPOINT single={10,20};ObserveIntegerRegion(call,rect,single,TRUE,result);
    rect={30,20,10,40};ObserveIntegerRegion(call,rect,single,FALSE,result);
    rect={10,40,30,20};ObserveIntegerRegion(call,rect,single,FALSE,result);
    return result;
}

static IntegerRegionSamples IntegerRegionFourByteAlignment(IntegerPointCall call) {
    alignas(16) unsigned char rectangle[32],point[16];
    CKRECT *r=new(rectangle+4) CKRECT{10,20,30,40};CKPOINT *p=new(point+4) CKPOINT{30,40};
    IntegerRegionSamples result;ObserveIntegerRegion(call,*r,*p,TRUE,result);
    p->x=31;ObserveIntegerRegion(call,*r,*p,FALSE,result);
    return result;
}

#if defined(_WIN32)
struct IntegerRegionPages {
    unsigned char *base=nullptr;size_t page=0;
    IntegerRegionPages() {
        SYSTEM_INFO info;GetSystemInfo(&info);page=info.dwPageSize;
        base=static_cast<unsigned char *>(VirtualAlloc(nullptr,page*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        if (base) {
            DWORD old=0;
            if (!VirtualProtect(base+page,page,PAGE_NOACCESS,&old)) { VirtualFree(base,0,MEM_RELEASE);base=nullptr; }
        }
    }
    ~IntegerRegionPages() { if (base) VirtualFree(base,0,MEM_RELEASE); }
};

static IntegerRegionSamples IntegerRegionExactFootprints(IntegerPointCall call) {
    IntegerRegionPages rectangle,point;IntegerRegionSamples result;
    if (!rectangle.base || !point.base) { ++result.errors;return result; }
    CKRECT *r=new(rectangle.base+rectangle.page-sizeof(CKRECT)) CKRECT{10,20,30,40};
    CKPOINT *p=new(point.base+point.page-sizeof(CKPOINT)) CKPOINT{30,40};
    ObserveIntegerRegion(call,*r,*p,TRUE,result);p->y=41;ObserveIntegerRegion(call,*r,*p,FALSE,result);
    return result;
}
#endif

#ifndef VX_INTEGER_REGION_NATIVE_PROBE
static void RecordIntegerRegion(const IntegerRegionSamples &r) {
    char digest[16];std::snprintf(digest,sizeof(digest),"%u",r.digest);
    EXPECT_EQ(r.errors,0);testing::Test::RecordProperty("calls",r.calls);testing::Test::RecordProperty("digest",digest);
}

class IntegerRegionDispatchTest:public testing::TestWithParam<int> {
    int previous=VX_SIMD_MODE_AUTO;
    void SetUp() override { previous=VxGetSIMDOverride();ASSERT_TRUE(VxSetSIMDOverride(GetParam()));RecordProperty("effective_mode",VxGetSIMDEffectiveBackend()); }
    void TearDown() override { EXPECT_TRUE(VxSetSIMDOverride(previous)); }
};

TEST_P(IntegerRegionDispatchTest, SignedBoundaryMatrixUsesAllFourClosedBorders) {
    const IntegerRegionSamples r=IntegerRegionMatrix(VxPtInRect);EXPECT_EQ(r.calls,531441);RecordIntegerRegion(r);
}
TEST_P(IntegerRegionDispatchTest, DegenerateAndInvertedRectanglesKeepOriginalSemantics) {
    const IntegerRegionSamples r=IntegerRegionLiterals(VxPtInRect);EXPECT_EQ(r.calls,14);RecordIntegerRegion(r);
}
TEST_P(IntegerRegionDispatchTest, FourByteAlignedObjectsDoNotRequireSixteenByteAlignment) {
    const IntegerRegionSamples r=IntegerRegionFourByteAlignment(VxPtInRect);EXPECT_EQ(r.calls,2);RecordIntegerRegion(r);
}
#if defined(_WIN32)
TEST_P(IntegerRegionDispatchTest, ExactPointAndRectangleFootprintsEndAtInaccessiblePages) {
    const IntegerRegionSamples r=IntegerRegionExactFootprints(VxPtInRect);EXPECT_EQ(r.calls,2);RecordIntegerRegion(r);
}
#endif
INSTANTIATE_TEST_SUITE_P(Backends,IntegerRegionDispatchTest,testing::Values(1,2,3,4,5,6));

#if defined(VX_SIMD_SSE)
static XBOOL IntegerRegionInline(CKRECT *r,CKPOINT *p) { return VxSIMDPtInRect(r,p); }
TEST(IntegerRegionReferenceTest, DirectInlineSimdMatchesTheSignedBoundaryMatrix) {
    const IntegerRegionSamples r=IntegerRegionMatrix(IntegerRegionInline);EXPECT_EQ(r.calls,531441);RecordIntegerRegion(r);
    RecordProperty("compiled_sse",1);
}
TEST(IntegerRegionReferenceTest, DirectInlineSimdAcceptsFourByteAlignedObjects) {
    RecordIntegerRegion(IntegerRegionFourByteAlignment(IntegerRegionInline));
}
#if defined(_WIN32)
TEST(IntegerRegionReferenceTest, DirectInlineSimdReadsOnlyTheDeclaredObjectFootprints) {
    const IntegerRegionSamples r=IntegerRegionExactFootprints(IntegerRegionInline);EXPECT_EQ(r.calls,2);RecordIntegerRegion(r);
}
#endif
#endif

TEST(IntegerRegionReferenceTest, PointLayoutHasTwoSigned32BitFieldsAndTrivialValueCopies) {
    EXPECT_EQ(sizeof(int),4u);EXPECT_TRUE(std::is_signed<int>::value);
    EXPECT_EQ(sizeof(CKPOINT),8u);EXPECT_EQ(alignof(CKPOINT),4u);EXPECT_EQ(offsetof(CKPOINT,x),0u);EXPECT_EQ(offsetof(CKPOINT,y),4u);
    EXPECT_TRUE(std::is_standard_layout<CKPOINT>::value);EXPECT_TRUE(std::is_trivially_copyable<CKPOINT>::value);EXPECT_TRUE(std::is_trivially_destructible<CKPOINT>::value);
    const CKPOINT original={INT_MIN,INT_MAX};CKPOINT copy=original,assigned={};assigned=original;copy.x=0;assigned.y=1;
    EXPECT_EQ(original.x,INT_MIN);EXPECT_EQ(original.y,INT_MAX);EXPECT_EQ(copy.y,INT_MAX);EXPECT_EQ(assigned.x,INT_MIN);
    int raw[2];std::memcpy(raw,&original,sizeof(raw));EXPECT_EQ(raw[0],INT_MIN);EXPECT_EQ(raw[1],INT_MAX);
}

TEST(IntegerRegionReferenceTest, RectangleLayoutHasFourSigned32BitFieldsAndTrivialValueCopies) {
    EXPECT_EQ(sizeof(CKRECT),16u);EXPECT_EQ(alignof(CKRECT),4u);
    EXPECT_EQ(offsetof(CKRECT,left),0u);EXPECT_EQ(offsetof(CKRECT,top),4u);EXPECT_EQ(offsetof(CKRECT,right),8u);EXPECT_EQ(offsetof(CKRECT,bottom),12u);
    EXPECT_TRUE(std::is_standard_layout<CKRECT>::value);EXPECT_TRUE(std::is_trivially_copyable<CKRECT>::value);EXPECT_TRUE(std::is_trivially_destructible<CKRECT>::value);
    const CKRECT original={INT_MIN,-16777217,16777217,INT_MAX};CKRECT copy=original,assigned={};assigned=original;copy.left=0;assigned.bottom=0;
    EXPECT_EQ(original.left,INT_MIN);EXPECT_EQ(original.bottom,INT_MAX);EXPECT_EQ(copy.right,16777217);EXPECT_EQ(assigned.top,-16777217);
    int raw[4];std::memcpy(raw,&original,sizeof(raw));EXPECT_EQ(raw[0],INT_MIN);EXPECT_EQ(raw[1],-16777217);EXPECT_EQ(raw[2],16777217);EXPECT_EQ(raw[3],INT_MAX);
}

TEST(IntegerRegionReferenceTest, OverrideTransitionsKeepTheSameIntegerPointContract) {
    const int previous=VxGetSIMDOverride();unsigned effective=0;
    for (int n=0;n<18;++n) {
        EXPECT_TRUE(VxSetSIMDOverride(1+n%6));EXPECT_EQ(VxGetSIMDOverride(),1+n%6);
        effective|=1u<<VxGetSIMDEffectiveBackend();RecordIntegerRegion(IntegerRegionLiterals(VxPtInRect));
    }
    RecordProperty("effective_modes",int(effective));EXPECT_TRUE(VxSetSIMDOverride(previous));
}
#endif
