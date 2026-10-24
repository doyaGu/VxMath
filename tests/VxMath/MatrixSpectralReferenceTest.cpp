#include <cmath>
#include <cstring>
#include <cfloat>
#include <new>
#ifndef VX_SPECTRAL_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef double MSMatrix[3][3];
typedef VxVector (*MSSpectralFn)(const VxMatrix &, VxMatrix &);
struct MSResult { int calls=0, errors=0, faults=0; };
static bool MSNear(double a,double b,double scale=1) {
    return std::isfinite(a)&&std::isfinite(b)&&std::fabs(a-b)<=2e-5*scale+4*std::ldexp(1.0,-149);
}
static void MSTo(const MSMatrix &a,VxMatrix &m) {
    m.SetIdentity();for(int r=0;r<3;++r)for(int c=0;c<3;++c)m[r][c]=float(a[r][c]);
}
static void MSEffective(const VxMatrix &m,MSMatrix &a) {
    for(int i=0;i<3;++i)a[i][i]=m[i][i];
    a[0][1]=a[1][0]=m[0][1];a[0][2]=a[2][0]=m[2][0];a[1][2]=a[2][1]=m[1][2];
}
static bool MSAffine(const VxMatrix &m) {
    for(int i=0;i<3;++i)if(m[i][3]!=0||m[3][i]!=0)return false;
    return m[3][3]==1;
}
static bool MSBasis(const VxMatrix &u) {
    for(int r=0;r<3;++r)for(int c=0;c<3;++c) {
        double dot=0;for(int i=0;i<3;++i)dot+=double(u[i][r])*u[i][c];
        if(!MSNear(dot,r==c?1:0))return false;
    }
    const double det=double(u[0][0])*(double(u[1][1])*u[2][2]-double(u[1][2])*u[2][1])
        -double(u[0][1])*(double(u[1][0])*u[2][2]-double(u[1][2])*u[2][0])
        +double(u[0][2])*(double(u[1][0])*u[2][1]-double(u[1][1])*u[2][0]);
    return MSNear(det,1)&&MSAffine(u);
}
static bool MSSpectrum(const MSMatrix &a,const VxMatrix &u,const VxVector &k) {
    double scale=0;for(int r=0;r<3;++r)for(int c=0;c<3;++c)if(std::fabs(a[r][c])>scale)scale=std::fabs(a[r][c]);
    if(!MSBasis(u))return false;
    for(int r=0;r<3;++r)for(int c=0;c<3;++c) {
        double mapped=0,reconstructed=0;
        for(int i=0;i<3;++i){mapped+=a[r][i]*u[i][c];reconstructed+=double(u[r][i])*k[i]*u[c][i];}
        if(!MSNear(mapped,double(u[r][c])*k[c],scale)||!MSNear(reconstructed,a[r][c],scale))return false;
    }
    return true;
}
static bool MSObserve(MSResult &result,MSSpectralFn fn,const MSMatrix &a,int alias,VxMatrix *basis=nullptr,VxVector *values=nullptr) {
    VxMatrix input,u;MSTo(a,input);MSMatrix effective;MSEffective(input,effective);
    unsigned before[16];std::memcpy(before,&input,64);
    VxMatrix &dest=alias?input:u;const VxVector k=fn(input,dest);++result.calls;
    const bool ok=MSSpectrum(effective,dest,k)&&(alias||std::memcmp(before,&input,64)==0);
    if(basis)*basis=dest;if(values)*values=k;result.errors+=!ok;return ok;
}
static MSResult MSDiagonal(MSSpectralFn fn) {
    MSResult result;const float tuples[][3]={{7,-2,3},{2,2,2},{0,0,0},{-0.0f,0,-0.0f},{-3,4,-3}};
    for(const auto &tuple:tuples)for(int alias=0;alias<2;++alias) {
        MSMatrix a={};for(int i=0;i<3;++i)a[i][i]=tuple[i];VxMatrix u;VxVector k;MSObserve(result,fn,a,alias,&u,&k);
        for(int i=0;i<3;++i)result.errors+=std::memcmp(&k[i],&tuple[i],4)!=0;
        for(int r=0;r<4;++r)for(int c=0;c<4;++c)result.errors+=u[r][c]!=(r==c?1:0);
    }
    return result;
}
static MSResult MSBlocks(MSSpectralFn fn,bool hugeDifference) {
    MSResult result;const int powers[]={-149,-145,-126,-100,0,100,127};
    for(int power:powers)for(int plane=0;plane<3;++plane)for(int sign=-1;sign<=1;sign+=2)for(int alias=0;alias<2;++alias) {
        if(hugeDifference&&power!=127)continue;
        const double b=std::ldexp(1.0,power);const int p=(plane+1)%3,q=(p+1)%3;
        MSMatrix a={};a[p][q]=a[q][p]=sign*b;if(hugeDifference){a[p][p]=b;a[q][q]=-b;}
        VxVector k;MSObserve(result,fn,a,alias,nullptr,&k);
        if(hugeDifference){result.errors+=!MSNear(k[p],std::sqrt(2.0)*b,b)||!MSNear(k[q],-std::sqrt(2.0)*b,b);}
        else result.errors+=k[p]!=float(-sign*b)||k[q]!=float(sign*b)||k[plane]!=0;
    }
    return result;
}
static MSResult MSGeneral(MSSpectralFn fn) {
    MSResult result;const int powers[]={-120,0,120};
    for(int n=0;n<40;++n)for(int power:powers)for(int alias=0;alias<2;++alias) {
        MSMatrix a;for(int r=0;r<3;++r)for(int c=r;c<3;++c)a[c][r]=a[r][c]=std::ldexp(double(((n+3)*(r+2)*(c+5)+n*n)%19-9),power);
        MSObserve(result,fn,a,alias);
    }
    return result;
}
static MSResult MSRepeated(MSSpectralFn fn) {
    MSResult result;const int powers[]={-100,0,100},v[3]={1,2,2};
    for(int power:powers)for(int shift=0;shift<3;++shift)for(int sign=-1;sign<=1;sign+=2)for(int beta=-1;beta<=1;beta+=2)for(int alias=0;alias<2;++alias) {
        double w[3];for(int r=0;r<3;++r)w[r]=v[(r+shift)%3]*(r==shift?sign:1);
        MSMatrix a;for(int r=0;r<3;++r)for(int c=0;c<3;++c)a[r][c]=std::ldexp((r==c?2.0:0)+beta*w[r]*w[c],power);
        VxMatrix u;VxVector k;if(!MSObserve(result,fn,a,alias,&u,&k))continue;
        const double scale=std::ldexp(1.0,power);int simple=-1,count=0;
        for(int i=0;i<3;++i)if(MSNear(k[i],(2+9*beta)*scale,scale))simple=i;else if(MSNear(k[i],2*scale,scale))++count;
        bool ok=simple>=0&&count==2;
        if(ok)for(int r=0;r<3;++r)for(int c=0;c<3;++c) {
            double repeated=0;for(int i=0;i<3;++i)if(i!=simple)repeated+=double(u[r][i])*u[c][i];
            const double projector=w[r]*w[c]/9;
            ok=ok&&MSNear(double(u[r][simple])*u[c][simple],projector)&&MSNear(repeated,(r==c?1:0)-projector);
        }
        result.errors+=!ok;
    }
    return result;
}
static MSResult MSNearRepeated(MSSpectralFn fn) {
    MSResult result;const int powers[]={-100,0,100};
    for(int power:powers)for(int plane=0;plane<3;++plane)for(int alias=0;alias<2;++alias) {
        const double base=std::ldexp(2.0,power),gap=std::ldexp(1.0,power-21);const int p=(plane+1)%3,q=(p+1)%3;
        MSMatrix a={};for(int i=0;i<3;++i)a[i][i]=base;a[plane][plane]+=4*gap;a[p][q]=a[q][p]=gap;
        VxVector k;MSObserve(result,fn,a,alias,nullptr,&k);
        result.errors+=k[p]!=float(base-gap)||k[q]!=float(base+gap)||k[plane]!=float(base+4*gap);
    }
    return result;
}
static MSResult MSIgnored(MSSpectralFn fn) {
    MSResult result;const MSMatrix a={{4,1,-1},{1,3,0.5},{-1,0.5,2}};
    const int fields[]={2,3,4,7,9,11,12,13,14,15};const unsigned words[]={0,0x80000000u,0x7f800000u,0xff800000u,0x7fc12345u,0x7f800001u};
    for(int field:fields)for(unsigned word:words) {
        VxMatrix input,u;MSTo(a,input);std::memcpy(reinterpret_cast<unsigned char *>(&input)+4*field,&word,4);
        unsigned before[16];std::memcpy(before,&input,64);const VxVector k=fn(input,u);++result.calls;
        result.errors+=!MSSpectrum(a,u,k)||std::memcmp(before,&input,64)!=0;
    }
    return result;
}
static MSResult MSOverflow(MSSpectralFn fn) {
    MSResult result;for(int plane=0;plane<3;++plane)for(int alias=0;alias<2;++alias) {
        const int p=(plane+1)%3,q=(p+1)%3;MSMatrix a={};a[p][p]=a[q][q]=a[p][q]=a[q][p]=FLT_MAX;
        VxMatrix input,u;MSTo(a,input);VxMatrix &dest=alias?input:u;const VxVector k=fn(input,dest);++result.calls;
        result.errors+=!MSBasis(dest)||k[p]!=0||!std::isinf(k[q])||k[q]<0||k[plane]!=0;
    }
    return result;
}
static MSResult MSAlignment(MSSpectralFn fn) {
    MSResult result;const MSMatrix a={{4,1,-1},{1,3,0.5},{-1,0.5,2}};
    for(int inputOffset=0;inputOffset<4;++inputOffset)for(int outputOffset=0;outputOffset<4;++outputOffset) {
        alignas(16) unsigned char src[96],dst[96];std::memset(src,0xa7,sizeof(src));std::memset(dst,0x9d,sizeof(dst));
        VxMatrix *input=new(src+16+4*inputOffset)VxMatrix,*u=new(dst+16+4*outputOffset)VxMatrix;MSTo(a,*input);
        unsigned char before[96];std::memcpy(before,src,96);const VxVector k=fn(*input,*u);++result.calls;
        bool ok=MSSpectrum(a,*u,k)&&std::memcmp(before,src,96)==0;
        for(int i=0;i<96;++i)if(i<16+4*outputOffset||i>=80+4*outputOffset)ok=ok&&dst[i]==0x9d;
        result.errors+=!ok;input->~VxMatrix();u->~VxMatrix();
    }
    return result;
}
#if defined(_WIN32) && defined(_MSC_VER)
static bool MSProtected(MSSpectralFn fn,const VxMatrix *input,VxMatrix *u,VxVector *k) {
    __try{*k=fn(*input,*u);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
#endif
static MSResult MSFootprint(MSSpectralFn fn) {
    MSResult result;
#if defined(_WIN32) && defined(_MSC_VER)
    SYSTEM_INFO info;GetSystemInfo(&info);const unsigned size=info.dwPageSize;
    unsigned char *memory=static_cast<unsigned char *>(VirtualAlloc(nullptr,6*size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!memory){++result.errors;return result;}DWORD old;const MSMatrix a={{4,1,-1},{1,3,0.5},{-1,0.5,2}};
    for(int layout=0;layout<4;++layout) {
        VxMatrix *input=new(memory+size+(layout&1?size-44:0))VxMatrix,*u=new(memory+4*size+(layout&2?size-64:0))VxMatrix;
        MSTo(a,*input);unsigned before[11];std::memcpy(before,input,44);
        bool ok=true;const int guards[]={0,2,3,5};for(int page:guards)ok=ok&&VirtualProtect(memory+page*size,size,PAGE_NOACCESS,&old);
        ok=ok&&VirtualProtect(memory+size,size,PAGE_READONLY,&old);if(!ok){++result.errors;break;}
        VxVector k;const bool ran=MSProtected(fn,input,u,&k);++result.calls;result.faults+=!ran;VirtualProtect(memory,6*size,PAGE_READWRITE,&old);
        if(ran)result.errors+=!MSSpectrum(a,*u,k)||std::memcmp(before,input,44)!=0;
        input->~VxMatrix();u->~VxMatrix();
    }
    VirtualFree(memory,0,MEM_RELEASE);
#else
    MSMatrix a={{4,1,-1},{1,3,0.5},{-1,0.5,2}};MSObserve(result,fn,a,0);
#endif
    return result;
}
#ifndef VX_SPECTRAL_PROBE
static void MSExpect(MSResult r){EXPECT_GT(r.calls,0);EXPECT_EQ(r.errors,0);EXPECT_EQ(r.faults,0);}
TEST(MatrixSpectralReference, DiagonalSlotsSignedZeroAndWholeInputAlias){MSExpect(MSDiagonal(Vx3DMatrixSpectralDecomposition));}
TEST(MatrixSpectralReference, CyclicPlanesAcrossRepresentableScale){MSExpect(MSBlocks(Vx3DMatrixSpectralDecomposition,false));}
TEST(MatrixSpectralReference, OppositeHugeDiagonalsHaveFiniteEigenvalues){MSExpect(MSBlocks(Vx3DMatrixSpectralDecomposition,true));}
TEST(MatrixSpectralReference, IndefiniteAndSingularSymmetricReconstruction){MSExpect(MSGeneral(Vx3DMatrixSpectralDecomposition));}
TEST(MatrixSpectralReference, DoubleRootUsesSpectralProjectors){MSExpect(MSRepeated(Vx3DMatrixSpectralDecomposition));}
TEST(MatrixSpectralReference, CloseDistinctRootsAreNotMerged){MSExpect(MSNearRepeated(Vx3DMatrixSpectralDecomposition));}
TEST(MatrixSpectralReference, TenUnselectedInputFieldsAreIgnored){MSExpect(MSIgnored(Vx3DMatrixSpectralDecomposition));}
TEST(MatrixSpectralReference, UnrepresentableRootKeepsFiniteProperBasis){MSExpect(MSOverflow(Vx3DMatrixSpectralDecomposition));}
TEST(MatrixSpectralReference, IndependentFourByteAlignmentAndOutputCanaries){MSExpect(MSAlignment(Vx3DMatrixSpectralDecomposition));}
TEST(MatrixSpectralReference, ReadOnly44ByteInputAnd64ByteOutput){MSExpect(MSFootprint(Vx3DMatrixSpectralDecomposition));}
TEST(MatrixSpectralReference, SnugglePreservesRepeatedAndNearbyStretch) {
    for(int distinct=0;distinct<2;++distinct)for(int axis=0;axis<3;++axis) {
        MSMatrix a;const int v[]={1,2,2};for(int r=0;r<3;++r)for(int c=0;c<3;++c)a[r][c]=(r==c?2.0:0)+v[(r+axis)%3]*v[(c+axis)%3];
        if(distinct)a[axis][axis]+=std::ldexp(1.0,-18);
        VxMatrix input,u;MSTo(a,input);VxVector k=Vx3DMatrixSpectralDecomposition(input,u);
        VxQuaternion initial;initial.FromMatrix(u,TRUE,TRUE);VxQuaternion temporary=initial;
        const VxQuaternion correction=Vx3DQuaternionSnuggle(&temporary,&k);
        const VxQuaternion combined=Vx3DQuaternionMultiply(initial,correction);combined.ToMatrix(u);
        EXPECT_TRUE(MSSpectrum(a,u,k));
    }
}
#endif
