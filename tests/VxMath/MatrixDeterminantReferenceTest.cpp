#include <cmath>
#include <cfloat>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <new>
#include <initializer_list>
#ifndef VX_MATRIX_DETERMINANT_PROBE
#ifndef VX_MATRIX_DETERMINANT_HEADER
#define VX_MATRIX_DETERMINANT_HEADER "VxMatrix.h"
#endif
#include VX_MATRIX_DETERMINANT_HEADER
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef float (*MDFn)(const VxMatrix &);
struct MDSamples { int samples=0,errors=0,inputErrors=0; };
static void MDFill(VxMatrix &m) { for(int r=0;r<4;++r)for(int c=0;c<4;++c)m[r][c]=0; }
static void MDObserve(MDSamples &r,MDFn fn,const VxMatrix &m,double expected) {
    unsigned before[16];std::memcpy(before,&m[0][0],64);
    const float actual=fn(m),reference=float(expected);const bool input=std::memcmp(before,&m[0][0],64)!=0;
    if(actual!=reference&&r.errors<2)std::fprintf(stderr,"det=%g expected=%g\n",actual,reference);
    ++r.samples;r.inputErrors+=input;r.errors+=(actual!=reference)||input;
}
static unsigned MDNext(unsigned &seed) { seed=seed*1664525u+1013904223u;return seed; }
static double MDIntegerDet(const int *a) {
    // Independent signed permutation expansion in exact integer arithmetic.
    const int permutations[6][3]={{0,1,2},{1,2,0},{2,0,1},{2,1,0},{1,0,2},{0,2,1}};
    std::int64_t result=0;
    for(int j=0;j<6;++j){const int *p=permutations[j];const std::int64_t term=std::int64_t(a[p[0]])*a[3+p[1]]*a[6+p[2]];result+=j<3?term:-term;}
    return double(result);
}
static MDSamples MDInteger(MDFn fn) { MDSamples r;unsigned seed=287;
    for(int i=0;i<5000;++i){VxMatrix m;MDFill(m);int a[9];for(int j=0;j<9;++j){a[j]=int(MDNext(seed)%15)-7;m[j/3][j%3]=float(a[j]);}MDObserve(r,fn,m,MDIntegerDet(a));}return r;
}
static MDSamples MDScale(MDFn fn) { MDSamples r;
    const int permutations[6][3]={{0,1,2},{1,2,0},{2,0,1},{2,1,0},{1,0,2},{0,2,1}};
    for(int p=0;p<6;++p)for(int q=0;q<6;++q)for(int a:{-149,-100,-64,-24,0,24,64,100,127})for(int b:{-149,-100,-64,-24,0,24,64,100,127})for(int sign:{-1,1}){
        int c=-a-b;if(c< -149)c=-149;if(c>127)c=127;
        VxMatrix m;MDFill(m);m[permutations[p][0]][permutations[q][0]]=std::ldexp(float(sign),a);
        m[permutations[p][1]][permutations[q][1]]=std::ldexp(1.0f,b);m[permutations[p][2]][permutations[q][2]]=std::ldexp(1.0f,c);
        const int parity=(p<3?1:-1)*(q<3?1:-1);MDObserve(r,fn,m,std::ldexp(double(sign*parity),a+b+c));
    }return r;
}
static MDSamples MDCancellation(MDFn fn) { MDSamples r;
    for(int e=1;e<=23;++e)for(int shift=0;shift<3;++shift)for(int sign:{-1,1}){
        const float n=std::ldexp(1.0f,e);VxMatrix m;MDFill(m);
        const float a[9]={n,n-1,n-1,n+1,n,n,n+1,n,n+1};
        for(int i=0;i<3;++i)for(int j=0;j<3;++j)m[i][(j+shift)%3]=a[3*i+j]*(i==0?sign:1);
        MDObserve(r,fn,m,sign);
    }return r;
}
static MDSamples MDUnimodular(MDFn fn) { MDSamples r;unsigned seed=7867;
    for(int sample=0;sample<1200;++sample){std::int64_t a[9]={1,0,0,0,1,0,0,0,1};
        for(int op=0;op<30;++op){const int row=int(MDNext(seed)%3),other=(row+1+MDNext(seed)%2)%3,factor=int(MDNext(seed)%15)-7;
            std::int64_t candidate[3];bool fits=true;for(int c=0;c<3;++c){candidate[c]=a[3*row+c]+factor*a[3*other+c];fits=fits&&candidate[c]>=-16777216&&candidate[c]<=16777216;}
            if(fits)for(int c=0;c<3;++c)a[3*row+c]=candidate[c];
        }
        VxMatrix m;MDFill(m);for(int j=0;j<9;++j)m[j/3][j%3]=float(a[j]);
        // Elementary integer row additions preserve determinant exactly one.
        MDObserve(r,fn,m,1);
    }return r;
}
static MDSamples MDSingular(MDFn fn) { MDSamples r;
    for(int row=0;row<3;++row)for(int column=0;column<3;++column)for(int e:{-149,-100,-64,0,64,100,127})for(int sign:{-1,1}){
        const float x=std::ldexp(float(sign),e);VxMatrix m;MDFill(m);
        for(int c=0;c<3;++c){m[row][c]=c==column?x:1;m[(row+1)%3][c]=m[row][c];m[(row+2)%3][c]=c==column?1:0;}
        MDObserve(r,fn,m,0);
    }return r;
}
static MDSamples MDIgnored(MDFn fn) { MDSamples r;
    const unsigned patterns[]={0u,0x80000000u,0x7f800000u,0xff800000u,0x7fc12345u,0x7f7fffffu,0xff7fffffu};
    const int indices[]={3,7,11,12,13,14,15};
    for(unsigned pattern:patterns)for(int index:indices){VxMatrix m;MDFill(m);m[0][0]=2;m[1][1]=-3;m[2][2]=4;
        float value;std::memcpy(&value,&pattern,4);m[index/4][index%4]=value;MDObserve(r,fn,m,-24);
    }return r;
}
static MDSamples MDRounding(MDFn fn) { MDSamples r;
    // Literal halfway cases with an extra determinant term on either side.
    for(int sign:{-1,1})for(int e:{-149,-126,-100,-1,0,60,100,125})for(int adjustment:{-1,0,1}){
        VxMatrix m;MDFill(m);m[0][0]=float(sign);m[1][1]=1.5f;m[2][2]=std::ldexp(std::nextafter(1.0f,2.0f),e);
        m[0][1]=std::ldexp(float(adjustment),e);m[1][2]=std::ldexp(1.0f,-100);m[2][0]=1;
        // At e=-149 the rounded input has no halfway term; compute from it.
        const double base=double(sign)*1.5*double(m[2][2]);float expected=float(base);
        if(adjustment){
            // Compare the literal base with adjacent float midpoints.
            const float lower=std::nextafter(expected,-INFINITY),upper=std::nextafter(expected,INFINITY);
            if(base==(double(expected)+lower)*.5&&adjustment<0)expected=lower;
            if(base==(double(expected)+upper)*.5&&adjustment>0)expected=upper;
        }
        MDObserve(r,fn,m,expected);
    }
    for(int sign:{-1,1})for(int side:{-1,0,1}){
        VxMatrix m;MDFill(m);m[0][0]=sign*FLT_MAX;m[0][1]=float(sign);m[0][2]=std::ldexp(float(sign*side),-103);
        m[1][1]=1;m[1][2]=1;m[2][0]=std::ldexp(1.0f,103);m[2][2]=1;
        // FLT_MAX + half an ULP, with a final term of -1, 0 or +1.
        MDObserve(r,fn,m,side>0?double(sign)*FLT_MAX:double(sign)*INFINITY);
    }
    for(int sign:{-1,1})for(float z:{std::nextafter(2.0f,0.0f),2.0f,std::nextafter(2.0f,3.0f)}){
        VxMatrix m;MDFill(m);m[0][0]=sign*std::ldexp(1.0f,-149);m[0][1]=float(sign);m[0][2]=sign*z;
        m[1][1]=.5f;m[1][2]=1;m[2][0]=std::ldexp(1.0f,-149);m[2][2]=1;
        MDObserve(r,fn,m,z<2?sign*double(std::ldexp(1.0f,-149)):0.0);
    }
    return r;
}
static MDSamples MDFootprint(MDFn fn) { MDSamples r;
    alignas(16) unsigned char bytes[80];
    for(int offset:{0,4,8,12}){VxMatrix *m=new(bytes+offset)VxMatrix;MDFill(*m);(*m)[0][0]=2;(*m)[1][1]=-3;(*m)[2][2]=4;MDObserve(r,fn,*m,-24);m->~VxMatrix();}
#if defined(_WIN32)
    SYSTEM_INFO si;GetSystemInfo(&si);const size_t page=si.dwPageSize;
    char *pages=static_cast<char*>(VirtualAlloc(nullptr,page*2,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));if(!pages){++r.errors;return r;}
    VxMatrix *m=new(pages+page-64)VxMatrix;MDFill(*m);(*m)[0][0]=2;(*m)[1][1]=-3;(*m)[2][2]=4;
    DWORD old;if(!VirtualProtect(pages,page,PAGE_READONLY,&old)||!VirtualProtect(pages+page,page,PAGE_NOACCESS,&old))++r.errors;
    else MDObserve(r,fn,*m,-24);
    VirtualFree(pages,0,MEM_RELEASE);
#endif
    return r;
}
#ifndef VX_MATRIX_DETERMINANT_PROBE
static void MDCheck(MDSamples r) { EXPECT_EQ(r.errors,0)<<"changed inputs="<<r.inputErrors;::testing::Test::RecordProperty("samples",r.samples); }
#define MD_TESTS(prefix,fn) \
TEST(MatrixDeterminantReference,prefix##IntegerSignedVolumes){MDCheck(MDInteger(fn));} \
TEST(MatrixDeterminantReference,prefix##IndependentRowAndColumnScales){MDCheck(MDScale(fn));} \
TEST(MatrixDeterminantReference,prefix##SmallDeterminantsAfterCancellation){MDCheck(MDCancellation(fn));} \
TEST(MatrixDeterminantReference,prefix##ExactElementaryRowOperations){MDCheck(MDUnimodular(fn));} \
TEST(MatrixDeterminantReference,prefix##SingularRowsAcrossScales){MDCheck(MDSingular(fn));} \
TEST(MatrixDeterminantReference,prefix##OnlyUpperLeftThreeByThreeIsUsed){MDCheck(MDIgnored(fn));} \
TEST(MatrixDeterminantReference,prefix##FloatMidpointsKeepSmallSignedTerms){MDCheck(MDRounding(fn));} \
TEST(MatrixDeterminantReference,prefix##FourByteAlignmentAndReadOnlyMatrixFootprint){MDCheck(MDFootprint(fn));}
MD_TESTS(Public,Vx3DMatrixDeterminant)
#if defined(VX_SIMD_SSE)
static float MDSimd(const VxMatrix &m) { return VxSIMDDeterminant3x3(&m); }
MD_TESTS(Simd,MDSimd)
#endif
#undef MD_TESTS
#endif
