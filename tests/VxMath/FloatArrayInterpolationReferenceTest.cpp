#include <math.h>
#include <string.h>
#include <float.h>
#include <limits.h>
#ifndef VX_FLOAT_ARRAY_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef void (*FAFn)(void *,void *,void *,float,int);
struct FAResult { int calls=0,errors=0,faults=0; };
static float FAReference(float a,float b,float t) {
    if(t==0)return a;
    if(t==1)return b;
    // Independent affine combination; bounded dyadic fixtures are exact.
    return float(double(a)*(1.0-double(t))+double(b)*t);
}
static FAResult FABasic(FAFn fn) {
    FAResult r;const float steps[]={-2,-0.25f,0,0.25f,0.5f,0.75f,1,1.5f,2};
    for(int count=1;count<=17;++count)for(int offset=0;offset<4;++offset)for(float t:steps) {
        alignas(16) float a[24],b[24],out[24],expected[24];
        for(int i=0;i<24;++i){a[i]=float(i-7)*0.25f;b[i]=float(15-i)*0.5f;out[i]=expected[i]=-517;}
        for(int i=0;i<count;++i)expected[offset+i]=FAReference(a[offset+i],b[offset+i],t);
        fn(out+offset,a+offset,b+offset,t,count);++r.calls;
        r.errors+=memcmp(out,expected,sizeof(out))!=0;
        for(int i=0;i<24;++i)r.errors+=a[i]!=float(i-7)*0.25f||b[i]!=float(15-i)*0.5f;
    }
    return r;
}
static FAResult FAOverlap(FAFn fn) {
    FAResult r;const int counts[]={1,2,3,4,5,7,8,9,15,16,17};const float steps[]={0,0.5f,1};
    for(int da=-6;da<=6;++da)for(int db=-6;db<=6;++db)for(int count:counts)for(float t:steps) {
        float pool[96],model[96];for(int i=0;i<96;++i)pool[i]=model[i]=float(i%17-8)*0.25f;
        const int out=32,a=out-da,b=out-db;
        // Original forward dataflow: each subsequent read sees prior writes.
        for(int i=0;i<count;++i)model[out+i]=FAReference(model[a+i],model[b+i],t);
        fn(pool+out,pool+a,pool+b,t,count);++r.calls;
        r.errors+=memcmp(pool,model,sizeof(pool))!=0;
    }
    return r;
}
static FAResult FARange(FAFn fn) {
    FAResult r;
    const float av[]={-FLT_MAX,ldexpf(1.0f,-149),FLT_MAX,FLT_MAX,FLT_MAX};
    const float bv[]={FLT_MAX,ldexpf(1.0f,-148),1,FLT_MAX,-FLT_MAX};
    const float tv[]={0.5f,0.5f,1,FLT_MAX,0};
    const float result[]={0,ldexpf(1.0f,-148),1,FLT_MAX,FLT_MAX};
    for(int kind=0;kind<5;++kind)for(int count=1;count<=9;++count) {
        float a[12],b[12],out[12];for(int i=0;i<12;++i){a[i]=av[kind];b[i]=bv[kind];out[i]=-517;}
        fn(out,a,b,tv[kind],count);++r.calls;bool ok=true;
        for(int i=0;i<12;++i)ok=ok&&out[i]==(i<count?result[kind]:-517);
        r.errors+=!ok;
    }
    return r;
}
static FAResult FAEndpoints(FAFn fn) {
    FAResult r;const unsigned words[]={0,0x80000000u,1,0x007fffffu,0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,0x7fc12345u,0x7f800001u};
    for(int endpoint=0;endpoint<2;++endpoint)for(int count=1;count<=10;++count) {
        float a[12],b[12],out[12];unsigned expected[12];
        for(int i=0;i<12;++i){const unsigned selected=words[i%10],other=0x7fc54321u;memcpy(a+i,endpoint?&other:&selected,4);memcpy(b+i,endpoint?&selected:&other,4);expected[i]=i<count?selected:0xdeadbeefu;unsigned sentinel=0xdeadbeefu;memcpy(out+i,&sentinel,4);}
        fn(out,a,b,float(endpoint),count);++r.calls;r.errors+=memcmp(out,expected,48)!=0;
    }
    const int distances[]={0,1,2,3,4,16,64,80};
    for(int endpoint=0;endpoint<2;++endpoint)for(int distance:distances) {
        float pool[512];unsigned model[512];
        for(int i=0;i<512;++i){model[i]=words[i%10];memcpy(pool+i,model+i,4);}
        const int dest=160,selected=dest-distance;
        for(int i=0;i<65;++i)model[dest+i]=model[selected+i];
        float *a=pool+(endpoint?dest-1:selected),*b=pool+(endpoint?selected:dest-1);
        fn(pool+dest,a,b,float(endpoint),65);++r.calls;
        r.errors+=memcmp(pool,model,sizeof(pool))!=0;
    }
    return r;
}
#if defined(_WIN32)
static bool FAGuardCall(FAFn fn,float *out,float *a,float *b,float factor,int count) {
    __try {fn(out,a,b,factor,count);return true;} __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
#endif
static FAResult FAFootprint(FAFn fn) {
    FAResult r;
#if defined(_WIN32)
    SYSTEM_INFO info;GetSystemInfo(&info);const size_t page=info.dwPageSize;unsigned char *blocks[3]={};
    for(int i=0;i<3;++i){blocks[i]=static_cast<unsigned char *>(VirtualAlloc(nullptr,page*3,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));if(!blocks[i]){++r.errors;for(int j=0;j<i;++j)VirtualFree(blocks[j],0,MEM_RELEASE);return r;}}
    const int counts[]={1,2,3,4,5,6,7,8,9,15,16,17,64,65,256,257};const float steps[]={0,0.5f,1};
    for(int edge=0;edge<2;++edge)for(int count:counts)for(float t:steps) {
        DWORD old;float *p[3];
        for(int k=0;k<3;++k){VirtualProtect(blocks[k],page*3,PAGE_READWRITE,&old);p[k]=reinterpret_cast<float *>(blocks[k]+(edge?2*page-count*4:page));}
        for(int i=0;i<count;++i){p[0][i]=-517;p[1][i]=float(2*i);p[2][i]=float(2*i+4);}
        for(int k=0;k<3;++k){VirtualProtect(blocks[k],page,PAGE_NOACCESS,&old);VirtualProtect(blocks[k]+2*page,page,PAGE_NOACCESS,&old);if(k)VirtualProtect(blocks[k]+page,page,PAGE_READONLY,&old);}
        ++r.calls;const bool ok=FAGuardCall(fn,p[0],p[1],p[2],t,count);r.faults+=!ok;r.errors+=!ok;
        if(ok)for(int i=0;i<count;++i)r.errors+=p[0][i]!=float(2*i+4*t)||p[1][i]!=float(2*i)||p[2][i]!=float(2*i+4);
    }
    for(int i=0;i<3;++i)VirtualFree(blocks[i],0,MEM_RELEASE);
#endif
    return r;
}
static FAResult FAEmpty(FAFn fn,bool publicEntry) {
    FAResult r;
#if defined(_WIN32)
    SYSTEM_INFO info;GetSystemInfo(&info);const size_t page=info.dwPageSize;
    unsigned char *bytes=static_cast<unsigned char *>(VirtualAlloc(nullptr,page*3,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS));if(!bytes){++r.errors;return r;}
    float *bad=reinterpret_cast<float *>(bytes+page);const int counts[]={0,-1,INT_MIN};
    for(int count:counts){++r.calls;const bool ok=FAGuardCall(fn,bad,bad,bad,0.5f,count);r.faults+=!ok;r.errors+=!ok;}
    if(publicEntry)for(int slot=0;slot<3;++slot){float *p[]={bad,bad,bad};p[slot]=nullptr;++r.calls;const bool ok=FAGuardCall(fn,p[0],p[1],p[2],0.5f,1);r.faults+=!ok;r.errors+=!ok;}
    VirtualFree(bytes,0,MEM_RELEASE);
#endif
    return r;
}
#ifndef VX_FLOAT_ARRAY_PROBE
#if defined(VX_SIMD_SSE)
static void FARaw(void *out,void *a,void *b,float t,int n) {VxSIMDInterpolateFloatArray(static_cast<float *>(out),static_cast<const float *>(a),static_cast<const float *>(b),t,n);}
#endif
class FloatArrayInterpolationReference : public testing::TestWithParam<int> {
protected:
    int previous=VX_SIMD_MODE_AUTO;
    void SetUp() override {
        previous=VxGetSIMDOverride();const int wanted=GetParam()==0?VX_SIMD_MODE_NONE:VX_SIMD_MODE_SSE2;
        ASSERT_TRUE(VxSetSIMDOverride(wanted));ASSERT_EQ(VxGetSIMDEffectiveBackend(),wanted);
    }
    void TearDown() override {EXPECT_TRUE(VxSetSIMDOverride(previous));}
    FAFn Function() const {
#if defined(VX_SIMD_SSE)
        if(GetParam()==2)return FARaw;
#endif
        return InterpolateFloatArray;
    }
};
TEST_P(FloatArrayInterpolationReference, InterpolateDyadicSamplesAndExtrapolate) {EXPECT_EQ(FABasic(Function()).errors,0);}
TEST_P(FloatArrayInterpolationReference, PreserveSequentialForwardOverlap) {EXPECT_EQ(FAOverlap(Function()).errors,0);}
TEST_P(FloatArrayInterpolationReference, RetainFiniteRangeAndSubnormalRounding) {EXPECT_EQ(FARange(Function()).errors,0);}
TEST_P(FloatArrayInterpolationReference, CopyEndpointBitsExactly) {EXPECT_EQ(FAEndpoints(Function()).errors,0);}
TEST_P(FloatArrayInterpolationReference, RespectReadOnlySourcesAndExactArrayBounds) {const FAResult r=FAFootprint(Function());EXPECT_EQ(r.errors+r.faults,0);}
TEST_P(FloatArrayInterpolationReference, EmptyCountsAccessNothingAndKeepExistingNullGuards) {const FAResult r=FAEmpty(Function(),GetParam()!=2);EXPECT_EQ(r.errors+r.faults,0);}
#if defined(VX_SIMD_SSE)
INSTANTIATE_TEST_SUITE_P(ScalarSSE2AndRaw,FloatArrayInterpolationReference,testing::Values(0,1,2));
#else
INSTANTIATE_TEST_SUITE_P(ScalarOnly,FloatArrayInterpolationReference,testing::Values(0));
#endif
#endif
