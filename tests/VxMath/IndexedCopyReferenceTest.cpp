#include <string.h>
#include <limits.h>
#include <stddef.h>
#ifndef VX_INDEXED_COPY_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
typedef decltype(&VxIndexedCopy) ICFn;
struct ICResult{int calls=0,errors=0,faults=0;};
static void ICModel(VxStridedData dst,VxStridedData src,unsigned size,int *indices,int count){
    unsigned char *out=static_cast<unsigned char *>(dst.Ptr),*base=static_cast<unsigned char *>(src.Ptr),*cursor=reinterpret_cast<unsigned char *>(indices);
    for(int i=0;i<count;++i){int index;memcpy(&index,cursor,4);unsigned char *p=base+static_cast<ptrdiff_t>(index)*static_cast<ptrdiff_t>(src.Stride);
        const unsigned width=size<=24?8:4;unsigned offset=0;
        for(;offset+width<=size;offset+=width){unsigned char unit[8];memcpy(unit,p+offset,width);memcpy(out+offset,unit,width);}
        if(offset<size){unsigned char word[4];memcpy(word,p+offset,4);memcpy(out+offset,word,4);}
        if(i+1<count){out+=dst.Stride;cursor+=4;}
    }
}
static int ICCall(ICFn fn,const VxStridedData &dst,const VxStridedData &src,unsigned size,int *indices,int n,bool *fault){
#if defined(_WIN32)
    __try{return fn(dst,src,size,indices,n);}__except(EXCEPTION_EXECUTE_HANDLER){*fault=true;return -99;}
#else
    (void)fault;return fn(dst,src,size,indices,n);
#endif
}
static ICResult ICBasic(ICFn fn){
    ICResult r;const unsigned sizes[]={4,8,12,16,20,24,28,32,36,48,60,64,68,128,256},pairs[][2]={{0,0},{0,1},{1,0},{4,4},{8,12},{12,8},{16,24},{24,16},{32,64},{64,32},{256,129}};const int counts[]={1,2,3,4,7,9,17};
    for(unsigned size:sizes)for(const auto &stride:pairs)for(int pattern=0;pattern<5;++pattern)for(int n:counts)for(int offset=0;offset<4;++offset){
        unsigned char source[8192],out[8192],model[8192],indexBytes[100],saved[100];for(unsigned i=0;i<sizeof(source);++i)source[i]=static_cast<unsigned char>(i*73+19);memset(out,0xcd,sizeof(out));memcpy(model,out,sizeof(out));memset(indexBytes,0xab,sizeof(indexBytes));
        for(int i=0;i<n;++i){int index=pattern==0?i:pattern==1?n-i-1:pattern==2?3:pattern==3?(i*5+3)%17:i%9-4;memcpy(indexBytes+offset+4*i,&index,4);}memcpy(saved,indexBytes,sizeof(saved));
        VxStridedData src(source+3072+3-offset,stride[1]),dst(out+offset,stride[0]),expected(model+offset,stride[0]);int *indices=reinterpret_cast<int *>(indexBytes+offset);ICModel(expected,src,size,indices,n);bool fault=false;const int ok=ICCall(fn,dst,src,size,indices,n,&fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=1||memcmp(out,model,sizeof(out))!=0||memcmp(saved,indexBytes,sizeof(saved))!=0;
        for(unsigned i=0;i<sizeof(source);++i)r.errors+=source[i]!=static_cast<unsigned char>(i*73+19);
    }
    return r;
}
static ICResult ICAlias(ICFn fn,bool generic){
    ICResult r;const unsigned specialSizes[]={4,8,12,16,20,24},genericSizes[]={28,32,36,48,60,64,68,128,256};const unsigned pairs[][2]={{0,0},{0,4},{4,0},{4,4},{8,12},{12,8},{24,24},{32,16}};const int counts[]={1,3,9};
    for(unsigned k=0;k<(generic?9u:6u);++k)for(const auto &stride:pairs)for(int delta=-28;delta<=28;++delta)for(int n:counts)for(int pattern=0;pattern<2;++pattern){
        const unsigned size=generic?genericSizes[k]:specialSizes[k];unsigned char pool[8192],model[8192];int indices[16];for(unsigned i=0;i<sizeof(pool);++i)pool[i]=static_cast<unsigned char>(i*73+19);memcpy(model,pool,sizeof(pool));for(int i=0;i<n;++i)indices[i]=pattern?i%5-2:0;
        VxStridedData src(pool+2048,stride[1]),dst(pool+2048+delta,stride[0]),ms(model+2048,stride[1]),md(model+2048+delta,stride[0]);ICModel(md,ms,size,indices,n);bool fault=false;const int ok=ICCall(fn,dst,src,size,indices,n,&fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=1||memcmp(pool,model,sizeof(pool))!=0;
    }
    return r;
}
static ICResult ICIndexFeedback(ICFn fn){
    ICResult r;const unsigned sizes[]={4,8,12,16,20,24,28,32,36,48,60,64,68,128,256},strides[]={0,4,8,12};const int counts[]={1,2,3,4,7,9},offsets[]={-2,-1,0,1,2,4};
    for(unsigned size:sizes)for(unsigned stride:strides)for(int n:counts)for(int offset:offsets)for(int zero=0;zero<2;++zero){
        unsigned pool[512],model[512],source[1024];for(unsigned i=0;i<512;++i)pool[i]=model[i]=i%16;for(unsigned i=0;i<1024;++i)source[i]=(i*3+1)%16;
        VxStridedData src(source,zero?0:size),dst(pool+64+offset,stride),expected(model+64+offset,stride);ICModel(expected,src,size,reinterpret_cast<int *>(model+64),n);bool fault=false;const int ok=ICCall(fn,dst,src,size,reinterpret_cast<int *>(pool+64),n,&fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=1||memcmp(pool,model,sizeof(pool))!=0;
    }
    return r;
}
static ICResult ICDescriptors(ICFn fn){
    ICResult r;unsigned source[]={0,0x12345678u};int indices[]={0,1};
    {VxStridedData src(source,4),dst(&src.Stride,0);bool fault=false;const int ok=ICCall(fn,dst,src,4,indices,2,&fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=1||src.Stride!=0x12345678u;}
    {struct Holder{VxStridedData descriptor;unsigned tail[4];}holder;memset(&holder,0,sizeof(holder));holder.descriptor.Ptr=&holder.descriptor.Stride;holder.descriptor.Stride=4;unsigned char expected[sizeof(holder)];memcpy(expected,&holder,sizeof(holder));const ptrdiff_t at=reinterpret_cast<unsigned char *>(&holder.descriptor.Stride)-reinterpret_cast<unsigned char *>(&holder);memcpy(expected+at,source,4);memcpy(expected+at+4,source+1,4);VxStridedData src(source,4);bool fault=false;const int ok=ICCall(fn,holder.descriptor,src,4,indices,2,&fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=1||memcmp(expected,&holder,sizeof(holder))!=0;}
    return r;
}
static ICResult ICLiteral(ICFn fn){
    ICResult r;const unsigned cases[][10]={{8,4,1,2},{12,4,1,2,2},{12,8,1,2,1},{16,4,1,2,2,4},{16,8,1,2,1,2},{20,4,1,2,2,4,4},{24,4,1,2,2,4,4,6},{28,4,1,1,1,1,1,1,1}};int index=0;
    for(const auto &c:cases){unsigned words[32];for(unsigned i=0;i<32;++i)words[i]=i+1;VxStridedData src(words,32),dst(reinterpret_cast<unsigned char *>(words)+c[1],32);bool fault=false;const int ok=ICCall(fn,dst,src,c[0],&index,1,&fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=1;
        for(unsigned i=0;i<32;++i){const bool written=i>=c[1]/4&&i<(c[1]+c[0])/4;const unsigned expected=written?c[2+i-c[1]/4]:i+1;r.errors+=words[i]!=expected;}
    }
    unsigned word=0xdeadbeefu,out[4]={};int extremes[]={INT_MIN,INT_MAX,-1,0};VxStridedData src(&word,0),dst(out,4);bool fault=false;const int ok=ICCall(fn,dst,src,4,extremes,4,&fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=1;for(unsigned value:out)r.errors+=value!=word;
    return r;
}
static ICResult ICFootprint(ICFn fn){
    ICResult r;
#if defined(_WIN32)
    SYSTEM_INFO info;GetSystemInfo(&info);const size_t page=info.dwPageSize,active=5*page;unsigned char *blocks[3]={};
    for(int i=0;i<3;++i){blocks[i]=static_cast<unsigned char *>(VirtualAlloc(nullptr,active+2*page,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));if(!blocks[i]){++r.errors;for(int j=0;j<i;++j)VirtualFree(blocks[j],0,MEM_RELEASE);return r;}}
    const unsigned sizes[]={4,8,12,16,20,24,28,32,36,48,60,64,68,128,256},pairs[][2]={{0,0},{0,1},{1,0},{4,12},{12,4},{32,32},{256,129}};const int counts[]={1,2,3,4,7,8,9,17,65};
    for(unsigned size:sizes)for(const auto &stride:pairs)for(int n:counts)for(int edge=0;edge<2;++edge)for(int negative=0;negative<2;++negative){
        unsigned char expected[5*4096];if(active>sizeof(expected)){++r.errors;break;}DWORD old;for(int i=0;i<3;++i){VirtualProtect(blocks[i],active+2*page,PAGE_READWRITE,&old);memset(blocks[i]+page,0xcd,active);}
        const size_t outBytes=size_t(n-1)*stride[0]+size,srcBytes=size_t(n-1)*stride[1]+size;unsigned char *out=blocks[0]+page+(edge?active-outBytes:0),*low=blocks[1]+page+(edge?active-srcBytes:0);int *indices=reinterpret_cast<int *>(blocks[2]+page+(edge?active-n*4:0));for(int i=0;i<n;++i)indices[i]=negative?-i:i;for(size_t i=0;i<srcBytes;++i)low[i]=static_cast<unsigned char>(i*73+19);
        VxStridedData src(low+(negative?size_t(n-1)*stride[1]:0),stride[1]),dst(out,stride[0]),md(expected+(out-blocks[0]-page),stride[0]);memcpy(expected,blocks[0]+page,active);ICModel(md,src,size,indices,n);
        for(int i=0;i<3;++i){VirtualProtect(blocks[i],page,PAGE_NOACCESS,&old);VirtualProtect(blocks[i]+page+active,page,PAGE_NOACCESS,&old);if(i)VirtualProtect(blocks[i]+page,active,PAGE_READONLY,&old);}
        bool fault=false;const int ok=ICCall(fn,dst,src,size,indices,n,&fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=1;if(!fault)r.errors+=memcmp(expected,blocks[0]+page,active)!=0;
    }
    for(int i=0;i<3;++i)VirtualFree(blocks[i],0,MEM_RELEASE);
#endif
    return r;
}
static ICResult ICRejected(ICFn fn,int mode){
    ICResult r;
#if defined(_WIN32)
    SYSTEM_INFO info;GetSystemInfo(&info);void *bad=VirtualAlloc(nullptr,info.dwPageSize,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS);if(!bad){++r.errors;return r;}VxStridedData src(bad,4),dst(bad,4);int *indices=static_cast<int *>(bad);
    if(mode==2){const unsigned sizes[]={4,8,12,16,20,24,28};const int counts[]={-1,INT_MIN};for(unsigned size:sizes)for(int n:counts){bool fault=false;const int ok=ICCall(fn,dst,src,size,indices,n,&fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=0;}}
    else{const unsigned sizes[]={0,1,2,3,5,6,7,0xffffffffu};for(unsigned size:sizes){if(mode==0&&size==0)continue;bool fault=false;const int ok=ICCall(fn,dst,src,size,indices,1,&fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=0;}
        if(mode==1)for(int kind=0;kind<4;++kind){VxStridedData d=dst,s=src;if(kind==1)d.Ptr=nullptr;if(kind==2)s.Ptr=nullptr;bool fault=false;const int ok=ICCall(fn,d,s,4,kind==3?nullptr:indices,kind==0?0:1,&fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=0;}
    }
    VirtualFree(bad,0,MEM_RELEASE);
#endif
    return r;
}
#ifndef VX_INDEXED_COPY_PROBE
#if defined(VX_SIMD_SSE)
static XBOOL ICRaw(const VxStridedData &dst,const VxStridedData &src,XDWORD size,const int *indices,int n){return VxSIMDIndexedCopy(dst,src,size,indices,n);}
#endif
class IndexedCopyReference:public testing::TestWithParam<int>{
protected:
    int previous=VX_SIMD_MODE_AUTO;
    void SetUp()override{previous=VxGetSIMDOverride();const int wanted=GetParam()==0?VX_SIMD_MODE_NONE:VX_SIMD_MODE_SSE2;ASSERT_TRUE(VxSetSIMDOverride(wanted));ASSERT_EQ(VxGetSIMDEffectiveBackend(),wanted);}
    void TearDown()override{EXPECT_TRUE(VxSetSIMDOverride(previous));}
    ICFn Function()const{
#if defined(VX_SIMD_SSE)
        if(GetParam()==2)return ICRaw;
#endif
        return VxIndexedCopy;
    }
};
TEST_P(IndexedCopyReference, GatherSignedRepeatedAndUnalignedIndicesWithZeroStrides){const ICResult r=ICBasic(Function());EXPECT_EQ(r.calls,23100);EXPECT_EQ(r.errors+r.faults,0);}
TEST_P(IndexedCopyReference, PreserveSmallPairAndTailFeedback){const ICResult r=ICAlias(Function(),false);EXPECT_EQ(r.calls,16416);EXPECT_EQ(r.errors+r.faults,0);}
TEST_P(IndexedCopyReference, PreserveGenericDwordFeedback){const ICResult r=ICAlias(Function(),true);EXPECT_EQ(r.calls,24624);EXPECT_EQ(r.errors+r.faults,0);}
TEST_P(IndexedCopyReference, ReadEachIndexAfterEarlierOutputWrites){const ICResult r=ICIndexFeedback(Function());EXPECT_EQ(r.calls,4320);EXPECT_EQ(r.errors+r.faults,0);}
TEST_P(IndexedCopyReference, SnapshotDescriptorsBeforeWritingTheirFields){const ICResult r=ICDescriptors(Function());EXPECT_EQ(r.calls,2);EXPECT_EQ(r.errors+r.faults,0);}
TEST_P(IndexedCopyReference, MatchIndependentLiteralPairsAndZeroStrideExtremes){const ICResult r=ICLiteral(Function());EXPECT_EQ(r.calls,9);EXPECT_EQ(r.errors+r.faults,0);}
// Guard-page and no-access rejection probes require the Windows memory API.
#if defined(_WIN32)
TEST_P(IndexedCopyReference, RespectReadOnlySourceIndicesAndExactGuardedBounds){const ICResult r=ICFootprint(Function());EXPECT_EQ(r.calls,3780);EXPECT_EQ(r.errors+r.faults,0);}
TEST_P(IndexedCopyReference, KeepExistingSafeParameterRejections){const ICResult r=ICRejected(Function(),1);EXPECT_EQ(r.calls,12);EXPECT_EQ(r.errors+r.faults,0);}
TEST_P(IndexedCopyReference, RejectNegativeCountsWithoutAccess){const ICResult r=ICRejected(Function(),2);EXPECT_EQ(r.calls,14);EXPECT_EQ(r.errors+r.faults,0);}
#endif
#if defined(VX_SIMD_SSE)
INSTANTIATE_TEST_SUITE_P(ScalarSSE2AndRaw,IndexedCopyReference,testing::Values(0,1,2));
#else
INSTANTIATE_TEST_SUITE_P(ScalarOnly,IndexedCopyReference,testing::Values(0));
#endif
#endif
