#include <float.h>
#include <math.h>
#include <new>
#include <string.h>
#ifndef VX_VECTOR_LENGTH_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef float(*VLFunction)(const VxVector *);
struct VLResult { int calls=0, errors=0, faults=0; };
static unsigned VLBits(float value) { unsigned bits; memcpy(&bits,&value,4); return bits; }
static float VLFloat(unsigned bits) { float value; memcpy(&value,&bits,4); return value; }
static float VLMember(const VxVector *v) { return v->Magnitude(); }
static float VLFree(const VxVector *v) { return Magnitude(*v); }
static float VLSquareMember(const VxVector *v) { return v->SquareMagnitude(); }
static float VLSquareFree(const VxVector *v) { return SquareMagnitude(*v); }
#if defined(VX_SIMD_SSE)
static float VLPointer(const VxVector *v) { return VxSIMDLengthVector(v); }
static float VLRegister(const VxVector *v) { return VxSIMDLength3(VxSIMDLoadFloat3(&v->x)); }
static float VLSquarePointer(const VxVector *v) { return VxSIMDLengthSquaredVector(v); }
static float VLSquareRegister(const VxVector *v) { return VxSIMDLengthSquared3(VxSIMDLoadFloat3(&v->x)); }
#endif
static float VLCall(VLFunction function, const VxVector *v, bool &fault) {
#if defined(_WIN32)
    __try { return function(v); } __except(EXCEPTION_EXECUTE_HANDLER) { fault=true; return 0; }
#else
    (void)fault; return function(v);
#endif
}
static float VLOracle(const float p[3], bool squared) {
    for(int k=0;k<3;++k) if(p[k]!=p[k]) return VLFloat(0x7fc00000u);
    double scale=0;
    for(int k=0;k<3;++k) if(fabs(p[k])>scale) scale=fabs(p[k]);
    if(scale==HUGE_VAL || scale==0) return static_cast<float>(scale);
    double sum=0; for(int k=0;k<3;++k) { const double ratio=p[k]/scale; sum+=ratio*ratio; }
    return static_cast<float>(squared ? (scale*scale)*sum : scale*sqrt(sum));
}
static bool VLCorrect(const float p[3], float actual, bool squared) {
    const float expected=VLOracle(p,squared);
    if(expected!=expected) return actual!=actual;
    if(expected==HUGE_VALF || expected==0) return VLBits(actual)==VLBits(expected);
    if(!(actual>=0 && actual<=FLT_MAX)) return false;
    const unsigned a=VLBits(actual),b=VLBits(expected);
    return (a>b?a-b:b-a)<=1;
}
static void VLObserve(VLResult &r, VLFunction function, const float p[3], bool squared) {
    VxVector v; memcpy(&v,p,12); bool fault=false; const float actual=VLCall(function,&v,fault);
    ++r.calls; r.faults+=fault; r.errors+=fault || !VLCorrect(p,actual,squared) || memcmp(&v,p,12)!=0;
}
static void VLCase(int shape,float p[3]) {
    const float values[][3]={{3,4,0},{1,-2,3},{-1,0,-0.0f},{0,0,0},{FLT_MAX,0,0},{VLFloat(1),-VLFloat(2),VLFloat(3)},{0x1p100f,-0x1p99f,0x1p98f},{0x1p-75f,0x1p-75f,0x1p-75f}};
    memcpy(p,values[shape],12);
}
static VLResult VLAnalytic(VLFunction function,bool squared) {
    VLResult r; for(int shape=0;shape<8;++shape) {float p[3];VLCase(shape,p);VLObserve(r,function,p,squared);}
    const float overflow[]={FLT_MAX,-FLT_MAX,FLT_MAX};VLObserve(r,function,overflow,squared);return r;
}
static VLResult VLPowers(VLFunction function,bool squared) {
    VLResult r;for(int power=-149;power<=127;++power)for(int sign=0;sign<8;++sign){float p[3];for(int k=0;k<3;++k)p[k]=ldexpf(sign&(1<<k)?-1.0f:1.0f,power-k);VLObserve(r,function,p,squared);}return r;
}
static VLResult VLRandoms(VLFunction function,bool squared) {
    VLResult r;unsigned state=326;for(int i=0;i<4096;++i){float p[3];for(int k=0;k<3;++k){state=state*1664525u+1013904223u;unsigned bits=state;if((bits&0x7f800000u)==0x7f800000u)bits^=0x00800000u;p[k]=VLFloat(bits);}VLObserve(r,function,p,squared);}return r;
}
static VLResult VLBoundaries(VLFunction function,bool squared) {
    VLResult r;const float values[]={static_cast<float>(sqrt(double(FLT_MAX))),static_cast<float>(sqrt(double(FLT_MIN))),0x1p-75f,0x1p-74f,0x1p127f};
    for(float value:values)for(int neighbor=-2;neighbor<=2;++neighbor)for(int shape=0;shape<4;++shape){const float v=VLFloat(VLBits(value)+neighbor);float p[]={v,shape&1?v:0.0f,shape&2?-v:0.0f};VLObserve(r,function,p,squared);}return r;
}
static VLResult VLZero(VLFunction function,bool squared) {
    VLResult r;for(int mask=0;mask<8;++mask){float p[3];for(int k=0;k<3;++k)p[k]=VLFloat(mask&(1<<k)?0x80000000u:0);VLObserve(r,function,p,squared);}return r;
}
static VLResult VLNonfinite(VLFunction function,bool squared) {
    VLResult r;const float values[]={VLFloat(0x7fc12345u),HUGE_VALF,-HUGE_VALF};for(int k=0;k<3;++k)for(float value:values)for(int mixed=0;mixed<2;++mixed){float p[]={-2,3,-0.0f};p[k]=value;if(mixed)p[(k+1)%3]=HUGE_VALF;VLObserve(r,function,p,squared);}return r;
}
static VLResult VLDerived(VLFunction function,bool squared) {
    VLResult r;const unsigned w[]={0,0x80000000u,0x7fc12345u,0x7f800000u};
    for(unsigned bits:w)for(int shape=0;shape<8;++shape){float p[3];VLCase(shape,p);VxVector4 v;memcpy(&v,p,12);memcpy(reinterpret_cast<unsigned char *>(&v)+12,&bits,4);unsigned char before[16];memcpy(before,&v,16);bool fault=false;const float actual=VLCall(function,&v,fault);++r.calls;r.faults+=fault;r.errors+=fault||!VLCorrect(p,actual,squared)||memcmp(before,&v,16)!=0;}return r;
}
static VLResult VLAligned(VLFunction function,bool squared) {
    VLResult r;for(int offset=0;offset<16;offset+=4)for(int shape=0;shape<8;++shape){alignas(16) unsigned char storage[64],before[64];memset(storage,0x6d,64);VxVector *v=::new(storage+16+offset)VxVector;float p[3];VLCase(shape,p);memcpy(v,p,12);memcpy(before,storage,64);bool fault=false;const float actual=VLCall(function,v,fault);++r.calls;r.faults+=fault;r.errors+=fault||!VLCorrect(p,actual,squared)||memcmp(before,storage,64)!=0;}return r;
}
static VLResult VLReadOnly(VLFunction function,bool squared) {
    VLResult r;
#if defined(_WIN32)
    SYSTEM_INFO info;GetSystemInfo(&info);const size_t page=info.dwPageSize;unsigned char *memory=static_cast<unsigned char *>(VirtualAlloc(nullptr,3*page,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS));if(!memory){++r.errors;return r;}
    for(int position=0;position<5;++position)for(int shape=0;shape<8;++shape){DWORD old;if(!VirtualProtect(memory+page,page,PAGE_READWRITE,&old)){++r.errors;break;}memset(memory+page,0x6d,page);const size_t offset=position==4?page-12:position*4;VxVector *v=::new(memory+page+offset)VxVector;float p[3];VLCase(shape,p);memcpy(v,p,12);if(!VirtualProtect(memory+page,page,PAGE_READONLY,&old)){++r.errors;break;}bool fault=false;const float actual=VLCall(function,v,fault);++r.calls;r.faults+=fault;bool valid=!fault&&VLCorrect(p,actual,squared)&&memcmp(p,v,12)==0;for(size_t k=0;k<page;++k)if(k<offset||k>=offset+12)valid=valid&&memory[page+k]==0x6d;r.errors+=!valid;}
    VirtualFree(memory,0,MEM_RELEASE);
#endif
    return r;
}
#ifndef VX_VECTOR_LENGTH_PROBE
class VectorLengthReference : public testing::TestWithParam<int> {
protected:
    bool Squared(){return GetParam()>=4;}
    VLFunction Function(){
        switch(GetParam()){
            case 0:return VLMember;case 1:return VLFree;case 4:return VLSquareMember;case 5:return VLSquareFree;
#if defined(VX_SIMD_SSE)
            case 2:return VLPointer;case 3:return VLRegister;case 6:return VLSquarePointer;case 7:return VLSquareRegister;
#endif
            default:return nullptr;
        }
    }
    void Check(VLResult r,int count){EXPECT_EQ(r.calls,count);EXPECT_EQ(r.errors,0);EXPECT_EQ(r.faults,0);}
};
TEST_P(VectorLengthReference,AnalyticValuesAndRepresentableRange){Check(VLAnalytic(Function(),Squared()),9);}
TEST_P(VectorLengthReference,EveryFiniteBinaryExponent){Check(VLPowers(Function(),Squared()),2216);}
TEST_P(VectorLengthReference,MixedFiniteBitPatterns){Check(VLRandoms(Function(),Squared()),4096);}
TEST_P(VectorLengthReference,RoundingAndOutputRangeBoundaries){Check(VLBoundaries(Function(),Squared()),100);}
TEST_P(VectorLengthReference,AllSignedZerosProducePositiveZero){Check(VLZero(Function(),Squared()),8);}
TEST_P(VectorLengthReference,NaNDominatesInfinityAndInfinityIsPositive){Check(VLNonfinite(Function(),Squared()),18);}
TEST_P(VectorLengthReference,BaseViewIgnoresWAndDoesNotWrite){Check(VLDerived(Function(),Squared()),32);}
TEST_P(VectorLengthReference,FourByteAlignedInputIsUnchanged){Check(VLAligned(Function(),Squared()),32);}
#if defined(_WIN32)
TEST_P(VectorLengthReference,ReadOnlyExactXYZTwelveByteFootprint){Check(VLReadOnly(Function(),Squared()),40);}
#endif
#if defined(VX_SIMD_SSE)
INSTANTIATE_TEST_SUITE_P(MemberFreePointerRegister,VectorLengthReference,testing::Values(0,1,2,3,4,5,6,7));
TEST(VectorLengthRegister,IgnoreWForMagnitude){const float fourth[]={0,-0.0f,2,FLT_MAX,VLFloat(0x7fc12345u),HUGE_VALF,-HUGE_VALF};for(float w:fourth)for(int shape=0;shape<8;++shape){float p[4];VLCase(shape,p);p[3]=w;EXPECT_TRUE(VLCorrect(p,VxSIMDLength3(_mm_loadu_ps(p)),false));}}
TEST(VectorLengthRegister,IgnoreWForSquareMagnitude){const float fourth[]={0,-0.0f,2,FLT_MAX,VLFloat(0x7fc12345u),HUGE_VALF,-HUGE_VALF};for(float w:fourth)for(int shape=0;shape<8;++shape){float p[4];VLCase(shape,p);p[3]=w;EXPECT_TRUE(VLCorrect(p,VxSIMDLengthSquared3(_mm_loadu_ps(p)),true));}}
#else
INSTANTIATE_TEST_SUITE_P(MemberFree,VectorLengthReference,testing::Values(0,1,4,5));
#endif
#endif
