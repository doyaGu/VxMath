#include <string.h>
#include <limits.h>
#ifndef VX_COPY_STRUCTURE_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
typedef decltype(&VxCopyStructure) CSFn;
struct CSResult{int calls=0,errors=0,faults=0;};
static void CSModel(int n,unsigned char *dst,unsigned sr,unsigned size,unsigned char *src,unsigned si){
    for(int i=0;i<n;++i){
        if(size==8||size==16){for(unsigned offset=0;offset<size;offset+=8){unsigned char pair[8];memcpy(pair,src+offset,8);memcpy(dst+offset,pair,8);}}
        else if(size==12){unsigned char xy[8],z[4];memcpy(xy,src,8);memcpy(dst,xy,4);memcpy(z,src+8,4);memcpy(dst+4,xy+4,4);memcpy(dst+8,z,4);}
        else for(unsigned offset=0;offset<size;offset+=4){unsigned char word[4];memcpy(word,src+offset,4);memcpy(dst+offset,word,4);}
        if(i+1<n){src+=si;dst+=sr;}
    }
}
static int CSCall(CSFn fn,int n,void *dst,unsigned sr,unsigned size,void *src,unsigned si,bool *fault){
#if defined(_WIN32)
    __try{return fn(n,dst,sr,size,src,si);}__except(EXCEPTION_EXECUTE_HANDLER){*fault=true;return -99;}
#else
    (void)fault;return fn(n,dst,sr,size,src,si);
#endif
}
static CSResult CSOrdinary(CSFn fn){
    CSResult r;const unsigned sizes[]={4,8,12,16,20,24,28,32,36,48,60,64,68,128},strides[]={1,3,4,7,8,12,16,20,32,129};const int counts[]={1,2,3,4,7,9};
    for(unsigned size:sizes)for(unsigned sr:strides)for(unsigned si:strides)for(int n:counts)for(int offset=0;offset<4;++offset){
        unsigned char src[2048],out[2048],expected[2048];for(unsigned i=0;i<sizeof(src);++i)src[i]=static_cast<unsigned char>(i*73+19);memset(out,0xcd,sizeof(out));memcpy(expected,out,sizeof(out));
        CSModel(n,expected+offset,sr,size,src+3-offset,si);bool fault=false;const int ok=CSCall(fn,n,out+offset,sr,size,src+3-offset,si,&fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=1||memcmp(out,expected,sizeof(out))!=0;
        for(unsigned i=0;i<sizeof(src);++i)r.errors+=src[i]!=static_cast<unsigned char>(i*73+19);
    }
    // Exercise the disjoint contiguous fast path for every supported test size,
    // including records larger than the stride matrix entries above.
    for(unsigned size:sizes)for(int n:counts)for(int offset=0;offset<4;++offset){
        unsigned char src[2048],out[2048],expected[2048];for(unsigned i=0;i<sizeof(src);++i)src[i]=static_cast<unsigned char>(i*73+19);memset(out,0xcd,sizeof(out));memcpy(expected,out,sizeof(out));
        CSModel(n,expected+offset,size,size,src+3-offset,size);bool fault=false;const int ok=CSCall(fn,n,out+offset,size,size,src+3-offset,size,&fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=1||memcmp(out,expected,sizeof(out))!=0;
        for(unsigned i=0;i<sizeof(src);++i)r.errors+=src[i]!=static_cast<unsigned char>(i*73+19);
    }
    return r;
}
static CSResult CSAlias(CSFn fn,int group){
    CSResult r;const unsigned pairs[][2]={{1,3},{3,1},{4,4},{8,8},{12,12},{16,16},{20,20},{4,12},{12,4},{8,16},{16,8},{20,12},{12,20},{32,64}};
    const unsigned smallSizes[]={4,8},mixedSizes[]={12,16},genericSizes[]={20,24,28,32,36,48,64,68,128};const int counts[]={1,2,3,4,9};
    for(unsigned index=0;index<(group==2?9u:2u);++index)for(const auto &stride:pairs)for(int delta=-24;delta<=24;++delta)for(int n:counts){
        const unsigned size=group==0?smallSizes[index]:group==1?mixedSizes[index]:genericSizes[index];unsigned char pool[2048],model[2048];for(unsigned i=0;i<sizeof(pool);++i)pool[i]=static_cast<unsigned char>(i*73+19);memcpy(model,pool,sizeof(pool));
        CSModel(n,model+128+delta,stride[0],size,model+128,stride[1]);bool fault=false;const int ok=CSCall(fn,n,pool+128+delta,stride[0],size,pool+128,stride[1],&fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=1||memcmp(pool,model,sizeof(pool))!=0;
    }
    return r;
}
static CSResult CSLiteral(CSFn fn){
    CSResult r;const unsigned cases[][8]={{8,4,1,2,0,0,0,0},{12,8,1,2,1,0,0,0},{12,4,1,2,3,0,0,0},{16,8,1,2,1,2,0,0},{16,4,1,2,2,4,0,0},{20,4,1,1,1,1,1,0}};
    for(const auto &c:cases){unsigned words[32];for(unsigned i=0;i<32;++i)words[i]=i+1;bool fault=false;const int ok=CSCall(fn,1,reinterpret_cast<unsigned char *>(words)+c[1],32,c[0],words,32,&fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=1;
        for(unsigned i=0;i<32;++i){const bool written=i>=c[1]/4&&i<(c[1]+c[0])/4;const unsigned expected=written?c[2+i-c[1]/4]:i+1;r.errors+=words[i]!=expected;}
    }
    return r;
}
static CSResult CSFootprint(CSFn fn){
    CSResult r;
#if defined(_WIN32)
    SYSTEM_INFO info;GetSystemInfo(&info);const size_t page=info.dwPageSize,active=3*page;unsigned char *blocks[2]={};
    for(int i=0;i<2;++i){blocks[i]=static_cast<unsigned char *>(VirtualAlloc(nullptr,active+2*page,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));if(!blocks[i]){++r.errors;for(int j=0;j<i;++j)VirtualFree(blocks[j],0,MEM_RELEASE);return r;}}
    const unsigned sizes[]={4,8,12,16,20,24,28,32,36,48,60,64,68,128},pairs[][2]={{0,0},{4,4},{8,12},{12,8},{16,20},{20,16},{1,3},{3,1},{7,12},{12,7},{32,129}};const int counts[]={1,2,3,4,7,9,17,65};
    for(unsigned size:sizes)for(const auto &stride:pairs)for(int n:counts)for(int edge=0;edge<2;++edge){
        unsigned char expected[3*4096];if(active>sizeof(expected)){++r.errors;break;}DWORD old;for(int i=0;i<2;++i){VirtualProtect(blocks[i],active+2*page,PAGE_READWRITE,&old);memset(blocks[i]+page,0xcd,active);}
        const unsigned sr=stride[0]?stride[0]:size,si=stride[1]?stride[1]:size;
        const size_t outBytes=size_t(n-1)*sr+size,srcBytes=size_t(n-1)*si+size;unsigned char *dst=blocks[0]+page+(edge?active-outBytes:0),*src=blocks[1]+page+(edge?active-srcBytes:0);for(size_t i=0;i<srcBytes;++i)src[i]=static_cast<unsigned char>(i*73+19);
        memcpy(expected,blocks[0]+page,active);CSModel(n,expected+(dst-blocks[0]-page),sr,size,src,si);for(int i=0;i<2;++i){VirtualProtect(blocks[i],page,PAGE_NOACCESS,&old);VirtualProtect(blocks[i]+page+active,page,PAGE_NOACCESS,&old);}VirtualProtect(blocks[1]+page,active,PAGE_READONLY,&old);
        bool fault=false;const int ok=CSCall(fn,n,dst,sr,size,src,si,&fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=1;if(!fault)r.errors+=memcmp(expected,blocks[0]+page,active)!=0;
    }
    for(int i=0;i<2;++i)VirtualFree(blocks[i],0,MEM_RELEASE);
#endif
    return r;
}
static CSResult CSRejected(CSFn fn,bool negative){
    CSResult r;
#if defined(_WIN32)
    SYSTEM_INFO info;GetSystemInfo(&info);void *bad=VirtualAlloc(nullptr,info.dwPageSize,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS);if(!bad){++r.errors;return r;}
    if(negative){const int counts[]={-1,INT_MIN};const unsigned sizes[]={4,8,12,16,20};for(int n:counts)for(unsigned size:sizes){bool fault=false;const int ok=CSCall(fn,n,bad,4,size,bad,4,&fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=0;}}
    else{const unsigned sizes[]={0,1,2,3,5,6,7,0xffffffffu};for(unsigned size:sizes){bool fault=false;const int ok=CSCall(fn,1,bad,4,size,bad,4,&fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=0;}
        for(int kind=0;kind<5;++kind){bool fault=false;const int ok=CSCall(fn,kind==0?0:1,kind==1?nullptr:bad,kind==2?0:4,4,kind==3?nullptr:bad,kind==4?0:4,&fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=0;}
    }
    VirtualFree(bad,0,MEM_RELEASE);
#endif
    return r;
}
#ifndef VX_COPY_STRUCTURE_PROBE
#if defined(VX_SIMD_SSE)
static XBOOL CSRaw(int n,void *dst,XDWORD sr,XDWORD size,const void *src,XDWORD si){return VxSIMDCopyStructure(n,dst,sr,size,src,si);}
#endif
class CopyStructureReference:public testing::TestWithParam<int>{
protected:
    int previous=VX_SIMD_MODE_AUTO;
    void SetUp()override{previous=VxGetSIMDOverride();const int wanted=GetParam()==0?VX_SIMD_MODE_NONE:VX_SIMD_MODE_SSE2;ASSERT_TRUE(VxSetSIMDOverride(wanted));ASSERT_EQ(VxGetSIMDEffectiveBackend(),wanted);}
    void TearDown()override{EXPECT_TRUE(VxSetSIMDOverride(previous));}
    CSFn Function()const{
#if defined(VX_SIMD_SSE)
        if(GetParam()==2)return CSRaw;
#endif
        return VxCopyStructure;
    }
};
TEST_P(CopyStructureReference, CopyByteStridesIncludingInputSmallerThanRecord){const CSResult r=CSOrdinary(Function());EXPECT_EQ(r.errors+r.faults,0);}
TEST_P(CopyStructureReference, PreserveFirstPairSnapshotsAndForwardRecordFeedback){const CSResult r=CSAlias(Function(),0);EXPECT_EQ(r.errors+r.faults,0);}
TEST_P(CopyStructureReference, PreserveTwelveByteMixedAndSixteenBytePairOrder){const CSResult r=CSAlias(Function(),1);EXPECT_EQ(r.errors+r.faults,0);}
TEST_P(CopyStructureReference, PreserveGenericDwordFeedbackAndMixedStrides){const CSResult r=CSAlias(Function(),2);EXPECT_EQ(r.errors+r.faults,0);}
TEST_P(CopyStructureReference, DistinguishOriginalReadWriteOrdersByLiteralWords){const CSResult r=CSLiteral(Function());EXPECT_EQ(r.errors+r.faults,0);}
TEST_P(CopyStructureReference, RespectReadOnlySourcesExactFootprintsAndPadding){const CSResult r=CSFootprint(Function());EXPECT_EQ(r.errors+r.faults,0);}
TEST_P(CopyStructureReference, RejectOriginalInvalidArgumentsWithoutAccess){const CSResult r=CSRejected(Function(),false);EXPECT_EQ(r.errors+r.faults,0);}
TEST_P(CopyStructureReference, RejectNegativeCountsWithoutAccess){const CSResult r=CSRejected(Function(),true);EXPECT_EQ(r.errors+r.faults,0);}
#if defined(VX_SIMD_SSE)
INSTANTIATE_TEST_SUITE_P(ScalarSSE2AndRaw,CopyStructureReference,testing::Values(0,1,2));
#else
INSTANTIATE_TEST_SUITE_P(ScalarOnly,CopyStructureReference,testing::Values(0));
#endif
#endif
