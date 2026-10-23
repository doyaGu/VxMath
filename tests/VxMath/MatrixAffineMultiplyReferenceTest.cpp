#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <new>
#include <initializer_list>
#ifndef VX_MATRIX_AFFINE_PROBE
#ifndef VX_MATRIX_AFFINE_HEADER
#define VX_MATRIX_AFFINE_HEADER "VxMatrix.h"
#endif
#include VX_MATRIX_AFFINE_HEADER
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef void (*MAFFn)(VxMatrix &,const VxMatrix &,const VxMatrix &);
struct MAFResult { int samples=0, errors=0; };
static unsigned MAFNext(unsigned &s) { s=s*1664525u+1013904223u;return s; }
static void MAFFree(VxMatrix &r,const VxMatrix &a,const VxMatrix &b) { Vx3DMultiplyMatrix(r,a,b); }
#if defined(VX_SIMD_SSE)
static void MAFSimd(VxMatrix &r,const VxMatrix &a,const VxMatrix &b) { VxSIMDMultiplyMatrixM(&r,&a,&b); }
#endif
static bool MAFShape(const VxMatrix &m) { return m[0][3]==0 && m[1][3]==0 && m[2][3]==0 && m[3][3]==1; }
static void MAFPoint(const VxMatrix &m,const double *p,double *out) {
    for(int c=0;c<3;++c) { out[c]=m[3][c];for(int r=0;r<3;++r)out[c]+=p[r]*m[r][c]; }
}
static void MAFExpected(VxMatrix &out,const VxMatrix &a,const VxMatrix &b) {
    // Recover the affine map from the images of origin and three basis points, B followed by A.
    double origin[3]={},middle[3],mapped[3];MAFPoint(b,origin,middle);MAFPoint(a,middle,mapped);
    out.Clear();for(int c=0;c<3;++c)out[3][c]=float(mapped[c]);out[3][3]=1;
    for(int axis=0;axis<3;++axis) {
        double basis[3]={},q[3],v[3];basis[axis]=1;MAFPoint(b,basis,q);MAFPoint(a,q,v);
        for(int c=0;c<3;++c)out[axis][c]=float(v[c]-mapped[c]);
    }
}
static bool MAFEqual(const VxMatrix &a,const VxMatrix &b) {
    for(int r=0;r<4;++r)for(int c=0;c<4;++c)if(a[r][c]!=b[r][c])return false;return true;
}
static bool MAFExecute(VxMatrix &result,MAFFn fn,const VxMatrix &a,const VxMatrix &b,int mode) {
    VxMatrix left=a,right=b;bool ok=true;
    switch(mode) {
    case 0:fn(result,left,right);break;
    case 1:fn(left,left,right);result=left;break;
    case 2:fn(right,left,right);result=right;break;
    case 3:fn(result,left,left);break;
    case 4:fn(left,left,left);result=left;break;
    case 5:result=left*right;break;
    case 6:ok=&(left*=right)==&left;result=left;break;
    case 7:ok=&(left*=left)==&left;result=left;break;
    }
    if(mode==0||mode==2||mode==3||mode==5)ok=ok&&std::memcmp(&left,&a,64)==0;
    if(mode!=2)ok=ok&&std::memcmp(&right,&b,64)==0;
    return ok;
}
static MAFResult MAFIntegers(MAFFn fn,bool translations=false) {
    MAFResult out;unsigned seed=290;
    for(int sample=0;sample<256;++sample) {
        VxMatrix a,b;a.SetIdentity();b.SetIdentity();
        for(int r=0;r<4;++r)for(int c=0;c<3;++c)if(!translations||r==3) { a[r][c]=float(int(MAFNext(seed)%17)-8);b[r][c]=float(int(MAFNext(seed)%17)-8); }
        for(int mode=0;mode<8;++mode) {
            VxMatrix expected,actual;MAFExpected(expected,a,mode==3||mode==4||mode==7?a:b);
            const bool ok=MAFExecute(actual,fn,a,b,mode)&&MAFEqual(actual,expected);++out.samples;out.errors+=!ok;
        }
    }
    return out;
}
static MAFResult MAFLiteral(MAFFn fn) {
    const float aa[4][4]={{2,0,0,0},{0,3,0,0},{0,0,4,0},{5,7,11,1}};
    const float bb[4][4]={{1,2,0,0},{0,1,0,0},{0,0,1,0},{13,17,19,1}};
    const float rr[4][4]={{2,6,0,0},{0,3,0,0},{0,0,4,0},{31,58,87,1}};
    VxMatrix a,b,expected;std::memcpy(&a,aa,64);std::memcpy(&b,bb,64);std::memcpy(&expected,rr,64);MAFResult out;
    for(int mode:{0,1,2,5,6}) { VxMatrix actual;const bool ok=MAFExecute(actual,fn,a,b,mode)&&MAFEqual(actual,expected);++out.samples;out.errors+=!ok; }
    return out;
}
static MAFResult MAFIgnored(MAFFn fn) {
    MAFResult out;
    const unsigned patterns[]={0,0x3f800000,0x40000000,0xc0400000,0x7f800000,0xff800000,0x7fc12345,0x80000000};
    for(unsigned pattern:patterns)for(int row=0;row<4;++row) {
        VxMatrix a,b;a.SetIdentity();b.SetIdentity();a[0][0]=2;a[3][0]=5;a[3][1]=7;a[3][2]=11;b[1][0]=3;b[3][0]=13;
        VxMatrix expected;MAFExpected(expected,a,b);std::memcpy(&b[row][3],&pattern,4);
        for(int mode:{0,1,2,5,6}) { VxMatrix actual;const bool ok=MAFExecute(actual,fn,a,b,mode)&&MAFEqual(actual,expected);++out.samples;out.errors+=!ok; }
    }
    return out;
}
static MAFResult MAFNumerics(MAFFn fn,int kind) {
    MAFResult out;
    for(int row=0;row<4;++row)for(int col=0;col<3;++col) {
        VxMatrix a,b;a.SetIdentity();b.SetIdentity();a[3][col]=0;
        float x,y,z,bx,by,bz,expected;
        if(kind==0) { x=std::ldexp(1.0f,80);y=1;z=-x;bx=by=bz=1;expected=1; }
        else if(kind==1) { x=std::ldexp(1.0f,100);y=-x;z=1;bx=by=std::ldexp(1.0f,40);bz=1;expected=1; }
        else { x=y=std::ldexp(1.0f,-100);z=0;bx=by=std::ldexp(1.0f,-50);bz=0;expected=std::ldexp(1.0f,-149); }
        a[0][col]=x;a[1][col]=y;a[2][col]=z;b[row][0]=bx;b[row][1]=by;b[row][2]=bz;
        for(int mode:{0,1,2,5,6}) { VxMatrix actual;const bool ok=MAFExecute(actual,fn,a,b,mode)&&MAFShape(actual)&&actual[row][col]==expected;++out.samples;out.errors+=!ok; }
    }
    return out;
}
static MAFResult MAFFootprint(MAFFn fn) {
    MAFResult result;
    VxMatrix a,b,expected;a.SetIdentity();b.SetIdentity();a[0][0]=2;a[3][0]=5;b[1][0]=3;b[3][1]=7;MAFExpected(expected,a,b);
    for(int shift=0;shift<16;shift+=4) {
        alignas(16) unsigned char bytes[320],before[320];std::memset(bytes,0xa5,sizeof(bytes));
        VxMatrix *pa=new(bytes+16+shift)VxMatrix(a),*pb=new(bytes+112+shift)VxMatrix(b),*pr=new(bytes+208+shift)VxMatrix;
        std::memcpy(before,bytes,sizeof(bytes));fn(*pr,*pa,*pb);bool ok=MAFEqual(*pr,expected);
        for(int i=0;i<320;++i)if(i<208+shift||i>=272+shift)ok=ok&&bytes[i]==before[i];
        ++result.samples;result.errors+=!ok;pa->~VxMatrix();pb->~VxMatrix();pr->~VxMatrix();
    }
#if defined(_WIN32)
    SYSTEM_INFO info;GetSystemInfo(&info);const unsigned size=info.dwPageSize;
    unsigned char *memory=static_cast<unsigned char *>(VirtualAlloc(nullptr,9*size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!memory){++result.errors;return result;}DWORD old;bool ok=true;
    for(unsigned page:{0u,2u,3u,5u,6u,8u})ok=ok&&VirtualProtect(memory+page*size,size,PAGE_NOACCESS,&old);
    if(!ok){VirtualFree(memory,0,MEM_RELEASE);++result.errors;return result;}
    for(int i=0;i<8;++i) {
        VxMatrix *pa=new(memory+size+(i&1?size-64:0))VxMatrix(a),*pb=new(memory+4*size+(i&2?size-64:0))VxMatrix(b),*pr=new(memory+7*size+(i&4?size-64:0))VxMatrix;
        ok=VirtualProtect(memory+size,size,PAGE_READONLY,&old)&&VirtualProtect(memory+4*size,size,PAGE_READONLY,&old);
        if(!ok){++result.errors;break;}fn(*pr,*pa,*pb);++result.samples;result.errors+=!MAFEqual(*pr,expected);
        VirtualProtect(memory+size,size,PAGE_READWRITE,&old);VirtualProtect(memory+4*size,size,PAGE_READWRITE,&old);
        pa->~VxMatrix();pb->~VxMatrix();pr->~VxMatrix();
    }
    VirtualFree(memory,0,MEM_RELEASE);
#endif
    return result;
}
#ifndef VX_MATRIX_AFFINE_PROBE
static void MAFExpect(MAFResult r,int count) { EXPECT_EQ(r.samples,count);EXPECT_EQ(r.errors,0); }
TEST(MatrixAffineMultiplyReference, CompositionOrderHasIndependentNonCommutingLiteral) { MAFExpect(MAFLiteral(MAFFree),5); }
TEST(MatrixAffineMultiplyReference, IntegerAffineMapsAllAliasAndOperatorEntries) { MAFExpect(MAFIntegers(MAFFree),2048); }
TEST(MatrixAffineMultiplyReference, PureTranslationsAllAliasAndOperatorEntries) { MAFExpect(MAFIntegers(MAFFree,true),2048); }
TEST(MatrixAffineMultiplyReference, RightFourthColumnDoesNotEnterAffineXYZ) { MAFExpect(MAFIgnored(MAFFree),160); }
TEST(MatrixAffineMultiplyReference, CancellationRetainsSmallFiniteCoefficients) { MAFExpect(MAFNumerics(MAFFree,0),60); }
TEST(MatrixAffineMultiplyReference, FiniteCoefficientsSurviveIntermediateOverflow) { MAFExpect(MAFNumerics(MAFFree,1),60); }
TEST(MatrixAffineMultiplyReference, ProductTailsCombineIntoRepresentableSubnormals) { MAFExpect(MAFNumerics(MAFFree,2),60); }
TEST(MatrixAffineMultiplyReference, UnalignedOutputAndReadonlyInputFootprints) {
#if defined(_WIN32)
    MAFExpect(MAFFootprint(MAFFree),12);
#else
    MAFExpect(MAFFootprint(MAFFree),4);
#endif
}
#if defined(VX_SIMD_SSE)
TEST(MatrixAffineMultiplyReference, PackedIntegerMapsAndAliases) { MAFExpect(MAFIntegers(MAFSimd),2048); }
TEST(MatrixAffineMultiplyReference, PackedIgnoredFourthColumn) { MAFExpect(MAFIgnored(MAFSimd),160); }
TEST(MatrixAffineMultiplyReference, PackedCancellationAndScale) { for(int k=0;k<3;++k)MAFExpect(MAFNumerics(MAFSimd,k),60); }
TEST(MatrixAffineMultiplyReference, TranslationDispatchTestsEveryField) {
    VxMatrix m;m.SetIdentity();EXPECT_TRUE(VxSIMDIsPureTranslationMatrix(&m));
    for(int i=0;i<16;++i) { m.SetIdentity();m[i/4][i%4]+=1;EXPECT_EQ(bool(VxSIMDIsPureTranslationMatrix(&m)),i>=12&&i<15); }
}
TEST(MatrixAffineMultiplyReference, PackedOutputAndReadonlyInputFootprints) {
#if defined(_WIN32)
    MAFExpect(MAFFootprint(MAFSimd),12);
#else
    MAFExpect(MAFFootprint(MAFSimd),4);
#endif
}
#endif
#endif
