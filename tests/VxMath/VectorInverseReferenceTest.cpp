#include <float.h>
#include <math.h>
#include <new>
#include <string.h>
#ifndef VX_VECTOR_INVERSE_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef float(*VIFunction)(const VxVector *);
struct VIResult { int calls=0, errors=0, faults=0; };
static unsigned VIBits(float value) { unsigned bits; memcpy(&bits,&value,4); return bits; }
static float VIFloat(unsigned bits) { float value; memcpy(&value,&bits,4); return value; }
static float VIInvMagnitude(const VxVector *v) { return InvMagnitude(*v); }
static float VIInvSquareMagnitude(const VxVector *v) { return InvSquareMagnitude(*v); }
static float VICall(VIFunction function, const VxVector *v, bool &fault) {
#if defined(_WIN32)
    __try { return function(v); } __except(EXCEPTION_EXECUTE_HANDLER) { fault=true; return 0; }
#else
    (void)fault; return function(v);
#endif
}
static float VIOracle(const float p[3], bool squared) {
    for(int k=0;k<3;++k) if(p[k]!=p[k]) return VIFloat(0x7fc00000u);
    double scale=0; for(int k=0;k<3;++k) if(fabs(p[k])>scale) scale=fabs(p[k]);
    if(scale==0) return HUGE_VALF;
    if(scale==HUGE_VAL) return 0;
    double sum=0;for(int k=0;k<3;++k){const double ratio=p[k]/scale;sum+=ratio*ratio;}
    const double inverse=1.0/scale;
    return static_cast<float>(squared ? (inverse/scale)/sum : inverse/sqrt(sum));
}
static bool VICorrect(const float p[3], float actual, bool squared) {
    const float expected=VIOracle(p,squared);
    if(expected!=expected) return actual!=actual;
    if(expected==HUGE_VALF || expected==0) return VIBits(actual)==VIBits(expected);
    if(!(actual>=0 && actual<=FLT_MAX)) return false;
    const unsigned a=VIBits(actual),b=VIBits(expected);
    return (a>b?a-b:b-a)<=1;
}
static void VIObserve(VIResult &r, VIFunction function, const float p[3], bool squared) {
    VxVector v; memcpy(&v,p,12); bool fault=false; const float actual=VICall(function,&v,fault);
    ++r.calls; r.faults+=fault; r.errors+=fault || !VICorrect(p,actual,squared) || memcmp(&v,p,12)!=0;
}
static void VICase(int shape,float p[3]) {
    const float values[][3]={{3,4,0},{1,-2,3},{-1,0,-0.0f},{0,0,0},{FLT_MAX,0,0},{VIFloat(1),-VIFloat(2),VIFloat(3)},{0x1p100f,-0x1p99f,0x1p98f},{0x1p-75f,0x1p-75f,0x1p-75f}};
    memcpy(p,values[shape],12);
}
static VIResult VIAnalytic(VIFunction function,bool squared) {
    VIResult r; for(int shape=0;shape<8;++shape) {float p[3];VICase(shape,p);VIObserve(r,function,p,squared);}
    const float overflow[]={FLT_MAX,-FLT_MAX,FLT_MAX};VIObserve(r,function,overflow,squared);return r;
}
static VIResult VIPowers(VIFunction function,bool squared) {
    VIResult r;for(int power=-149;power<=127;++power)for(int sign=0;sign<8;++sign){float p[3];for(int k=0;k<3;++k)p[k]=ldexpf(sign&(1<<k)?-1.0f:1.0f,power-k);VIObserve(r,function,p,squared);}return r;
}
static VIResult VIRandoms(VIFunction function,bool squared) {
    VIResult r;unsigned state=326;for(int i=0;i<4096;++i){float p[3];for(int k=0;k<3;++k){state=state*1664525u+1013904223u;unsigned bits=state;if((bits&0x7f800000u)==0x7f800000u)bits^=0x00800000u;p[k]=VIFloat(bits);}VIObserve(r,function,p,squared);}return r;
}
static VIResult VIBoundaries(VIFunction function,bool squared) {
    VIResult r;const float values[]={static_cast<float>(sqrt(1.0/double(FLT_MAX))),static_cast<float>(sqrt(1.0/double(FLT_MIN))),0x1p70f,0x1p-60f,static_cast<float>(1.0/double(FLT_MAX))};
    for(float value:values)for(int neighbor=-2;neighbor<=2;++neighbor)for(int shape=0;shape<4;++shape){const float v=VIFloat(VIBits(value)+neighbor);float p[]={v,shape&1?v:0.0f,shape&2?-v:0.0f};VIObserve(r,function,p,squared);}return r;
}
static VIResult VIZero(VIFunction function,bool squared) {
    VIResult r;for(int mask=0;mask<8;++mask){float p[3];for(int k=0;k<3;++k)p[k]=VIFloat(mask&(1<<k)?0x80000000u:0);VIObserve(r,function,p,squared);}return r;
}
static VIResult VINonfinite(VIFunction function,bool squared) {
    VIResult r;const float values[]={VIFloat(0x7fc12345u),HUGE_VALF,-HUGE_VALF};for(int k=0;k<3;++k)for(float value:values)for(int mixed=0;mixed<2;++mixed){float p[]={-2,3,-0.0f};p[k]=value;if(mixed)p[(k+1)%3]=HUGE_VALF;VIObserve(r,function,p,squared);}return r;
}
static VIResult VIDerived(VIFunction function,bool squared) {
    VIResult r;const unsigned w[]={0,0x80000000u,0x7fc12345u,0x7f800000u};
    for(unsigned bits:w)for(int shape=0;shape<8;++shape){float p[3];VICase(shape,p);VxVector4 v;memcpy(&v,p,12);memcpy(reinterpret_cast<unsigned char *>(&v)+12,&bits,4);unsigned char before[16];memcpy(before,&v,16);bool fault=false;const float actual=VICall(function,&v,fault);++r.calls;r.faults+=fault;r.errors+=fault||!VICorrect(p,actual,squared)||memcmp(before,&v,16)!=0;}return r;
}
static VIResult VIAligned(VIFunction function,bool squared) {
    VIResult r;for(int offset=0;offset<16;offset+=4)for(int shape=0;shape<8;++shape){alignas(16) unsigned char storage[64],before[64];memset(storage,0x6d,64);VxVector *v=::new(storage+16+offset)VxVector;float p[3];VICase(shape,p);memcpy(v,p,12);memcpy(before,storage,64);bool fault=false;const float actual=VICall(function,v,fault);++r.calls;r.faults+=fault;r.errors+=fault||!VICorrect(p,actual,squared)||memcmp(before,storage,64)!=0;}return r;
}
static VIResult VIReadOnly(VIFunction function,bool squared) {
    VIResult r;
#if defined(_WIN32)
    SYSTEM_INFO info;GetSystemInfo(&info);const size_t page=info.dwPageSize;unsigned char *memory=static_cast<unsigned char *>(VirtualAlloc(nullptr,3*page,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS));if(!memory){++r.errors;return r;}
    for(int position=0;position<5;++position)for(int shape=0;shape<8;++shape){DWORD old;if(!VirtualProtect(memory+page,page,PAGE_READWRITE,&old)){++r.errors;break;}memset(memory+page,0x6d,page);const size_t offset=position==4?page-12:position*4;VxVector *v=::new(memory+page+offset)VxVector;float p[3];VICase(shape,p);memcpy(v,p,12);if(!VirtualProtect(memory+page,page,PAGE_READONLY,&old)){++r.errors;break;}bool fault=false;const float actual=VICall(function,v,fault);++r.calls;r.faults+=fault;bool valid=!fault&&VICorrect(p,actual,squared)&&memcmp(p,v,12)==0;for(size_t k=0;k<page;++k)if(k<offset||k>=offset+12)valid=valid&&memory[page+k]==0x6d;r.errors+=!valid;}
    VirtualFree(memory,0,MEM_RELEASE);
#endif
    return r;
}
#if defined(VX_SIMD_SSE)
typedef __m128(*VIUnaryFunction)(__m128);
struct VIUnaryResult { int vectors=0, lanes=0, errors=0; };
static __m128 VIUnarySqrt(__m128 v) { return VxSIMDReciprocalSqrtAccurate(v); }
static __m128 VIUnaryReciprocal(__m128 v) { return VxSIMDReciprocalAccurate(v); }
static float VIUnaryOracle(float value,bool root) { return static_cast<float>(root?1.0/sqrt(double(value)):1.0/double(value)); }
static bool VIUnaryCorrect(float input,float actual,bool root) {
    const float expected=VIUnaryOracle(input,root);
    if(expected!=expected)return actual!=actual;
    const unsigned a=VIBits(actual),b=VIBits(expected);
    if(fabs(expected)==HUGE_VALF||expected==0)return a==b;
    if((a^b)&0x80000000u)return false;
    return (a>b?a-b:b-a)<=1;
}
static void VIUnaryObserve(VIUnaryResult &r,VIUnaryFunction function,const float p[4],bool root) {
    unsigned char before[16];memcpy(before,p,16);float out[4];_mm_storeu_ps(out,function(_mm_loadu_ps(p)));++r.vectors;
    for(int k=0;k<4;++k){++r.lanes;r.errors+=!VIUnaryCorrect(p[k],out[k],root);}r.errors+=memcmp(p,before,16)!=0;
}
static VIUnaryResult VIUnarySpecial(VIUnaryFunction function,bool root) {
    VIUnaryResult r;const unsigned bits[]={0,0x80000000u,0x7f800000u,0xff800000u,0x7fc12345u,0xffc12345u,0x3f800000u,0xbf800000u,1,0x80000001u,0x00800000u,0x80800000u,0x7f7fffffu,0xff7fffffu,0x40800000u,0xc0800000u};
    for(int i=0;i<16;i+=4){float p[4];for(int k=0;k<4;++k)p[k]=VIFloat(bits[i+k]);VIUnaryObserve(r,function,p,root);}return r;
}
static VIUnaryResult VIUnaryPowers(VIUnaryFunction function,bool root) {
    VIUnaryResult r;for(int power=-149;power<=127;++power)for(int sign=0;sign<2;++sign){float p[4];for(int k=0;k<4;++k)p[k]=ldexpf(sign?-1.0f:1.0f,power-k);VIUnaryObserve(r,function,p,root);}return r;
}
static VIUnaryResult VIUnaryRandoms(VIUnaryFunction function,bool root) {
    VIUnaryResult r;unsigned state=327;for(int i=0;i<2048;++i){float p[4];for(int k=0;k<4;++k){state=state*1664525u+1013904223u;unsigned bits=state;if((bits&0x7f800000u)==0x7f800000u)bits^=0x00800000u;p[k]=VIFloat(bits);}VIUnaryObserve(r,function,p,root);}return r;
}
static VIUnaryResult VIUnaryBoundaries(VIUnaryFunction function,bool root) {
    VIUnaryResult r;const unsigned bits[]={0,1,2,3,0x007ffffeu,0x007fffffu,0x00800000u,0x00800001u,0x3f7fffffu,0x3f800000u,0x3f800001u,0x7f7ffffeu,0x7f7fffffu,0x7f800000u,0x7fc12345u,0x80000000u,0x80000001u,0xff800000u,0xffc12345u,0xbf800000u};
    for(unsigned value:bits)for(int lane=0;lane<4;++lane){float p[]={1,2,3,4};p[lane]=VIFloat(value);VIUnaryObserve(r,function,p,root);}return r;
}
#endif
#ifndef VX_VECTOR_INVERSE_PROBE
class VectorInverseReference : public testing::TestWithParam<int> {
protected:
    bool Squared(){return GetParam()!=0;}
    VIFunction Function(){return Squared()?VIInvSquareMagnitude:VIInvMagnitude;}
    void Check(VIResult r,int count){EXPECT_EQ(r.calls,count);EXPECT_EQ(r.errors,0);EXPECT_EQ(r.faults,0);}
};
TEST_P(VectorInverseReference,AnalyticAndRepresentableInverseRange){Check(VIAnalytic(Function(),Squared()),9);}
TEST_P(VectorInverseReference,EveryFiniteBinaryExponent){Check(VIPowers(Function(),Squared()),2216);}
TEST_P(VectorInverseReference,MixedFiniteBitPatterns){Check(VIRandoms(Function(),Squared()),4096);}
TEST_P(VectorInverseReference,RoundingAndInverseRangeBoundaries){Check(VIBoundaries(Function(),Squared()),100);}
TEST_P(VectorInverseReference,AllSignedZerosProducePositiveInfinity){Check(VIZero(Function(),Squared()),8);}
TEST_P(VectorInverseReference,NaNDominatesInfinityAndInfinityProducesPositiveZero){Check(VINonfinite(Function(),Squared()),18);}
TEST_P(VectorInverseReference,BaseViewIgnoresWAndDoesNotWrite){Check(VIDerived(Function(),Squared()),32);}
TEST_P(VectorInverseReference,FourByteAlignedInputIsUnchanged){Check(VIAligned(Function(),Squared()),32);}
#if defined(_WIN32)
TEST_P(VectorInverseReference,ReadOnlyExactXYZTwelveByteFootprint){Check(VIReadOnly(Function(),Squared()),40);}
#endif
INSTANTIATE_TEST_SUITE_P(InverseMagnitudeAndSquare,VectorInverseReference,testing::Values(0,1));
#if defined(VX_SIMD_SSE)
class VectorInverseUnaryReference : public testing::TestWithParam<int> {
protected:
    bool Root(){return GetParam()==0;}
    VIUnaryFunction Function(){return Root()?VIUnarySqrt:VIUnaryReciprocal;}
    void Check(VIUnaryResult r,int vectors){EXPECT_EQ(r.vectors,vectors);EXPECT_EQ(r.lanes,vectors*4);EXPECT_EQ(r.errors,0);}
};
TEST_P(VectorInverseUnaryReference,IEEEValuesSignsAndAllFourLanes){Check(VIUnarySpecial(Function(),Root()),4);}
TEST_P(VectorInverseUnaryReference,EveryBinaryExponentAndBothSigns){Check(VIUnaryPowers(Function(),Root()),554);}
TEST_P(VectorInverseUnaryReference,MixedFiniteRegisters){Check(VIUnaryRandoms(Function(),Root()),2048);}
TEST_P(VectorInverseUnaryReference,BoundaryValuesDoNotContaminateOtherLanes){Check(VIUnaryBoundaries(Function(),Root()),80);}
INSTANTIATE_TEST_SUITE_P(AccurateRootAndReciprocal,VectorInverseUnaryReference,testing::Values(0,1));
#endif
#endif
