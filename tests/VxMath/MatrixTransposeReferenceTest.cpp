#include <cstring>
#include <cstddef>
#include <new>
#include <type_traits>
#include <initializer_list>
#ifndef VX_MATRIX_TRANSPOSE_PROBE
#include "VxMatrix.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef void (*MTFn)(VxMatrix &, const VxMatrix &);
struct MTResult { int samples=0, calls=0, errors=0; };
static void MTFree(VxMatrix &out, const VxMatrix &in) { Vx3DTransposeMatrix(out,in); }
#if defined(VX_SIMD_SSE)
static void MTTyped(VxMatrix &out, const VxMatrix &in) { VxSIMDTransposeMatrixM(&out,&in); }
static void MTRaw(VxMatrix &out, const VxMatrix &in) { VxSIMDMatrixTranspose(out[0],in[0]); }
#endif
static unsigned MTNext(unsigned &seed) { seed=seed*1664525u+1013904223u;return seed; }
static void MTPermute(unsigned *out, const unsigned *in) {
    for (int r=0;r<4;++r) for (int c=0;c<4;++c) out[4*c+r]=in[4*r+c];
}
static void MTLabels(unsigned *words) {
    for (int i=0;i<16;++i) words[i]=0x12345678u+0x01234567u*unsigned(i);
}
static bool MTApply(MTFn fn, const unsigned *words, bool alias) {
    struct Guarded { unsigned left[4];VxMatrix matrix;unsigned right[4]; } input,output;
    unsigned beforeInput[sizeof(input)/4],beforeOutput[sizeof(output)/4],expected[16];
    std::memset(&input,0xa5,sizeof(input));std::memset(&output,0x5a,sizeof(output));std::memcpy(&input.matrix,words,64);
    std::memcpy(beforeInput,&input,sizeof(input));std::memcpy(beforeOutput,&output,sizeof(output));MTPermute(expected,words);
    fn(alias ? input.matrix : output.matrix,input.matrix);
    if (alias) std::memcpy(reinterpret_cast<unsigned char *>(beforeInput)+offsetof(Guarded,matrix),expected,64);
    else std::memcpy(reinterpret_cast<unsigned char *>(beforeOutput)+offsetof(Guarded,matrix),expected,64);
    return std::memcmp(&input,beforeInput,sizeof(input))==0 && std::memcmp(&output,beforeOutput,sizeof(output))==0;
}
static MTResult MTLiteral(MTFn fn) {
    unsigned words[16];MTLabels(words);MTResult r;
    for (bool alias : {false,true}) { ++r.samples;++r.calls;r.errors+=!MTApply(fn,words,alias); }
    return r;
}
static MTResult MTFields(MTFn fn) {
    MTResult r;
    for (int field=0;field<16;++field) for (int bit=0;bit<32;++bit) {
        unsigned words[16]={};words[field]=1u<<bit;
        for (bool alias : {false,true}) { ++r.samples;++r.calls;r.errors+=!MTApply(fn,words,alias); }
    }
    return r;
}
static MTResult MTPatterns(MTFn fn) {
    MTResult r;
    for (int field=0;field<16;++field) for (unsigned bits : {0u,0x80000000u,1u,0x80000001u,0x007fffffu,0x00800000u,0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,0x7fc12345u,0xffc12345u,0x7f800001u,0xff800001u,0x3f800000u,0xffffffffu}) {
        unsigned words[16];MTLabels(words);words[field]=bits;
        for (bool alias : {false,true}) { ++r.samples;++r.calls;r.errors+=!MTApply(fn,words,alias); }
    }
    return r;
}
static MTResult MTRandom(MTFn fn) {
    MTResult r;unsigned seed=302;
    for (int i=0;i<4096;++i) {
        unsigned words[16];for (unsigned &v : words) v=MTNext(seed);
        for (bool alias : {false,true}) { ++r.samples;++r.calls;r.errors+=!MTApply(fn,words,alias); }
    }
    return r;
}
static MTResult MTRoundtrip(MTFn fn) {
    MTResult r;unsigned seed=310;
    for (int sample=0;sample<256;++sample) for (bool alias : {false,true}) {
        unsigned words[16];for (unsigned &v : words) v=MTNext(seed);VxMatrix a,b,c;std::memcpy(&a,words,64);
        if (alias) { fn(a,a);fn(a,a);++r.samples;r.calls+=2;r.errors+=std::memcmp(&a,words,64)!=0; }
        else { fn(b,a);fn(c,b);++r.samples;r.calls+=2;r.errors+=std::memcmp(&c,words,64)!=0 || std::memcmp(&a,words,64)!=0; }
    }
    return r;
}
static MTResult MTFootprint(MTFn fn) {
    MTResult r;unsigned words[16],expected[16];MTLabels(words);words[3]=0x7f800001u;words[6]=0x80000000u;MTPermute(expected,words);
    for (int ss=0;ss<16;ss+=4) for (int ds=0;ds<16;ds+=4) {
        alignas(16) unsigned char input[128],output[128],beforeInput[128],beforeOutput[128];std::memset(input,0xa5,128);std::memset(output,0x5a,128);
        VxMatrix *a=new(input+16+ss) VxMatrix,*b=new(output+16+ds) VxMatrix;std::memcpy(a,words,64);std::memcpy(beforeInput,input,128);std::memcpy(beforeOutput,output,128);std::memcpy(beforeOutput+16+ds,expected,64);
        fn(*b,*a);++r.samples;++r.calls;r.errors+=std::memcmp(input,beforeInput,128)!=0 || std::memcmp(output,beforeOutput,128)!=0;
    }
    for (int shift=0;shift<16;shift+=4) {
        alignas(16) unsigned char bytes[128],expectedBytes[128];std::memset(bytes,0xa5,128);VxMatrix *m=new(bytes+16+shift) VxMatrix;std::memcpy(m,words,64);std::memcpy(expectedBytes,bytes,128);std::memcpy(expectedBytes+16+shift,expected,64);
        fn(*m,*m);++r.samples;++r.calls;r.errors+=std::memcmp(bytes,expectedBytes,128)!=0;
    }
#if defined(_WIN32)
    SYSTEM_INFO info;GetSystemInfo(&info);const unsigned size=info.dwPageSize;
    unsigned char *memory=static_cast<unsigned char *>(VirtualAlloc(nullptr,5*size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if (!memory) { ++r.errors;return r; }DWORD old;
    for (unsigned page : {0u,2u,4u}) if (!VirtualProtect(memory+page*size,size,PAGE_NOACCESS,&old)) { VirtualFree(memory,0,MEM_RELEASE);++r.errors;return r; }
    for (int edge=0;edge<4;++edge) {
        std::memset(memory+size,0xa5,size);std::memset(memory+3*size,0x5a,size);
        VxMatrix *a=new(memory+size+(edge&1 ? size-64 : 0)) VxMatrix,*b=new(memory+3*size+(edge&2 ? size-64 : 0)) VxMatrix;std::memcpy(a,words,64);
        if (!VirtualProtect(memory+size,size,PAGE_READONLY,&old)) { VirtualFree(memory,0,MEM_RELEASE);++r.errors;return r; }
        fn(*b,*a);bool ok=std::memcmp(a,words,64)==0 && std::memcmp(b,expected,64)==0;
        const unsigned offset=edge&2 ? size-64 : 0;for (unsigned i=0;i<size;++i) if (i<offset || i>=offset+64) ok=ok && memory[3*size+i]==0x5a;
        ++r.samples;++r.calls;r.errors+=!ok;
        if (!VirtualProtect(memory+size,size,PAGE_READWRITE,&old)) { VirtualFree(memory,0,MEM_RELEASE);++r.errors;return r; }
    }
    for (int edge=0;edge<2;++edge) {
        std::memset(memory+size,0xa5,size);const unsigned offset=edge ? size-64 : 0;
        VxMatrix *m=new(memory+size+offset) VxMatrix;std::memcpy(m,words,64);fn(*m,*m);bool ok=std::memcmp(m,expected,64)==0;
        for (unsigned i=0;i<size;++i) if (i<offset || i>=offset+64) ok=ok && memory[size+i]==0xa5;
        ++r.samples;++r.calls;r.errors+=!ok;
    }
    VirtualFree(memory,0,MEM_RELEASE);
#endif
    return r;
}
#if defined(VX_SIMD_SSE)
static MTResult MTOverlap() {
    MTResult r;unsigned seed=307;
    for (int sample=0;sample<64;++sample) for (int delta=-15;delta<=15;++delta) {
        float values[64];unsigned before[64],expected[64],transposed[16];for (unsigned &v : before) v=MTNext(seed);
        std::memcpy(values,before,sizeof(values));std::memcpy(expected,before,sizeof(expected));MTPermute(transposed,before+16);std::memcpy(expected+16+delta,transposed,64);
        VxSIMDMatrixTranspose(values+16+delta,values+16);++r.samples;++r.calls;r.errors+=std::memcmp(values,expected,sizeof(values))!=0;
    }
    return r;
}
#endif
#ifndef VX_MATRIX_TRANSPOSE_PROBE
static void MTExpect(MTResult r, int samples, int calls) { EXPECT_EQ(r.samples,samples);EXPECT_EQ(r.calls,calls);EXPECT_EQ(r.errors,0); }
#if defined(_WIN32)
static const int MTBoundarySamples=26;
#else
static const int MTBoundarySamples=20;
#endif
#define MT_CASES(Label, Function) \
TEST(MatrixTransposeReference, Label##LiteralBitPositions) { MTExpect(MTLiteral(Function),2,2); } \
TEST(MatrixTransposeReference, Label##EveryBitInEveryField) { MTExpect(MTFields(Function),1024,1024); } \
TEST(MatrixTransposeReference, Label##IEEEPatternsAndPayloads) { MTExpect(MTPatterns(Function),512,512); } \
TEST(MatrixTransposeReference, Label##RandomRepresentationsAndCanaries) { MTExpect(MTRandom(Function),8192,8192); } \
TEST(MatrixTransposeReference, Label##TwoApplicationsRestoreBits) { MTExpect(MTRoundtrip(Function),512,1024); } \
TEST(MatrixTransposeReference, Label##UnalignedAndProtectedBoundaries) { MTExpect(MTFootprint(Function),MTBoundarySamples,MTBoundarySamples); }
MT_CASES(Public, MTFree)
#if defined(VX_SIMD_SSE)
MT_CASES(TypedSIMD, MTTyped)
MT_CASES(RawSIMD, MTRaw)
TEST(MatrixTransposeReference, RawOverlappingFloatArraysUseInputSnapshot) { MTExpect(MTOverlap(),1984,1984); }
#endif
#undef MT_CASES
TEST(MatrixTransposeReference, EntrySignatures) {
    static_assert(sizeof(unsigned)==4 && sizeof(VxMatrix)==64,"Matrix transpose operates on sixteen 32-bit fields");
    static_assert(std::is_same<decltype(&Vx3DTransposeMatrix),MTFn>::value,"Public transpose signature");
#if defined(VX_SIMD_SSE)
    static_assert(std::is_same<decltype(&VxSIMDMatrixTranspose),void(*)(float *,const float *) noexcept>::value,"Raw transpose signature");
    static_assert(std::is_same<decltype(&VxSIMDTransposeMatrixM),void(*)(VxMatrix *,const VxMatrix *) noexcept>::value,"Typed transpose signature");
#endif
}
#endif
