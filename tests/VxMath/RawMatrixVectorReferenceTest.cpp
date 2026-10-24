#include <cmath>
#include <cstring>
#include <new>
#include <initializer_list>
#ifndef VX_RAW_MATRIX_PROBE
#include "VxMatrix.h"
#include "VxFrustum.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef void (*RMFn)(VxVector4 *, const VxMatrix &, const void *, int);
struct RMResult { int samples=0, errors=0; };
static bool RMRotation(int kind) { return kind==1 || kind==4; }
static int RMChannels(int kind) {
#if defined(VX_RAW_MATRIX_NATIVE)
    if (RMRotation(kind)) return 3;
#endif
    return 4;
}
#if defined(VX_SIMD_SSE)
static void RMRaw(VxVector4 *out, const VxMatrix &m, const void *source, int kind) {
    __m128 result;
    if (kind>=3) {
        const VxVector *p=static_cast<const VxVector *>(source);
        const __m128 r0=_mm_loadu_ps(m[0]),r1=_mm_loadu_ps(m[1]),r2=_mm_loadu_ps(m[2]);
        result=kind==3 ? VxSIMDTransformVec3Rows(r0,r1,r2,_mm_loadu_ps(m[3]),p) : VxSIMDRotateVec3Rows(r0,r1,r2,p);
    } else {
        const __m128 p=VxSIMDLoadFloat4(static_cast<const float *>(source));
        result=kind==0 ? VxSIMDMatrixMultiplyVector3(m[0],p) : kind==1 ? VxSIMDMatrixRotateVector3(m[0],p) : VxSIMDMatrixMultiplyVector4(m[0],p);
    }
    VxSIMDStoreFloat4(&out->x,result);
}
#endif
static unsigned RMNext(unsigned &seed) { seed=seed*1664525u+1013904223u;return seed; }
static bool RMEqual(const VxVector4 &a, const VxVector4 &b, int kind) {
    for (int c=0;c<RMChannels(kind);++c) if (!(a[c]==b[c] || (std::isnan(a[c]) && std::isnan(b[c])))) return false;
    return true;
}
static VxVector4 RMInteger(const VxMatrix &m, const VxVector4 &p, int kind) {
    VxVector4 out;
    for (int c=0;c<4;++c) {
        long long value=RMRotation(kind) ? 0 : static_cast<long long>(m[3][c])*static_cast<long long>(kind==2 ? p.w : 1);
        for (int r=0;r<3;++r) value+=static_cast<long long>(m[r][c])*static_cast<long long>(p[r]);
        out[c]=float(value);
    }
    return out;
}
static VxMatrix RMMatrix() {
    const float fields[4][4]={{2,1,-1,7},{3,-2,4,-3},{-1,5,2,9},{7,11,-13,2}};
    VxMatrix m;std::memcpy(&m,fields,64);return m;
}
static bool RMData(RMFn fn, const VxMatrix &m, const VxVector4 &p, const VxVector4 &expected, int kind, bool alias) {
    VxMatrix matrix=m;VxVector4 input=p,output(123,456,789,987);const VxVector4 before=input;
    fn(alias ? &input : &output,matrix,&input,kind);
    return RMEqual(alias ? input : output,expected,kind) && std::memcmp(&m,&matrix,64)==0 && (alias || std::memcmp(&input,&before,16)==0);
}
static RMResult RMLiteral(RMFn fn) {
    RMResult r;const VxMatrix m=RMMatrix();const VxVector4 p(2,-3,5,2);
    const VxVector4 expected[3]={VxVector4(-3,44,-17,70),VxVector4(-10,33,-4,68),VxVector4(4,55,-30,72)};
    for (int kind=0;kind<5;++kind) for (bool alias : {false,true}) { ++r.samples;r.errors+=!RMData(fn,m,p,expected[kind>=3 ? kind-3 : kind],kind,alias); }
    return r;
}
static RMResult RMIntegers(RMFn fn) {
    RMResult r;unsigned seed=301;
    for (int sample=0;sample<1024;++sample) {
        VxMatrix m;VxVector4 p;
        for (int row=0;row<4;++row) for (int c=0;c<4;++c) m[row][c]=float(int(RMNext(seed)%17)-8);
        for (int c=0;c<4;++c) p[c]=float(int(RMNext(seed)%17)-8);
        for (int kind=0;kind<5;++kind) for (bool alias : {false,true}) { ++r.samples;r.errors+=!RMData(fn,m,p,RMInteger(m,p,kind),kind,alias); }
    }
    return r;
}
static RMResult RMFields(RMFn fn) {
    RMResult r;const VxVector4 p(2,3,5,7);
    for (int field=0;field<16;++field) {
        VxMatrix m;m.Clear();m[field/4][field%4]=7;
        for (int kind=0;kind<5;++kind) for (bool alias : {false,true}) { ++r.samples;r.errors+=!RMData(fn,m,p,RMInteger(m,p,kind),kind,alias); }
    }
    return r;
}
static RMResult RMIgnoredW(RMFn fn) {
    RMResult r;const VxMatrix m=RMMatrix();
    for (int kind : {0,1,3,4}) for (unsigned bits : {0u,0x80000000u,0x3f800000u,0xc0000000u,0x3f000000u,0x7f800000u,0xff800000u,0x7fc12345u}) {
        VxVector4 p(2,-3,5,1);const VxVector4 expected=RMInteger(m,p,kind);std::memcpy(&p.w,&bits,4);
        for (bool alias : {false,true}) { ++r.samples;r.errors+=!RMData(fn,m,p,expected,kind,alias); }
    }
    return r;
}
static RMResult RMIgnoredTranslation(RMFn fn) {
    RMResult r;const VxVector4 p(2,-3,5,2);
    for (int kind : {1,4}) for (int c=0;c<4;++c) for (unsigned bits : {0u,0x80000000u,0x3f800000u,0xc0000000u,0x3f000000u,0x7f800000u,0xff800000u,0x7fc12345u}) {
        VxMatrix m=RMMatrix();const VxVector4 expected=RMInteger(m,p,kind);std::memcpy(&m[3][c],&bits,4);
        for (bool alias : {false,true}) { ++r.samples;r.errors+=!RMData(fn,m,p,expected,kind,alias); }
    }
    return r;
}
static RMResult RMMatrixAliases(RMFn fn) {
    RMResult r;
    for (int kind=0;kind<5;++kind) for (int input=-1;input<4;++input) for (int output=-1;output<4;++output) {
        VxMatrix m=RMMatrix(),expectedMatrix=m;VxVector4 p(2,-3,5,2),out;
        const VxVector4 point=input<0 ? p : m[input];const VxVector4 expected=RMInteger(m,point,kind);
        if (output>=0) for (int c=0;c<RMChannels(kind);++c) expectedMatrix[output][c]=expected[c];
        fn(output<0 ? &out : &m[output],m,input<0 ? &p : &m[input],kind);
        ++r.samples;r.errors+=!(RMEqual(output<0 ? out : m[output],expected,kind) && std::memcmp(&m,&expectedMatrix,64)==0 && p==VxVector4(2,-3,5,2));
    }
    return r;
}
static RMResult RMNumerics(RMFn fn, int group) {
    RMResult r;
    for (int kind=0;kind<5;++kind) {
        const int terms=RMRotation(kind) || (group!=0 && kind!=2) ? 3 : 4;
        for (int col=0;col<4;++col) for (int shift=0;shift<terms;++shift) {
            VxMatrix m;m.Clear();VxVector4 p(1,1,1,1),expected(0,0,0,0);float values[4],weights[4];
            if (group==0) { values[0]=std::ldexp(1.0f,80);values[1]=1;values[2]=-values[0];values[3]=1;for (float &v : weights) v=1;expected[col]=float(terms-2); }
            else if (group==1) { values[0]=std::ldexp(1.0f,100);values[1]=-values[0];values[2]=1;values[3]=2;weights[0]=weights[1]=std::ldexp(1.0f,40);weights[2]=weights[3]=1;expected[col]=terms==3 ? 1.0f : 3.0f; }
            else { for (int i=0;i<4;++i) { values[i]=std::ldexp(1.0f,-100);weights[i]=std::ldexp(1.0f,-51); }expected[col]=std::ldexp(1.0f,-149); }
            for (int i=0;i<terms;++i) {
                const int at=(i+shift)%terms;
                if (at==3 && kind!=2) m[at][col]=values[i]*weights[i];
                else { m[at][col]=values[i];p[at]=weights[i]; }
            }
            if (group==1 && (kind==0 || kind==3)) { m[3][col]=2;expected[col]=3; }
            for (bool alias : {false,true}) { ++r.samples;r.errors+=!RMData(fn,m,p,expected,kind,alias); }
        }
    }
    return r;
}
static RMResult RMNonfinite(RMFn fn) {
    RMResult r;const VxVector4 p(1,1,1,1);
    for (int kind=0;kind<5;++kind) for (int col=0;col<4;++col) for (int value=0;value<3;++value) {
        VxMatrix m;m.Clear();m[0][col]=value==1 ? -HUGE_VALF : HUGE_VALF;if (value==2) m[1][col]=-HUGE_VALF;
        VxVector4 expected(0,0,0,0);expected[col]=value==2 ? NAN : m[0][col];
        for (bool alias : {false,true}) { ++r.samples;r.errors+=!RMData(fn,m,p,expected,kind,alias); }
    }
    return r;
}
static RMResult RMFootprint(RMFn fn) {
    RMResult r;const VxMatrix matrix=RMMatrix();const VxVector4 p(2,-3,5,2);
    for (int kind=0;kind<5;++kind) for (int shift=0;shift<16;shift+=4) {
        alignas(16) unsigned char bytes[192];std::memset(bytes,0xa5,sizeof(bytes));
        VxVector4 *out=new(bytes+32+shift) VxVector4;unsigned char before[192];std::memcpy(before,bytes,192);
        fn(out,matrix,&p,kind);bool ok=RMEqual(*out,RMInteger(matrix,p,kind),kind);
        for (int b=0;b<192;++b) if (b<32+shift || b>=32+shift+4*RMChannels(kind)) ok=ok && bytes[b]==before[b];
        ++r.samples;r.errors+=!ok;
    }
#if defined(_WIN32)
    SYSTEM_INFO info;GetSystemInfo(&info);const unsigned size=info.dwPageSize;
    unsigned char *memory=static_cast<unsigned char *>(VirtualAlloc(nullptr,9*size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if (!memory) { ++r.errors;return r; }DWORD old;
    for (int kind=0;kind<5;++kind) for (int edge=0;edge<8;++edge) {
        for (unsigned page : {0u,2u,3u,5u,6u,8u}) if (!VirtualProtect(memory+page*size,size,PAGE_NOACCESS,&old)) { VirtualFree(memory,0,MEM_RELEASE);++r.errors;return r; }
        // Construct before protecting the translation row of a rotation matrix.
        if (!VirtualProtect(memory+2*size,size,PAGE_READWRITE,&old)) { VirtualFree(memory,0,MEM_RELEASE);++r.errors;return r; }
        const unsigned matrixBytes=RMRotation(kind) ? 48 : 64,sourceBytes=kind>=3 ? 12 : 16;
        VxMatrix *m=new(memory+size+(edge&1 ? size-matrixBytes : 0)) VxMatrix(matrix);
        if (!VirtualProtect(memory+2*size,size,PAGE_NOACCESS,&old)) { VirtualFree(memory,0,MEM_RELEASE);++r.errors;return r; }
        void *source=memory+4*size+(edge&2 ? size-sourceBytes : 0);
        if (kind>=3) new(source) VxVector(p.x,p.y,p.z);else new(source) VxVector4(p);
        VxVector4 *out=new(memory+7*size+(edge&4 ? size-16 : 0)) VxVector4(123,456,789,987);
        for (unsigned page : {1u,4u}) if (!VirtualProtect(memory+page*size,size,PAGE_READONLY,&old)) { VirtualFree(memory,0,MEM_RELEASE);++r.errors;return r; }
        fn(out,*m,source,kind);++r.samples;r.errors+=!RMEqual(*out,RMInteger(matrix,p,kind),kind);
        for (unsigned page : {1u,4u}) if (!VirtualProtect(memory+page*size,size,PAGE_READWRITE,&old)) { VirtualFree(memory,0,MEM_RELEASE);++r.errors;return r; }
    }
    VirtualFree(memory,0,MEM_RELEASE);
#endif
    return r;
}
#if !defined(VX_RAW_MATRIX_PROBE) && defined(VX_SIMD_SSE)
static void RMExpect(RMResult r, int count) { EXPECT_EQ(r.samples,count);EXPECT_EQ(r.errors,0); }
TEST(RawMatrixVectorReference, LiteralFourLaneImages) { RMExpect(RMLiteral(RMRaw),10); }
TEST(RawMatrixVectorReference, IntegerLinearAndAffineImages) { RMExpect(RMIntegers(RMRaw),10240); }
TEST(RawMatrixVectorReference, AllMatrixFieldsIncludingOutputW) { RMExpect(RMFields(RMRaw),160); }
TEST(RawMatrixVectorReference, ThreeComponentInputsIgnoreRegisterW) { RMExpect(RMIgnoredW(RMRaw),64); }
TEST(RawMatrixVectorReference, RotationsIgnoreEntireTranslationRow) { RMExpect(RMIgnoredTranslation(RMRaw),128); }
TEST(RawMatrixVectorReference, SourceDestinationAndMatrixAliases) { RMExpect(RMMatrixAliases(RMRaw),125); }
TEST(RawMatrixVectorReference, SevereCancellationRetainsSmallTerms) { RMExpect(RMNumerics(RMRaw,0),144); }
TEST(RawMatrixVectorReference, FiniteResultsSurviveProductOverflow) { RMExpect(RMNumerics(RMRaw,1),128); }
TEST(RawMatrixVectorReference, ProductTailsFormSubnormalResults) { RMExpect(RMNumerics(RMRaw,2),128); }
TEST(RawMatrixVectorReference, NonfiniteResultClassification) { RMExpect(RMNonfinite(RMRaw),120); }
TEST(RawMatrixVectorReference, ExactMatrixSourceAndDestinationPageEdges) {
#if defined(_WIN32)
    RMExpect(RMFootprint(RMRaw),60);
#else
    RMExpect(RMFootprint(RMRaw),20);
#endif
}
TEST(RawMatrixVectorReference, FrustumTransformKeepsCancelledOrigin) {
    VxMatrix m;m.SetIdentity();m[0][0]=std::ldexp(1.0f,80);m[1][0]=1;m[2][0]=-m[0][0];m[3][0]=1;
    VxFrustum frustum(VxVector(1,1,1),VxVector(1,0,0),VxVector(0,1,0),VxVector(0,0,1),1,10,1,1);
    frustum.Transform(m);EXPECT_EQ(frustum.GetOrigin().x,2);EXPECT_EQ(frustum.GetOrigin().y,1);EXPECT_EQ(frustum.GetOrigin().z,1);
}
#endif
