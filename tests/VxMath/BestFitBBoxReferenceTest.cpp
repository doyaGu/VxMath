#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include <new>
#include <initializer_list>
#ifndef VX_BEST_FIT_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef decltype(&VxComputeBestFitBBox) BFFn;
struct BFResult { int calls=0,errors=0,faults=0; };
static unsigned BFBits(float value){unsigned bits;memcpy(&bits,&value,4);return bits;}
static float BFFloat(unsigned bits){float value;memcpy(&value,&bits,4);return value;}
static bool BFFinite(double value){return value==value && fabs(value)<=DBL_MAX;}
static void BFMarker(VxMatrix &m){for(int r=0;r<4;++r)for(int c=0;c<4;++c)m[r][c]=float(10+r*4+c);}
static int BFCall(BFFn fn,const unsigned char *points,unsigned stride,int n,VxMatrix &out,float border,bool &fault){
#if defined(_WIN32)
    __try{return fn(points,stride,n,out,border);}__except(EXCEPTION_EXECUTE_HANDLER){fault=true;return -99;}
#else
    (void)fault;return fn(points,stride,n,out,border);
#endif
}
static void BFDecode(const unsigned char *points,unsigned stride,int n,float decoded[17][3]){
    for(int i=0;i<n;++i)memcpy(decoded[i],points+size_t(i)*stride,12);
}
// Geometry and PCA are checked independently of the production eigensolver.
// Pairwise differences define covariance without subtracting raw moments.
static bool BFGeometry(const VxMatrix &m,const float points[17][3],int n,float border,bool pca){
    double low[3],high[3],length[3],unit[3][3],covariance[3][3]={};
    for(int k=0;k<3;++k)low[k]=high[k]=points[0][k];
    for(int i=0;i<n;++i)for(int k=0;k<3;++k){if(points[i][k]<low[k])low[k]=points[i][k];if(points[i][k]>high[k])high[k]=points[i][k];}
    double span=0;for(int k=0;k<3;++k)if(high[k]-low[k]>span)span=high[k]-low[k];
    const double tolerance=span*3.0e-5;
    for(int r=0;r<4;++r)for(int c=0;c<4;++c)if(!BFFinite(m[r][c]))return false;
    if(m[0][3]!=0 || m[1][3]!=0 || m[2][3]!=0 || m[3][3]!=1)return false;
    for(int r=0;r<3;++r){double square=0;for(int k=0;k<3;++k)square+=double(m[r][k])*m[r][k];length[r]=sqrt(square);for(int k=0;k<3;++k)unit[r][k]=length[r]>0 ? m[r][k]/length[r] : 0;}
    for(int a=0;a<3;++a)for(int b=a+1;b<3;++b){double dot=0;for(int k=0;k<3;++k)dot+=unit[a][k]*unit[b][k];if(fabs(dot)>3.0e-5)return false;}
    if(border>=0)for(int i=0;i<n;++i){
        double residual[3];for(int k=0;k<3;++k)residual[k]=double(points[i][k])-m[3][k];
        for(int axis=0;axis<3;++axis)if(length[axis]>span*1.0e-6 && length[axis]>0){
            double dot=0;for(int k=0;k<3;++k)dot+=(double(points[i][k])-m[3][k])*unit[axis][k];
            if(fabs(dot)>length[axis]+tolerance)return false;
            for(int k=0;k<3;++k)residual[k]-=dot*unit[axis][k];
        }
        double error=0;for(int k=0;k<3;++k)error+=residual[k]*residual[k];
        if(sqrt(error)>tolerance && span>0)return false;
        if(span==0)for(int k=0;k<3;++k)if(m[3][k]!=points[0][k])return false;
    }
    if(pca){
        for(int a=0;a<n;++a)for(int b=a+1;b<n;++b)for(int r=0;r<3;++r)for(int c=0;c<3;++c)covariance[r][c]+=(double(points[a][r])-points[b][r])*(double(points[a][c])-points[b][c]);
        double norm=0;for(int r=0;r<3;++r)for(int c=0;c<3;++c)norm+=covariance[r][c]*covariance[r][c];norm=sqrt(norm);
        for(int axis=0;axis<3;++axis)if(length[axis]>span*1.0e-6 && norm>0){
            double product[3]={},lambda=0;for(int r=0;r<3;++r)for(int c=0;c<3;++c)product[r]+=covariance[r][c]*unit[axis][c];
            for(int r=0;r<3;++r)lambda+=product[r]*unit[axis][r];
            double residual=0;for(int r=0;r<3;++r){const double d=product[r]-lambda*unit[axis][r];residual+=d*d;}
            if(sqrt(residual)>norm*3.0e-5)return false;
        }
    }
    return true;
}
static void BFCheck(BFResult &r,BFFn fn,const unsigned char *source,unsigned stride,int n,float border,bool pca=true){
    float decoded[17][3];BFDecode(source,stride,n,decoded);VxMatrix out;BFMarker(out);bool fault=false;
    const int ok=BFCall(fn,source,stride,n,out,border,fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=1||!BFGeometry(out,decoded,n,border,pca);
}
static void BFCuboid(float points[17][3],int rotation,double scale,const double center[3]){
    const double bases[3][3][3]={{{1,0,0},{0,1,0},{0,0,1}},{{0.6,0.8,0},{-0.8,0.6,0},{0,0,1}},{{0.36,0.48,0.8},{-0.8,0.6,0},{-0.48,-0.64,0.6}}};
    const double half[]={3,2,1};
    for(int i=0;i<8;++i)for(int k=0;k<3;++k){double value=center[k];for(int axis=0;axis<3;++axis)value+=(i&(1<<axis)?1:-1)*half[axis]*scale*bases[rotation][axis][k];points[i][k]=float(value);}
}
static BFResult BFAnalytic(BFFn fn){
    BFResult r;const double centers[][3]={{0,0,0},{2,-3,4},{-7,5,11}};const float borders[]={0,0.125f,-0.125f};
    for(int rotation=0;rotation<3;++rotation)for(const auto &center:centers)for(float border:borders){
        float points[17][3];BFCuboid(points,rotation,1,center);VxMatrix out;BFMarker(out);bool fault=false;int ok=BFCall(fn,reinterpret_cast<const unsigned char *>(points),12,8,out,border,fault);++r.calls;r.faults+=fault;
        bool valid=!fault&&ok==1&&BFGeometry(out,points,8,border,true);double lengths[3];
        for(int axis=0;axis<3;++axis){double square=0;for(int k=0;k<3;++k)square+=double(out[axis][k])*out[axis][k];lengths[axis]=sqrt(square);}
        bool used[3]={};for(double target:{3.0+border,2.0+border,1.0+border}){int match=-1;for(int axis=0;axis<3;++axis)if(!used[axis]&&fabs(lengths[axis]-target)<0.0002)match=axis;if(match<0)valid=false;else used[match]=true;}
        for(int k=0;k<3;++k)valid=valid&&fabs(out[3][k]-center[k])<0.0002;r.errors+=!valid;
    }
    return r;
}
static BFResult BFGeneral(BFFn fn){
    BFResult r;unsigned random=321;
    for(int sample=0;sample<64;++sample){float points[17][3];for(int i=0;i<17;++i)for(int k=0;k<3;++k){random=random*1664525u+1013904223u;points[i][k]=float(int((random>>16)%33)-16)*0.25f;}BFCheck(r,fn,reinterpret_cast<const unsigned char *>(points),12,17,0.125f);}
    return r;
}
static BFResult BFDegenerate(BFFn fn){
    BFResult r;const float borders[]={0,0.125f,-0.125f};
    for(int kind=0;kind<4;++kind)for(float border:borders){
        float points[17][3]={};int n=kind==0?1:kind==1?2:kind==2?4:8;
        for(int i=0;i<n;++i){points[i][0]=kind==0||kind==3 ? 3.0f : kind==1 ? (i?5.0f:-3.0f) : (i&1?2.0f:-2.0f);points[i][1]=kind==0||kind==3?4.0f:kind==1?2.0f:(i&2?1.0f:-1.0f);points[i][2]=kind==0||kind==3?5.0f:kind==1?-1.0f:3.0f;}
        BFCheck(r,fn,reinterpret_cast<const unsigned char *>(points),12,n,border);
    }
    return r;
}
static BFResult BFEmpty(BFFn fn){
    BFResult r;const int counts[]={1,0,-1,INT_MIN};
    for(int n:counts)for(unsigned stride:{0u,12u}){VxMatrix out;BFMarker(out);VxMatrix expected=out;bool fault=false;int ok=BFCall(fn,nullptr,stride,n,out,0,fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=0||memcmp(&out,&expected,64)!=0;}
#if defined(_WIN32)
    SYSTEM_INFO info;GetSystemInfo(&info);void *bad=VirtualAlloc(nullptr,info.dwPageSize,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS);if(!bad){++r.errors;return r;}
    for(int n:{0,-1,INT_MIN})for(unsigned stride:{0u,12u}){
        VxMatrix out;BFMarker(out);VxMatrix expected=out;bool fault=false;int ok=BFCall(fn,static_cast<unsigned char *>(bad),stride,n,out,0,fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=0||memcmp(&out,&expected,64)!=0;
        fault=false;ok=BFCall(fn,static_cast<unsigned char *>(bad),stride,n,*static_cast<VxMatrix *>(bad),0,fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=0;
    }
    bool fault=false;int ok=BFCall(fn,nullptr,12,1,*static_cast<VxMatrix *>(bad),0,fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=0;VirtualFree(bad,0,MEM_RELEASE);
#endif
    return r;
}
static BFResult BFLimits(BFFn fn){
    BFResult r;const float values[]={BFFloat(BFBits(1.0e6f)-1),1.0e6f,BFFloat(BFBits(1.0e6f)+1)};
    for(int kind=0;kind<2;++kind)for(int sign:{-1,1})for(float value:values){
        float point[3]={};float border=0;if(kind==0)point[0]=sign*value;else border=sign*value;
        VxMatrix out;BFMarker(out);bool fault=false;int ok=BFCall(fn,reinterpret_cast<unsigned char *>(point),0,1,out,border,fault);++r.calls;r.faults+=fault;
        bool valid=!fault&&ok==(value<=1.0e6f);for(int row=0;row<3;++row)for(int col=0;col<3;++col)valid=valid&&out[row][col]==(row==col?border:0);for(int k=0;k<3;++k)valid=valid&&out[3][k]==point[k];valid=valid&&out[0][3]==0&&out[1][3]==0&&out[2][3]==0&&out[3][3]==1;r.errors+=!valid;
    }
    return r;
}
static void BFBytes(unsigned char *data,unsigned stride,int n){
    for(int i=0;i<n;++i){const float point[]={float(i%5-2),float((i*3)%7-3),float(i%3-1)};memcpy(data+size_t(i)*stride,point,12);}
}
static BFResult BFLayouts(BFFn fn,bool legacy){
    BFResult r;const unsigned strides[]={0,1,4,8,12,16,20,31,64};const int counts[]={1,2,3,8,17};
    for(unsigned stride:strides)for(int n:counts)for(int offset=0;offset<4;++offset){
        if(legacy&&stride<12)continue;unsigned char data[2048]={},saved[2048];BFBytes(data+offset,stride,n);memcpy(saved,data,sizeof(data));BFCheck(r,fn,data+offset,stride,n,0.125f);r.errors+=memcmp(data,saved,sizeof(data))!=0;
    }
    return r;
}
static BFResult BFAliases(BFFn fn){
    BFResult r;
    for(int n:{1,2,4})for(int offset=0;offset<4;++offset){
        VxMatrix out;BFMarker(out);unsigned char *bytes=reinterpret_cast<unsigned char *>(&out);BFBytes(bytes+offset,16,n);float points[17][3];BFDecode(bytes+offset,16,n,points);bool fault=false;int ok=BFCall(fn,bytes+offset,16,n,out,0.125f,fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=1||!BFGeometry(out,points,n,0.125f,true);
    }
    return r;
}
static void BFWideCase(int kind,float points[17][3],int &n){
    n=8;const double zero[]={0,0,0},translated[]={100000,-120000,80000};
    if(kind<6){const double scales[]={1,0.015625,0.0625,0x1p-40,0x1p-100,1024};BFCuboid(points,kind==0?0:2,scales[kind],kind==1||kind==2 ? translated : zero);}
    if(kind==6){n=2;for(int i=0;i<2;++i){points[i][0]=i?BFFloat(BFBits(100000.0f)+1):100000.0f;points[i][1]=points[i][2]=0;}}
    if(kind==7){n=17;for(int i=0;i<n;++i){points[i][0]=100000.0f+float(i%5)*0.015625f;points[i][1]=-120000.0f+float(i%3)*0.03125f;points[i][2]=80000.0f+float(i%7)*0.015625f;}}
}
static BFResult BFWide(BFFn fn){BFResult r;for(int kind=0;kind<8;++kind){float points[17][3];int n;BFWideCase(kind,points,n);BFCheck(r,fn,reinterpret_cast<unsigned char *>(points),12,n,0);}return r;}
static BFResult BFNonfinite(BFFn fn){
    BFResult r;const float values[]={BFFloat(0x7fc12345u),HUGE_VALF,-HUGE_VALF};
    for(int field=0;field<4;++field)for(float value:values){float points[17][3];const double center[]={0,0,0};BFCuboid(points,0,1,center);float border=0;if(field<3)points[3][field]=value;else border=value;VxMatrix out;BFMarker(out);bool fault=false;int ok=BFCall(fn,reinterpret_cast<unsigned char *>(points),12,8,out,border,fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=0;}
    return r;
}
static BFResult BFFootprint(BFFn fn,bool legacy){
    BFResult r;
#if defined(_WIN32)
    SYSTEM_INFO info;GetSystemInfo(&info);const size_t page=info.dwPageSize;unsigned char *memory[2]={};
    for(int i=0;i<2;++i){memory[i]=static_cast<unsigned char *>(VirtualAlloc(nullptr,3*page,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS));if(!memory[i]){++r.errors;if(i)VirtualFree(memory[0],0,MEM_RELEASE);return r;}}
    const unsigned strides[]={0,1,4,8,12,16,20,31,64};const int counts[]={1,2,3,8,17};
    for(unsigned stride:strides)for(int n:counts)for(unsigned edge=0;edge<4;++edge){
        if(legacy&&stride<12)continue;DWORD old;for(int i=0;i<2;++i)r.errors+=!VirtualProtect(memory[i]+page,page,PAGE_READWRITE,&old);
        const size_t span=size_t(n-1)*stride+12;unsigned char *points=memory[0]+page+((edge&1)?page-span:0);memset(points,0,span);BFBytes(points,stride,n);float decoded[17][3];BFDecode(points,stride,n,decoded);VxMatrix *out=::new(memory[1]+page+((edge&2)?page-sizeof(VxMatrix):0))VxMatrix;BFMarker(*out);
        r.errors+=!VirtualProtect(memory[0]+page,page,PAGE_READONLY,&old);bool fault=false;int ok=BFCall(fn,points,stride,n,*out,0.125f,fault);++r.calls;r.faults+=fault;r.errors+=fault||ok!=1||!BFGeometry(*out,decoded,n,0.125f,true);
    }
    for(int i=0;i<2;++i)VirtualFree(memory[i],0,MEM_RELEASE);
#endif
    return r;
}
#ifndef VX_BEST_FIT_PROBE
class BestFitBBoxReference:public testing::TestWithParam<int>{
protected:
    int previous=VX_SIMD_MODE_AUTO;
    void SetUp()override{previous=VxGetSIMDOverride();ASSERT_TRUE(VxSetSIMDOverride(GetParam()));ASSERT_EQ(VxGetSIMDEffectiveBackend(),GetParam());}
    void TearDown()override{EXPECT_TRUE(VxSetSIMDOverride(previous));}
    void Check(BFResult r,int count){EXPECT_EQ(r.calls,count);EXPECT_EQ(r.errors+r.faults,0);}
};
TEST_P(BestFitBBoxReference, LeaveOutputUntouchedForEmptyInput){
#if defined(_WIN32)
    Check(BFEmpty(VxComputeBestFitBBox),21); // Includes 13 PAGE_NOACCESS cases.
#else
    Check(BFEmpty(VxComputeBestFitBBox),8);
#endif
}
TEST_P(BestFitBBoxReference, FitAnalyticOrientedCuboidsAndSignedBorder){Check(BFAnalytic(VxComputeBestFitBBox),27);}
TEST_P(BestFitBBoxReference, MatchIndependentCovarianceEigenResidualAndContainment){Check(BFGeneral(VxComputeBestFitBBox),64);}
TEST_P(BestFitBBoxReference, FitPointLinePlaneAndRepeatedPoints){Check(BFDegenerate(VxComputeBestFitBBox),12);}
TEST_P(BestFitBBoxReference, KeepComputedFailureMatrixAtMagnitudeLimit){Check(BFLimits(VxComputeBestFitBBox),12);}
TEST_P(BestFitBBoxReference, ReadByteStridesWithoutChangingSourceOrPadding){Check(BFLayouts(VxComputeBestFitBBox,false),180);}
TEST_P(BestFitBBoxReference, FinishReadingPointsBeforeAliasedMatrixWrites){Check(BFAliases(VxComputeBestFitBBox),12);}
TEST_P(BestFitBBoxReference, PreservePCAAcrossTranslationScaleAndRoundedCenters){Check(BFWide(VxComputeBestFitBBox),8);}
TEST_P(BestFitBBoxReference, RejectNonfiniteFitResults){Check(BFNonfinite(VxComputeBestFitBBox),12);}
#if defined(_WIN32)
TEST_P(BestFitBBoxReference, RespectReadonlyPointsAndExactMatrixFootprint){Check(BFFootprint(VxComputeBestFitBBox,false),180);}
#endif
INSTANTIATE_TEST_SUITE_P(ScalarAndSSE2,BestFitBBoxReference,testing::Values(VX_SIMD_MODE_NONE,VX_SIMD_MODE_SSE2));
#endif
