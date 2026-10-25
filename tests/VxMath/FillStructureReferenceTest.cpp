#include <string.h>
#include <limits.h>
#ifndef VX_FILL_STRUCTURE_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
typedef decltype(&VxFillStructure) FSFn;
struct FSResult {int calls=0,errors=0,faults=0;};
static void FSModel(int n,unsigned char *dst,unsigned stride,unsigned size,const unsigned char *src) {
    unsigned char snapshot[16];if(size<=16)memcpy(snapshot,src,size);
    for(int i=0;i<n;++i)for(unsigned offset=0;offset<size;offset+=4) {
        unsigned char word[4];memcpy(word,(size<=16?snapshot:src)+offset,4);memcpy(dst+i*stride+offset,word,4);
    }
}
static FSResult FSOrdinary(FSFn fn) {
    FSResult r;const unsigned sizes[]={4,8,12,16,20,24,28,32,36,48,60,64,68,128},strides[]={1,2,3,4,5,7,8,11,12,13,15,16,17,20,32,64,129,256};const int counts[]={1,2,3,4,7,8,9,17};
    for(unsigned size:sizes)for(unsigned stride:strides)for(int n:counts)for(int offset=0;offset<4;++offset) {
        unsigned char src[160],out[8192],expected[8192];for(unsigned i=0;i<sizeof(src);++i)src[i]=static_cast<unsigned char>(i*73+19);memset(out,0xcd,sizeof(out));memcpy(expected,out,sizeof(out));
        FSModel(n,expected+offset,stride,size,src+3-offset);const int ok=fn(n,out+offset,stride,size,src+3-offset);++r.calls;r.errors+=ok!=1||memcmp(out,expected,sizeof(out))!=0;
        for(unsigned i=0;i<sizeof(src);++i)r.errors+=src[i]!=static_cast<unsigned char>(i*73+19);
    }
    return r;
}
static FSResult FSAlias(FSFn fn,bool special) {
    FSResult r;const unsigned smallSizes[]={4,8,12,16},genericSizes[]={20,24,28,32,36,48,64,68,128};
    const unsigned strides[]={1,3,4,5,7,8,12,16,20,24,32,64,129};const int counts[]={1,2,3,5,9};
    for(unsigned index=0;index<(special?4u:9u);++index)for(unsigned stride:strides)for(int delta=-20;delta<=20;++delta)for(int n:counts) {
        const unsigned size=special?smallSizes[index]:genericSizes[index];unsigned char pool[2048],model[2048];for(unsigned i=0;i<sizeof(pool);++i)pool[i]=static_cast<unsigned char>(i*73+19);memcpy(model,pool,sizeof(pool));
        FSModel(n,model+128+delta,stride,size,model+128);const int ok=fn(n,pool+128+delta,stride,size,pool+128);++r.calls;r.errors+=ok!=1||memcmp(pool,model,sizeof(pool))!=0;
    }
    return r;
}
static FSResult FSLiteral(FSFn fn) {
    FSResult r;const unsigned sizes[]={16,20};
    for(unsigned size:sizes){unsigned words[32];for(unsigned i=0;i<32;++i)words[i]=i+1;const int ok=fn(1,words+1,4,size,words);++r.calls;r.errors+=ok!=1||words[0]!=1;
        for(unsigned i=1;i<=size/4;++i)r.errors+=words[i]!=(size==16?i:1);
        for(unsigned i=1+size/4;i<32;++i)r.errors+=words[i]!=i+1;
    }
    return r;
}
#if defined(_WIN32)
static int FSGuard(FSFn fn,int n,void *dst,unsigned stride,unsigned size,void *src,bool *fault) {
    __try{return fn(n,dst,stride,size,src);}__except(EXCEPTION_EXECUTE_HANDLER){*fault=true;return -99;}
}
#endif
static FSResult FSFootprint(FSFn fn) {
    FSResult r;
#if defined(_WIN32)
    SYSTEM_INFO info;GetSystemInfo(&info);const size_t page=info.dwPageSize,active=3*page;unsigned char *blocks[2]={};
    for(int i=0;i<2;++i){blocks[i]=static_cast<unsigned char *>(VirtualAlloc(nullptr,active+2*page,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));if(!blocks[i]){++r.errors;for(int j=0;j<i;++j)VirtualFree(blocks[j],0,MEM_RELEASE);return r;}}
    const unsigned sizes[]={4,8,12,16,20,24,28,32,36,48,60,64,68,128},strides[]={1,3,4,7,12,16,20,32,129};const int counts[]={1,2,3,4,7,8,9,17,65};
    for(unsigned size:sizes)for(unsigned stride:strides)for(int n:counts)for(int edge=0;edge<2;++edge) {
        unsigned char expected[3*4096];if(active>sizeof(expected)){++r.errors;break;}DWORD old;for(int i=0;i<2;++i)VirtualProtect(blocks[i],active+2*page,PAGE_READWRITE,&old);
        const size_t bytes=size_t(n-1)*stride+size;unsigned char *out=blocks[0]+page+(edge?active-bytes:0),*src=blocks[1]+page+(edge?active-size:0);memset(blocks[0]+page,0xcd,active);for(unsigned k=0;k<size;++k)src[k]=static_cast<unsigned char>(k*73+19);
        memcpy(expected,blocks[0]+page,active);FSModel(n,expected+(out-blocks[0]-page),stride,size,src);
        for(int i=0;i<2;++i){VirtualProtect(blocks[i],page,PAGE_NOACCESS,&old);VirtualProtect(blocks[i]+page+active,page,PAGE_NOACCESS,&old);}VirtualProtect(blocks[1]+page,active,PAGE_READONLY,&old);
        bool fault=false;const int ok=FSGuard(fn,n,out,stride,size,src,&fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=1;if(!fault)r.errors+=memcmp(expected,blocks[0]+page,active)!=0;
    }
    for(int i=0;i<2;++i)VirtualFree(blocks[i],0,MEM_RELEASE);
#endif
    return r;
}
static FSResult FSRejected(FSFn fn,bool negative) {
    FSResult r;
#if defined(_WIN32)
    SYSTEM_INFO info;GetSystemInfo(&info);void *bad=VirtualAlloc(nullptr,info.dwPageSize,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS);if(!bad){++r.errors;return r;}
    if(negative){const int counts[]={-1,INT_MIN};const unsigned sizes[]={4,16,20};for(int n:counts)for(unsigned size:sizes){bool fault=false;const int ok=FSGuard(fn,n,bad,4,size,bad,&fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=0;}}
    else {
        const unsigned sizes[]={0,1,2,3,5,6,7,0xffffffffu};for(unsigned size:sizes){bool fault=false;const int ok=FSGuard(fn,1,bad,4,size,bad,&fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=0;}
        for(int kind=0;kind<4;++kind){bool fault=false;const int ok=FSGuard(fn,kind==0?0:1,kind==1?nullptr:bad,kind==2?0:4,4,kind==3?nullptr:bad,&fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=0;}
    }
    VirtualFree(bad,0,MEM_RELEASE);
#endif
    return r;
}
#ifndef VX_FILL_STRUCTURE_PROBE
#if defined(VX_SIMD_SSE)
static XBOOL FSRaw(int n,void *dst,XDWORD stride,XDWORD size,const void *src){return VxSIMDFillStructure(n,dst,stride,size,src);}
#endif
class FillStructureReference:public testing::TestWithParam<int> {
protected:
    int previous=VX_SIMD_MODE_AUTO;
    void SetUp() override{previous=VxGetSIMDOverride();const int wanted=GetParam()==0?VX_SIMD_MODE_NONE:VX_SIMD_MODE_SSE2;ASSERT_TRUE(VxSetSIMDOverride(wanted));ASSERT_EQ(VxGetSIMDEffectiveBackend(),wanted);}
    void TearDown() override{EXPECT_TRUE(VxSetSIMDOverride(previous));}
    FSFn Function()const{
#if defined(VX_SIMD_SSE)
        if(GetParam()==2)return FSRaw;
#endif
        return VxFillStructure;
    }
};
TEST_P(FillStructureReference, FillUnalignedByteStridesAndPreservePadding){EXPECT_EQ(FSOrdinary(Function()).errors,0);}
TEST_P(FillStructureReference, CacheSmallPatternOnceAcrossAliasedDestinations){EXPECT_EQ(FSAlias(Function(),true).errors,0);}
TEST_P(FillStructureReference, PreserveGenericDwordFeedbackAndSourceRereads){EXPECT_EQ(FSAlias(Function(),false).errors,0);}
TEST_P(FillStructureReference, DistinguishSixteenByteSnapshotFromTwentyByteFeedback){EXPECT_EQ(FSLiteral(Function()).errors,0);}
TEST_P(FillStructureReference, RespectExactSourceAndDestinationBounds){const FSResult r=FSFootprint(Function());EXPECT_EQ(r.errors+r.faults,0);}
TEST_P(FillStructureReference, RejectOriginalInvalidArgumentsWithoutAccess){const FSResult r=FSRejected(Function(),false);EXPECT_EQ(r.errors+r.faults,0);}
TEST_P(FillStructureReference, RejectNegativeCountsWithoutAccess){const FSResult r=FSRejected(Function(),true);EXPECT_EQ(r.errors+r.faults,0);}
#if defined(VX_SIMD_SSE)
INSTANTIATE_TEST_SUITE_P(ScalarSSE2AndRaw,FillStructureReference,testing::Values(0,1,2));
#else
INSTANTIATE_TEST_SUITE_P(ScalarOnly,FillStructureReference,testing::Values(0));
#endif
#endif
