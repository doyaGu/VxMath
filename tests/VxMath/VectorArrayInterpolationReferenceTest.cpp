#include <math.h>
#include <string.h>
#include <float.h>
#include <limits.h>
#ifndef VX_VECTOR_ARRAY_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef decltype(&InterpolateVectorArray) VAFn;
struct VAResult {int calls=0,errors=0,faults=0;};
static float VAReference(float a,float b,float t) {
    if(t==0)return a;
    if(t==1)return b;
    return float(double(a)*(1.0-double(t))+double(b)*t);
}
static void VAModel(unsigned char *out,const unsigned char *a,const unsigned char *b,float t,int count,unsigned sr,unsigned si) {
    for(int i=0;i<count;++i) {
        float av[3],bv[3],v[3];memcpy(av,a,12);memcpy(bv,b,12);
        for(int k=0;k<3;++k)v[k]=VAReference(av[k],bv[k],t);
        // The original snapshots XYZ, then stores Z,Y,X before advancing.
        for(int k=2;k>=0;--k)memcpy(out+4*k,v+k,4);
        a+=si;b+=si;out+=sr;
    }
}
static VAResult VABasic(VAFn fn) {
    VAResult r;const int counts[]={1,2,3,4,5,7,8,9,16,17};
    const unsigned strides[]={0,4,8,12,16,20,32};const float steps[]={-2,-0.25f,0,0.5f,1,1.5f};
    for(int count:counts)for(unsigned si:strides)for(unsigned sr:strides)for(float t:steps) {
        float a[160],b[160],out[160],expected[160];
        for(int i=0;i<160;++i){a[i]=float(i%23-11)*0.25f;b[i]=float(17-i%29)*0.5f;out[i]=expected[i]=-517;}
        VAModel(reinterpret_cast<unsigned char *>(expected+1),reinterpret_cast<unsigned char *>(a+2),reinterpret_cast<unsigned char *>(b+3),t,count,sr,si);
        fn(out+1,a+2,b+3,t,count,sr,si);++r.calls;r.errors+=memcmp(out,expected,sizeof(out))!=0;
        for(int i=0;i<160;++i)r.errors+=a[i]!=float(i%23-11)*0.25f||b[i]!=float(17-i%29)*0.5f;
    }
    return r;
}
static VAResult VAOverlap(VAFn fn) {
    VAResult r;const unsigned strides[][2]={{12,12},{16,16},{20,12},{12,20},{0,12},{12,0},{0,0},{4,8},{8,4},{16,28},{32,16},{8,8}};
    const int counts[]={1,2,3,4,5,7,8,9,17};const float steps[]={0,0.5f,1};
    for(const auto &stride:strides)for(int da=-4;da<=4;++da)for(int db=-4;db<=4;++db)for(int count:counts)for(float t:steps) {
        float pool[256],model[256];for(int i=0;i<256;++i)pool[i]=model[i]=float(i%17-8)*0.25f;
        const int out=64,a=out-da,b=out-db;
        VAModel(reinterpret_cast<unsigned char *>(model+out),reinterpret_cast<unsigned char *>(model+a),reinterpret_cast<unsigned char *>(model+b),t,count,stride[0],stride[1]);
        fn(pool+out,pool+a,pool+b,t,count,stride[0],stride[1]);++r.calls;r.errors+=memcmp(pool,model,sizeof(pool))!=0;
    }
    return r;
}
static VAResult VAByteStrides(VAFn fn) {
    VAResult r;const unsigned strides[]={1,2,3,5,7,11,13,15,17,31};const int counts[]={1,2,3,4,7,9};const float steps[]={0,0.5f,1};
    for(unsigned si:strides)for(unsigned sr:strides)for(int count:counts)for(float t:steps) {
        unsigned char a[512],b[512],out[512],model[512];memset(a,0x3f,sizeof(a));memset(b,0x40,sizeof(b));memset(out,0xcd,sizeof(out));memcpy(model,out,sizeof(out));
        VAModel(model+1,a+2,b+3,t,count,sr,si);fn(out+1,a+2,b+3,t,count,sr,si);++r.calls;r.errors+=memcmp(model,out,sizeof(out))!=0;
        for(int i=0;i<512;++i)r.errors+=a[i]!=0x3f||b[i]!=0x40;
    }
    return r;
}
static VAResult VAEndpoints(VAFn fn) {
    VAResult r;const unsigned words[]={0,0x80000000u,1,0x007fffffu,0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,0x7fc12345u,0x7f800001u};
    const unsigned strides[][2]={{12,12},{16,20},{20,16},{0,0},{4,8},{8,4},{3,5},{13,17}};const int counts[]={1,4,9,65};
    for(const auto &stride:strides)for(int da=-5;da<=5;++da)for(int db=-5;db<=5;++db)for(int count:counts)for(int endpoint=0;endpoint<2;++endpoint) {
        unsigned char pool[2048],model[2048];for(int i=0;i<512;++i)memcpy(pool+4*i,words+i%10,4);memcpy(model,pool,sizeof(pool));
        const int out=128,a=out-da,b=out-db;
        for(int i=0;i<count;++i){unsigned char selected[12];memcpy(selected,model+(endpoint?b:a)+i*stride[1],12);for(int k=2;k>=0;--k)memcpy(model+out+i*stride[0]+4*k,selected+4*k,4);}
        fn(pool+out,pool+a,pool+b,float(endpoint),count,stride[0],stride[1]);++r.calls;r.errors+=memcmp(model,pool,sizeof(pool))!=0;
    }
    return r;
}
static VAResult VARange(VAFn fn) {
    VAResult r;const float av[]={-FLT_MAX,ldexpf(1.0f,-149),FLT_MAX,FLT_MAX,FLT_MAX};
    const float bv[]={FLT_MAX,ldexpf(1.0f,-148),1,FLT_MAX,-FLT_MAX};const float tv[]={0.5f,0.5f,1,FLT_MAX,0};
    const float expected[]={0,ldexpf(1.0f,-148),1,FLT_MAX,FLT_MAX};
    const unsigned strides[]={12,16};
    for(int kind=0;kind<5;++kind)for(int count=1;count<=9;++count)for(unsigned stride:strides) {
        float a[40],b[40],out[40],model[40];for(int i=0;i<40;++i){a[i]=av[kind];b[i]=bv[kind];out[i]=model[i]=-517;}
        for(int i=0;i<count;++i)for(int k=0;k<3;++k)model[i*(stride/4)+k]=expected[kind];
        fn(out,a,b,tv[kind],count,stride,stride);++r.calls;r.errors+=memcmp(out,model,sizeof(out))!=0;
    }
    return r;
}
#if defined(_WIN32)
static bool VAGuardCall(VAFn fn,void *out,void *a,void *b,float t,int n,unsigned sr,unsigned si) {
    __try {fn(out,a,b,t,n,sr,si);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
#endif
static VAResult VAFootprint(VAFn fn) {
    VAResult r;
#if defined(_WIN32)
    SYSTEM_INFO info;GetSystemInfo(&info);const size_t page=info.dwPageSize,active=3*page;unsigned char *blocks[3]={};
    for(int i=0;i<3;++i){blocks[i]=static_cast<unsigned char *>(VirtualAlloc(nullptr,active+2*page,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));if(!blocks[i]){++r.errors;for(int j=0;j<i;++j)VirtualFree(blocks[j],0,MEM_RELEASE);return r;}}
    const unsigned strides[][2]={{0,0},{12,12},{16,20},{20,16},{4,8},{8,4},{13,17},{1,3}};
    const int counts[]={1,2,3,4,5,7,8,9,17,64,65,257};const float steps[]={0,0.5f,1};
    for(const auto &stride:strides)for(int count:counts)for(int edge=0;edge<2;++edge)for(float t:steps) {
        DWORD old;unsigned char *p[3];unsigned char model[3*4096];if(active>sizeof(model)){++r.errors;break;}
        for(int k=0;k<3;++k){VirtualProtect(blocks[k],active+2*page,PAGE_READWRITE,&old);memset(blocks[k]+page,k==0?0xcd:k==1?0x3f:0x40,active);const size_t bytes=size_t(count-1)*stride[k==0?0:1]+12;p[k]=blocks[k]+(edge?page+active-bytes:page);}
        memcpy(model,blocks[0]+page,active);VAModel(model+(p[0]-blocks[0]-page),p[1],p[2],t,count,stride[0],stride[1]);
        for(int k=0;k<3;++k){VirtualProtect(blocks[k],page,PAGE_NOACCESS,&old);VirtualProtect(blocks[k]+page+active,page,PAGE_NOACCESS,&old);if(k)VirtualProtect(blocks[k]+page,active,PAGE_READONLY,&old);}
        ++r.calls;const bool ok=VAGuardCall(fn,p[0],p[1],p[2],t,count,stride[0],stride[1]);r.faults+=!ok;r.errors+=!ok;
        if(ok)r.errors+=memcmp(model,blocks[0]+page,active)!=0;
    }
    for(int i=0;i<3;++i)VirtualFree(blocks[i],0,MEM_RELEASE);
#endif
    return r;
}
static VAResult VAEmpty(VAFn fn,bool publicEntry) {
    VAResult r;
#if defined(_WIN32)
    SYSTEM_INFO info;GetSystemInfo(&info);void *bad=VirtualAlloc(nullptr,info.dwPageSize,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS);if(!bad){++r.errors;return r;}
    const int counts[]={0,-1,INT_MIN};for(int n:counts){++r.calls;const bool ok=VAGuardCall(fn,bad,bad,bad,0.5f,n,12,12);r.faults+=!ok;r.errors+=!ok;}
    if(publicEntry)for(int slot=0;slot<3;++slot){void *p[]={bad,bad,bad};p[slot]=nullptr;++r.calls;const bool ok=VAGuardCall(fn,p[0],p[1],p[2],0.5f,1,12,12);r.faults+=!ok;r.errors+=!ok;}
    VirtualFree(bad,0,MEM_RELEASE);
#endif
    return r;
}
#ifndef VX_VECTOR_ARRAY_PROBE
#if defined(VX_SIMD_SSE)
static void VARaw(void *out,void *a,void *b,float t,int n,XDWORD sr,XDWORD si){VxSIMDInterpolateVectorArray(out,a,b,t,n,sr,si);}
#endif
class VectorArrayInterpolationReference:public testing::TestWithParam<int> {
protected:
    int previous=VX_SIMD_MODE_AUTO;
    void SetUp() override {previous=VxGetSIMDOverride();const int wanted=GetParam()==0?VX_SIMD_MODE_NONE:VX_SIMD_MODE_SSE2;ASSERT_TRUE(VxSetSIMDOverride(wanted));ASSERT_EQ(VxGetSIMDEffectiveBackend(),wanted);}
    void TearDown() override {EXPECT_TRUE(VxSetSIMDOverride(previous));}
    VAFn Function() const {
#if defined(VX_SIMD_SSE)
        if(GetParam()==2)return VARaw;
#endif
        return InterpolateVectorArray;
    }
};
TEST_P(VectorArrayInterpolationReference, PreserveStrideZeroOverlapsAndPadding){EXPECT_EQ(VABasic(Function()).errors,0);}
TEST_P(VectorArrayInterpolationReference, SnapshotEachVectorBeforeForwardFeedback){EXPECT_EQ(VAOverlap(Function()).errors,0);}
TEST_P(VectorArrayInterpolationReference, AcceptByteStridesAndUnalignedBuffers){EXPECT_EQ(VAByteStrides(Function()).errors,0);}
TEST_P(VectorArrayInterpolationReference, CopyEndpointBitsWithVectorSnapshot){EXPECT_EQ(VAEndpoints(Function()).errors,0);}
TEST_P(VectorArrayInterpolationReference, RetainFiniteRangeAndSubnormalRounding){EXPECT_EQ(VARange(Function()).errors,0);}
TEST_P(VectorArrayInterpolationReference, ReadOnlySourcesAndExactXyzWrites){const VAResult r=VAFootprint(Function());EXPECT_EQ(r.errors+r.faults,0);}
TEST_P(VectorArrayInterpolationReference, EmptyCountsAndExistingNullGuards){const VAResult r=VAEmpty(Function(),GetParam()!=2);EXPECT_EQ(r.errors+r.faults,0);}
#if defined(VX_SIMD_SSE)
INSTANTIATE_TEST_SUITE_P(ScalarSSE2AndRaw,VectorArrayInterpolationReference,testing::Values(0,1,2));
#else
INSTANTIATE_TEST_SUITE_P(ScalarOnly,VectorArrayInterpolationReference,testing::Values(0));
#endif
#endif
