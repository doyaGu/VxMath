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
static float EMFloat(unsigned bits) { float value;memcpy(&value,&bits,4);return value; }
static bool EMNear(double a,double b,double scale) {
    return a==a && b==b && fabs(a)<=DBL_MAX && fabs(b)<=DBL_MAX
        && fabs(a-b)<=fabs(scale)*3e-6+8*ldexp(1.0,-149);
}
static void EMSet(EMMatrix &m,const float a[3][3]) {
    memset(&m,0x42,sizeof(m));
    for(int r=0;r<3;++r)for(int c=0;c<3;++c)m[r][c]=a[r][c];
}
static void EMGet(const EMMatrix &m,float a[3][3]) {
    for(int r=0;r<3;++r)for(int c=0;c<3;++c)a[r][c]=m[r][c];
}
static void EMCovCall(EMMatrix &m,const void *points,unsigned stride,unsigned count,bool &fault) {
#if defined(_WIN32)
    __try {m.Covariance(static_cast<const float *>(points),stride,count);}
    __except(EXCEPTION_EXECUTE_HANDLER) {fault=true;}
#else
    (void)fault;m.Covariance(static_cast<const float *>(points),stride,count);
#endif
}
static void EMTriCall(EMMatrix &m,float *d,float *e,bool &fault) {
#if defined(_WIN32)
    __try {m.Tridiagonal(d,e);}__except(EXCEPTION_EXECUTE_HANDLER) {fault=true;}
#else
    (void)fault;m.Tridiagonal(d,e);
#endif
}
static bool EMQLCall(EMMatrix &m,float *d,float *e,bool &fault) {
#if defined(_WIN32)
    __try {return m.QLAlgorithm(d,e)!=0;}__except(EXCEPTION_EXECUTE_HANDLER) {fault=true;return false;}
#else
    (void)fault;return m.QLAlgorithm(d,e)!=0;
#endif
}
static void EMEigenCall(EMMatrix &m,bool &fault) {
#if defined(_WIN32)
    __try {m.EigenStuff3();}__except(EXCEPTION_EXECUTE_HANDLER) {fault=true;}
#else
    (void)fault;m.EigenStuff3();
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
static bool EMBasis(const EMMatrix &q) {
    for(int r=0;r<3;++r)for(int c=0;c<3;++c) {
        double dot=0;for(int i=0;i<3;++i)dot+=double(q[i][r])*q[i][c];
        if(!EMNear(dot,r==c?1:0,1))return false;
    }
    return true;
}
static bool EMReconstruct(const float a[3][3],const EMMatrix &q,const float d[3],const float *e) {
    if(!EMBasis(q))return false;
    double scale=0;for(int r=0;r<3;++r)for(int c=0;c<3;++c)if(fabs(a[r][c])>scale)scale=fabs(a[r][c]);
    for(int r=0;r<3;++r)for(int c=0;c<3;++c) {
        double sum=0;for(int i=0;i<3;++i)sum+=double(q[r][i])*d[i]*q[c][i];
        if(e)for(int i=0;i<2;++i)sum+=e[i]*(double(q[r][i])*q[c][i+1]+double(q[r][i+1])*q[c][i]);
        if(!EMNear(sum,a[r][c],scale))return false;
    }
    return true;
}
static bool EMEigenMatches(const float a[3][3],const EMMatrix &q) {
    if(!EMBasis(q))return false;
    double scale=0,values[3]={};
    for(int r=0;r<3;++r)for(int c=0;c<3;++c)if(fabs(a[r][c])>scale)scale=fabs(a[r][c]);
    for(int i=0;i<3;++i)for(int r=0;r<3;++r)for(int c=0;c<3;++c)values[i]+=double(q[r][i])*a[r][c]*q[c][i];
    for(int r=0;r<3;++r)for(int c=0;c<3;++c) {
        double mapped=0,reconstructed=0;
        for(int k=0;k<3;++k){mapped+=double(a[r][k])*q[k][c];reconstructed+=double(q[r][k])*values[k]*q[c][k];}
        const double divisor=scale>0?scale:1;
        if(!EMNear(mapped/divisor,q[r][c]*values[c]/divisor,1)||!EMNear(reconstructed/divisor,a[r][c]/divisor,1))return false;
    }
    return true;
}
static void EMInput(int index,int power,float a[3][3]) {
    for(int r=0;r<3;++r)for(int c=r;c<3;++c)
        a[r][c]=a[c][r]=float(ldexp(double(((index+3)*(r+2)*(c+5)+index*index)%13-6),power));
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
static void EMTriObserve(EMResult &result,const float input[3][3],bool poisonLower) {
    EMMatrix m;EMSet(m,input);if(poisonLower)for(int r=1;r<3;++r)for(int c=0;c<r;++c)m[r][c]=EMFloat(0x7fc12345);
    EMMatrix before=m;float d[3]={},e[3]={};bool fault=false;EMTriCall(m,d,e,fault);++result.calls;result.faults+=fault;
    result.errors+=fault||!EMReconstruct(input,m,d,e)||e[2]!=0;
}
static EMResult EMTriOrdinary() {
    EMResult result;for(int i=0;i<64;++i){float a[3][3];EMInput(i,0,a);EMTriObserve(result,a,false);}return result;
}
static EMResult EMTriRange() {
    EMResult result;const int powers[]={-140,-100,-24,100,124};
    for(int power:powers)for(int i=0;i<16;++i){float a[3][3];EMInput(i,power,a);EMTriObserve(result,a,false);}return result;
}
static EMResult EMTriBranches() {
    EMResult result;for(int kind=0;kind<5;++kind){float a[3][3]={{1,2,0},{2,4,3},{0,3,5}};if(kind){float edge=kind==1?EMFloat(0x33ffffff):kind==2?EMFloat(0x34000000):kind==3?EMFloat(0x34000001):ldexpf(1,-100);for(int r=0;r<3;++r)for(int c=0;c<3;++c)a[r][c]*=edge;a[0][2]=a[2][0]=edge;}EMTriObserve(result,a,true);}
    return result;
}
static EMResult EMQLDiagonal() {
    EMResult result;const float sets[][3]={{7,-2,3},{2,2,2},{0,0,0},{-3,4,-3}};
    for(const auto &v:sets){const float identity[3][3]={{1,0,0},{0,1,0},{0,0,1}};EMMatrix m;EMSet(m,identity);EMMatrix before=m;float d[3],e[]={0,0,37};memcpy(d,v,12);bool fault=false;bool ok=EMQLCall(m,d,e,fault);++result.calls;result.faults+=fault;result.errors+=fault||!ok||memcmp(&m,&before,sizeof(m))!=0||memcmp(d,v,12)!=0||e[2]!=37;}
    return result;
}
static void EMQLObserve(EMResult &result,int index,int power,bool rotated) {
    const float basis[3][3]={{0.6f,-0.8f,0},{0.8f,0.6f,0},{0,0,1}},identity[3][3]={{1,0,0},{0,1,0},{0,0,1}};
    EMMatrix m;EMSet(m,rotated?basis:identity);EMMatrix before=m;
    float d[]={float(ldexp(index%7-3,power)),float(ldexp((index*3)%11-5,power)),float(ldexp((index*5)%13-6,power))};
    float e[]={float(ldexp(index%5-2,power)),float(ldexp((index*7)%5-2,power)),37};float input[3][3];
    for(int r=0;r<3;++r)for(int c=0;c<3;++c){double sum=0;for(int k=0;k<3;++k)sum+=double(m[r][k])*d[k]*m[c][k];for(int k=0;k<2;++k)sum+=e[k]*(double(m[r][k])*m[c][k+1]+double(m[r][k+1])*m[c][k]);input[r][c]=float(sum);}
    bool fault=false;bool ok=EMQLCall(m,d,e,fault);++result.calls;result.faults+=fault;
    result.errors+=fault||!ok||!EMReconstruct(input,m,d,nullptr);
}
static EMResult EMQLGeneral() {EMResult r;for(int i=0;i<64;++i)for(int rotated=0;rotated<2;++rotated)EMQLObserve(r,i,0,rotated!=0);return r;}
static EMResult EMQLRange() {EMResult r;const int powers[]={-140,-100,100,124};for(int p:powers)for(int i=0;i<32;++i)EMQLObserve(r,i,p,false);return r;}
static EMResult EMQLFailure() {
    EMResult r;for(int k=0;k<2;++k){const float identity[3][3]={{1,0,0},{0,1,0},{0,0,1}};EMMatrix m;EMSet(m,identity);float d[]={1,2,3},e[]={1,2,0};if(k)d[0]=EMFloat(0x7fc12345);else e[0]=EMFloat(0x7fc12345);bool fault=false;bool ok=EMQLCall(m,d,e,fault);++r.calls;r.faults+=fault;r.errors+=fault||ok;}
    return r;
}
static void EMEigenObserve(EMResult &result,const float input[3][3]) {
    EMMatrix m;EMSet(m,input);EMMatrix before=m;bool fault=false;EMEigenCall(m,fault);++result.calls;result.faults+=fault;result.errors+=fault||!EMEigenMatches(input,m);
}
static EMResult EMEigenGeneral() {
    EMResult r;for(int i=0;i<64;++i){float a[3][3];EMInput(i,0,a);EMEigenObserve(r,a);}
    for(int kind=0;kind<4;++kind){float a[3][3]={};for(int i=0;i<3;++i)for(int j=0;j<3;++j)a[i][j]=kind==0?0:kind==1?(i==j?2.0f:0):kind==2?float((i+1)*(j+1)):float((i==j?2:0)-(i+1)*(j+1));EMEigenObserve(r,a);}return r;
}
static EMResult EMEigenRange() {EMResult r;const int powers[]={-140,-100,-24,100,124};for(int p:powers)for(int i=0;i<32;++i){float a[3][3];EMInput(i,p,a);EMEigenObserve(r,a);}return r;}
static EMResult EMEigenExtremes() {
    EMResult r;const int powers[]={-149,-145,126,127};
    for(int p:powers)for(int kind=0;kind<3;++kind){float a[3][3];for(int i=0;i<3;++i)for(int j=0;j<3;++j)a[i][j]=float(ldexp(kind==0?1:kind==1?(i==j?-1:1):(i==j?1:0),p));EMEigenObserve(r,a);}return r;
}
static EMResult EMPipeline() {
    EMResult r;for(int translated=0;translated<2;++translated)for(int p=-20;p<=20;p+=20){float points[17][3];for(int i=0;i<17;++i)for(int k=0;k<3;++k)points[i][k]=float(ldexp((i*(k+3))%17-8,p)+(translated?ldexp(k==1?-1:1,p+20):0));double covariance[3][3];EMCovModel(points,12,17,covariance);float input[3][3];for(int i=0;i<3;++i)for(int j=0;j<3;++j)input[i][j]=float(covariance[i][j]);EMMatrix m;bool fault=false;EMCovCall(m,points,12,17,fault);EMEigenCall(m,fault);++r.calls;r.faults+=fault;r.errors+=fault||!EMEigenMatches(input,m);}return r;
}
static EMResult EMFootprint() {
    EMResult r;
#if defined(_WIN32)
    SYSTEM_INFO info;GetSystemInfo(&info);const size_t page=info.dwPageSize;unsigned char *memory[4]={};
    for(int i=0;i<4;++i){memory[i]=static_cast<unsigned char *>(VirtualAlloc(nullptr,3*page,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS));if(!memory[i]){++r.errors;for(int j=0;j<i;++j)VirtualFree(memory[j],0,MEM_RELEASE);return r;}}
    for(int op=0;op<4;++op)for(int edge=0;edge<16;++edge){DWORD old;for(int i=0;i<4;++i)r.errors+=!VirtualProtect(memory[i]+page,page,PAGE_READWRITE,&old);
        EMMatrix *m=::new(memory[0]+page+((edge&1)?page-sizeof(EMMatrix):0))EMMatrix;float a[3][3];EMInput(3,0,a);EMSet(*m,a);
        float *d=reinterpret_cast<float *>(memory[1]+page+((edge&2)?page-12:0)),*e=reinterpret_cast<float *>(memory[2]+page+((edge&4)?page-12:0));
        unsigned char *points=memory[3]+page+((edge&8)?page-42:0);for(int i=0;i<3;++i){const float p[]={float(i+1),float(i-2),float(i*3)};memcpy(points+size_t(i)*15,p,12);} // last XYZ ends exactly at the protected-page boundary
        r.errors+=!VirtualProtect(memory[3]+page,page,PAGE_READONLY,&old);bool fault=false,ok=true;
        if(op==0){double expected[3][3];EMCovModel(points,15,3,expected);EMCovCall(*m,points,15,3,fault);ok=EMCovMatches(*m,expected);}
        else if(op==1){EMTriCall(*m,d,e,fault);ok=EMReconstruct(a,*m,d,e);}
        else if(op==2){const float identity[3][3]={{1,0,0},{0,1,0},{0,0,1}};EMSet(*m,identity);for(int i=0;i<3;++i){d[i]=float(i+1);e[i]=i<2?1.0f:0;}const float tridiag[3][3]={{1,1,0},{1,2,1},{0,1,3}};ok=EMQLCall(*m,d,e,fault)&&EMReconstruct(tridiag,*m,d,nullptr);}
        else {EMEigenCall(*m,fault);ok=EMEigenMatches(a,*m);}
        ++r.calls;r.faults+=fault;r.errors+=fault||!ok;
    }
    for(int i=0;i<4;++i)VirtualFree(memory[i],0,MEM_RELEASE);
#endif
    return r;
}
#ifndef VX_EIGEN_MATRIX_PROBE
static void EMCheck(EMResult r,int calls){EXPECT_EQ(r.calls,calls);EXPECT_EQ(r.errors,0);EXPECT_EQ(r.faults,0);}
TEST(EigenMatrixReference,CovarianceUsesPopulationNormalization){EMCheck(EMCovOrdinary(),20);}
TEST(EigenMatrixReference,CovarianceSupportsByteStridesAndAliasedSource){EMCheck(EMCovLayouts(),148);}
TEST(EigenMatrixReference,CovariancePreservesTranslatedSmallVariances){EMCheck(EMCovRange(),13);}
TEST(EigenMatrixReference,ZeroCountProducesUndefinedCovarianceWithoutReadingPoints){EMCheck(EMZeroCount(),1);}
TEST(EigenMatrixReference,TridiagonalReconstructsSymmetricInput){EMCheck(EMTriOrdinary(),64);}
TEST(EigenMatrixReference,TridiagonalPreservesTinyAndLargeOffDiagonalTerms){EMCheck(EMTriRange(),80);}
TEST(EigenMatrixReference,TridiagonalUsesUpperTriangleAndBothHouseholderBranches){EMCheck(EMTriBranches(),5);}
TEST(EigenMatrixReference,QLLeavesDiagonalOrderAndUnusedSentinelAlone){EMCheck(EMQLDiagonal(),4);}
TEST(EigenMatrixReference,QLAccumulatesRotationsIntoSuppliedBasis){EMCheck(EMQLGeneral(),128);}
TEST(EigenMatrixReference,QLAvoidsFloatShiftAndRotationOverflow){EMCheck(EMQLRange(),128);}
TEST(EigenMatrixReference,QLReportsNonconvergenceForUnorderedInputs){EMCheck(EMQLFailure(),2);}
TEST(EigenMatrixReference,EigenStuffProducesOrthonormalEigenvectorsForDegenerateSpectra){EMCheck(EMEigenGeneral(),68);}
TEST(EigenMatrixReference,EigenStuffPreservesSpectrumAcrossScales){EMCheck(EMEigenRange(),160);}
TEST(EigenMatrixReference,EigenStuffPreservesDirectionsAtFiniteFloatLimits){EMCheck(EMEigenExtremes(),12);}
TEST(EigenMatrixReference,CovarianceEigenPipelinePreservesTranslatedPrincipalAxes){EMCheck(EMPipeline(),6);}
#if defined(_WIN32)
TEST(EigenMatrixReference,RespectReadonlyPointsAndExactObjectAndArrayFootprints){EMCheck(EMFootprint(),64);}
#endif
#endif
