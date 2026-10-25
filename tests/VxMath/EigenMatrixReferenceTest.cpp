#include <float.h>
#include <math.h>
#include <string.h>
#include <new>
#ifndef VX_EIGEN_MATRIX_PROBE
#include "VxMath.h"
#include "VxEigenMatrix.h"
#include <gtest/gtest.h>
#define VX_EIGEN_MATRIX_CLASS VxEigenMatrix
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef VX_EIGEN_MATRIX_CLASS EMMatrix;
struct EMResult { int calls=0,errors=0,faults=0; };

static bool EMNear(double a,double b,double scale) {
    return a==a && b==b && fabs(a)<=DBL_MAX && fabs(b)<=DBL_MAX
        && fabs(a-b)<=fabs(scale)*3e-6+8*ldexp(1.0,-149);
}
static void EMSet(EMMatrix &m,const float a[3][3]) {
    memset(&m,0x42,sizeof(m));
    for(int r=0;r<3;++r)for(int c=0;c<3;++c)m[r][c]=a[r][c];
}

static void EMCovCall(EMMatrix &m,const void *points,unsigned stride,unsigned count,bool &fault) {
#if defined(_WIN32)
    __try {m.Covariance(static_cast<const float *>(points),stride,count);}
    __except(EXCEPTION_EXECUTE_HANDLER) {fault=true;}
#else
    (void)fault;m.Covariance(static_cast<const float *>(points),stride,count);
#endif
}

static void EMCovModel(const void *data,unsigned stride,int count,double out[3][3]) {
    float points[17][3];const unsigned char *bytes=static_cast<const unsigned char *>(data);
    for(int i=0;i<count;++i)memcpy(points[i],bytes+size_t(i)*stride,12);
    // Population covariance = sum of pair differences / N^2. This does not
    // repeat production mean/scatter or raw-moment accumulation.
    for(int r=0;r<3;++r)for(int c=0;c<3;++c) {
        double sum=0;for(int a=0;a<count;++a)for(int b=a+1;b<count;++b)
            sum+=(double(points[a][r])-points[b][r])*(double(points[a][c])-points[b][c]);
        out[r][c]=sum/(double(count)*count);
    }
}
static bool EMCovMatches(const EMMatrix &m,const double expected[3][3]) {
    double scale=0;for(int r=0;r<3;++r)for(int c=0;c<3;++c)if(fabs(expected[r][c])>scale)scale=fabs(expected[r][c]);
    for(int r=0;r<3;++r)for(int c=0;c<3;++c)
        if(!EMNear(m[r][c],expected[r][c],scale)||m[r][c]!=m[c][r])return false;
    return true;
}
static void EMCovObserve(EMResult &result,const void *data,unsigned stride,int count) {
    double expected[3][3];EMCovModel(data,stride,count,expected);
    EMMatrix m;const float marker[3][3]={{11,12,13},{14,15,16},{17,18,19}};EMSet(m,marker);EMMatrix before=m;bool fault=false;
    EMCovCall(m,data,stride,count,fault);++result.calls;result.faults+=fault;
    result.errors+=fault||!EMCovMatches(m,expected);
}

static EMResult EMCovOrdinary() {
    EMResult result;const int counts[]={1,2,3,8,17};
    for(int count:counts)for(int offset=0;offset<4;++offset){float p[17][3];for(int i=0;i<count;++i)for(int k=0;k<3;++k)p[i][k]=float((i*(k*2+3))%11-5)*0.25f+offset;EMCovObserve(result,p,12,count);}
    return result;
}
static EMResult EMCovLayouts() {
    EMResult result;const unsigned strides[]={0,1,4,8,12,16,31};const int counts[]={1,2,3,8,17};
    for(unsigned stride:strides)for(int count:counts)for(int offset=0;offset<4;++offset){unsigned char data[1024]={},saved[1024];for(int i=0;i<count;++i){float p[]={float(i%5-2),float(i%3-1),float(i%7-3)};memcpy(data+offset+size_t(i)*stride,p,12);}memcpy(saved,data,sizeof(data));EMCovObserve(result,data+offset,stride,count);result.errors+=memcmp(data,saved,sizeof(data))!=0;}
    for(int count=1;count<=2;++count)for(int offset=0;offset<4;++offset){EMMatrix m;memset(&m,0,sizeof(m));unsigned char *p=reinterpret_cast<unsigned char *>(&m)+offset;for(int i=0;i<count;++i){float v[]={float(i+1),float(3-i),float(i-2)};memcpy(p+12*i,v,12);}double expected[3][3];EMCovModel(p,12,count,expected);bool fault=false;EMCovCall(m,p,12,count,fault);++result.calls;result.faults+=fault;result.errors+=fault||!EMCovMatches(m,expected);}
    return result;
}
static EMResult EMCovRange() {
    EMResult result;const int powers[]={-50,-20,0,40,60};
    for(int power:powers)for(int translated=0;translated<2;++translated){float p[17][3];for(int i=0;i<17;++i)for(int k=0;k<3;++k)p[i][k]=float(ldexp((i*(k+3))%17-8,power)+(translated?ldexp(k==1?-1.0:1.0,power+20):0));EMCovObserve(result,p,12,17);}
    for(int k=0;k<3;++k){float p[17][3];for(int i=0;i<17;++i)for(int c=0;c<3;++c)p[i][c]=float(ldexp(c==k?-1.0:1.0,64)+ldexp(double((i*(c+3))%17-8),42));EMCovObserve(result,p,12,17);}
    return result;
}
static EMResult EMZeroCount() {
    EMResult result;EMMatrix m;bool fault=false;EMCovCall(m,nullptr,0,0,fault);++result.calls;result.faults+=fault;
    for(int r=0;r<3;++r)for(int c=0;c<3;++c)result.errors+=m[r][c]==m[r][c];
    return result;
}

#ifndef VX_EIGEN_MATRIX_PROBE
static void EMCheck(EMResult r,int calls){EXPECT_EQ(r.calls,calls);EXPECT_EQ(r.errors,0);EXPECT_EQ(r.faults,0);}
TEST(EigenMatrixReference,CovarianceUsesPopulationNormalization){EMCheck(EMCovOrdinary(),20);}
TEST(EigenMatrixReference,CovarianceSupportsByteStridesAndAliasedSource){EMCheck(EMCovLayouts(),148);}
TEST(EigenMatrixReference,CovariancePreservesTranslatedSmallVariances){EMCheck(EMCovRange(),13);}
TEST(EigenMatrixReference,ZeroCountProducesUndefinedCovarianceWithoutReadingPoints){EMCheck(EMZeroCount(),1);}
#if defined(_WIN32)
#endif
#endif
