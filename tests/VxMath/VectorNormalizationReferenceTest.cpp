#include <float.h>
#include <math.h>
#include <new>
#include <string.h>
#ifndef VX_VECTOR_NORMALIZE_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef void(*VNFn)(VxVector *);
struct VNResult { int calls=0,errors=0,faults=0; };
static unsigned VNBits(float v){unsigned bits;memcpy(&bits,&v,4);return bits;}
static float VNFloat(unsigned bits){float v;memcpy(&v,&bits,4);return v;}
static void VNCall(VNFn fn,VxVector &v,bool &fault){
#if defined(_WIN32)
    __try{fn(&v);}__except(EXCEPTION_EXECUTE_HANDLER){fault=true;}
#else
    (void)fault;fn(&v);
#endif
}
static void VNOracle(const float input[3],float out[3]) {
    double scale=0;for(int k=0;k<3;++k)if(fabs(input[k])>scale)scale=fabs(input[k]);
    if(scale==0){memcpy(out,input,12);return;}
    double square=0;for(int k=0;k<3;++k){const double ratio=input[k]/scale;square+=ratio*ratio;}
    const double magnitude=sqrt(square);for(int k=0;k<3;++k)out[k]=float((input[k]/scale)/magnitude);
}
static bool VNCorrect(const float input[3],const VxVector &v) {
    float out[3],actual[3];VNOracle(input,out);memcpy(actual,&v,12);bool zero=true;double square=0;
    for(int k=0;k<3;++k){zero=zero&&input[k]==0;square+=double(actual[k])*actual[k];}
    if(zero)return memcmp(input,actual,12)==0;
    if(!(fabs(square-1)<=5e-7))return false;
    for(int k=0;k<3;++k){
        if(!(fabs(actual[k])<=FLT_MAX))return false;
        if(input[k]==0){if(VNBits(actual[k])!=VNBits(input[k]))return false;continue;}
        const unsigned a=VNBits(actual[k]),b=VNBits(out[k]);
        if((a^b)&0x80000000u)return false;
        if((a>b?a-b:b-a)>2)return false;
    }
    return true;
}
static void VNObserve(VNResult &r,VNFn fn,const float input[3]) {
    VxVector v;memcpy(&v,input,12);bool fault=false;VNCall(fn,v,fault);++r.calls;r.faults+=fault;r.errors+=fault||!VNCorrect(input,v);
}
static void VNCase(int shape,float p[3]) {
    const float values[][3]={{3,4,0},{1,-2,3},{-1,0,-0.0f},{0,1,0},{FLT_MAX,-FLT_MAX,FLT_MAX},{VNFloat(1),-VNFloat(2),VNFloat(3)},{0x1p-30f,-0x1p-31f,0x1p-32f},{0x1p60f,-0x1p59f,0x1p58f}};
    memcpy(p,values[shape],12);
}
static VNResult VNAnalytic(VNFn fn) {VNResult r;for(int i=0;i<4;++i){float p[3];VNCase(i,p);VNObserve(r,fn,p);}const float regression[]={3.5f,0.625f,-1.0f};VNObserve(r,fn,regression);return r;}
static VNResult VNPowers(VNFn fn) {
    VNResult r;for(int power=-149;power<=127;++power)for(int sign=0;sign<8;++sign){float p[3];for(int k=0;k<3;++k)p[k]=ldexpf((sign&(1<<k))?-1.0f:1.0f,power-k);VNObserve(r,fn,p);}return r;
}
static unsigned VNRandom(unsigned &state){state=state*1664525u+1013904223u;return state;}
static VNResult VNRandoms(VNFn fn) {
    VNResult r;unsigned random=324;
    for(int i=0;i<2048;++i){float p[3];for(int k=0;k<3;++k){unsigned bits=VNRandom(random);if((bits&0x7f800000u)==0x7f800000u)bits^=0x00800000u;p[k]=VNFloat(bits);}VNObserve(r,fn,p);}return r;
}
static VNResult VNThresholds(VNFn fn) {
    VNResult r;const float edge=sqrtf(0x1p-23f);const unsigned bits=VNBits(edge);const float values[]={VNFloat(bits-1),edge,VNFloat(bits+1),0x1p-30f};
    for(int k=0;k<3;++k)for(int sign=0;sign<2;++sign)for(float value:values){float p[3]={};p[k]=sign?-value:value;VNObserve(r,fn,p);}return r;
}
static VNResult VNZero(VNFn fn) {
    VNResult r;for(int mask=0;mask<8;++mask){float p[3];for(int k=0;k<3;++k)p[k]=VNFloat((mask&(1<<k))?0x80000000u:0);VNObserve(r,fn,p);}return r;
}
static VNResult VNNonfinite(VNFn fn) {
    VNResult r;const float values[]={VNFloat(0x7fc12345u),HUGE_VALF,-HUGE_VALF};
    for(int field=0;field<3;++field)for(float value:values)for(int mixed=0;mixed<2;++mixed){float p[]={-2,3,-0.0f};p[field]=value;if(mixed)p[(field+1)%3]=HUGE_VALF;VxVector v;memcpy(&v,p,12);bool fault=false;VNCall(fn,v,fault);++r.calls;r.faults+=fault;float out[3];memcpy(out,&v,12);bool valid=!fault;if(value!=value)valid=valid&&memcmp(out,p,12)==0;else for(int k=0;k<3;++k){if(fabs(p[k])==HUGE_VALF)valid=valid&&out[k]!=out[k];else valid=valid&&VNBits(out[k])==(VNBits(p[k])&0x80000000u);}r.errors+=!valid;}return r;
}
static VNResult VNDerived(VNFn fn) {
    VNResult r;const unsigned w[]={0,0x80000000u,0x7fcabcdeu,0x7f800000u};
    for(unsigned bits:w)for(int i=0;i<8;++i){float p[3];VNCase(i,p);VxVector4 v;memcpy(&v,p,12);memcpy(reinterpret_cast<unsigned char *>(&v)+12,&bits,4);bool fault=false;VNCall(fn,static_cast<VxVector &>(v),fault);++r.calls;r.faults+=fault;unsigned after;memcpy(&after,reinterpret_cast<unsigned char *>(&v)+12,4);r.errors+=fault||after!=bits||!VNCorrect(p,v);}return r;
}
static VNResult VNSurroundings(VNFn fn) {
    VNResult r;for(int offset=0;offset<16;offset+=4)for(int i=0;i<8;++i){alignas(16) unsigned char data[64],saved[64];memset(data,0x6d,sizeof(data));VxVector *v=::new(data+16+offset)VxVector;float p[3];VNCase(i,p);memcpy(v,p,12);memcpy(saved,data,sizeof(data));bool fault=false;VNCall(fn,*v,fault);++r.calls;r.faults+=fault;bool valid=!fault&&VNCorrect(p,*v);for(int k=0;k<64;++k)if(k<16+offset||k>=28+offset)valid=valid&&data[k]==saved[k];r.errors+=!valid;}return r;
}
static VNResult VNFootprint(VNFn fn) {
    VNResult r;
#if defined(_WIN32)
    SYSTEM_INFO info;GetSystemInfo(&info);const size_t page=info.dwPageSize;unsigned char *memory=static_cast<unsigned char *>(VirtualAlloc(nullptr,3*page,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS));if(!memory){++r.errors;return r;}DWORD old;if(!VirtualProtect(memory+page,page,PAGE_READWRITE,&old)){++r.errors;VirtualFree(memory,0,MEM_RELEASE);return r;}
    for(int position=0;position<5;++position)for(int i=0;i<8;++i){const size_t offset=position==4?page-12:position*4;memset(memory+page,0x6d,page);VxVector *v=::new(memory+page+offset)VxVector;float p[3];VNCase(i,p);memcpy(v,p,12);bool fault=false;VNCall(fn,*v,fault);++r.calls;r.faults+=fault;bool valid=!fault&&VNCorrect(p,*v);for(size_t k=0;k<page;++k)if(k<offset||k>=offset+12)valid=valid&&memory[page+k]==0x6d;r.errors+=!valid;}VirtualFree(memory,0,MEM_RELEASE);
#endif
    return r;
}
#ifndef VX_VECTOR_NORMALIZE_PROBE
static void VNMember(VxVector *v){v->Normalize();}
#if defined(VX_SIMD_SSE)
static void VNRaw(VxVector *v){VxSIMDNormalizeVector(v);}
#endif
class VectorNormalizationReference:public testing::TestWithParam<int>{
protected:
    VNFn Function(){
#if defined(VX_SIMD_SSE)
        if(GetParam())return VNRaw;
#endif
        return VNMember;
    }
    void Check(VNResult r,int count){EXPECT_EQ(r.calls,count);EXPECT_EQ(r.errors,0);EXPECT_EQ(r.faults,0);}
};
TEST_P(VectorNormalizationReference,NormalizeAnalyticDirections){Check(VNAnalytic(Function()),5);}
TEST_P(VectorNormalizationReference,NormalizeEveryFiniteBinaryExponent){Check(VNPowers(Function()),2216);}
TEST_P(VectorNormalizationReference,NormalizeMixedMagnitudeFiniteInputs){Check(VNRandoms(Function()),2048);}
TEST_P(VectorNormalizationReference,DoNotSuppressNonzeroVectorsAtFormerEpsilon){Check(VNThresholds(Function()),24);}
TEST_P(VectorNormalizationReference,PreserveSignedZerosForZeroVector){Check(VNZero(Function()),8);}
TEST_P(VectorNormalizationReference,PreserveUnorderedInputAndUseIEEEDivisionForInfinity){Check(VNNonfinite(Function()),18);}
TEST_P(VectorNormalizationReference,NormalizeBaseViewWithoutReadingOrChangingW){Check(VNDerived(Function()),32);}
TEST_P(VectorNormalizationReference,OnlyWriteXYZInFourByteAlignedStorage){Check(VNSurroundings(Function()),32);}
#if defined(_WIN32)
TEST_P(VectorNormalizationReference,RespectExactTwelveByteFootprint){Check(VNFootprint(Function()),40);}
#endif
#if defined(VX_SIMD_SSE)
INSTANTIATE_TEST_SUITE_P(MemberAndRaw,VectorNormalizationReference,testing::Values(0,1));
#else
INSTANTIATE_TEST_SUITE_P(ScalarMember,VectorNormalizationReference,testing::Values(0));
#endif
#endif
