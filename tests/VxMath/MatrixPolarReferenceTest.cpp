#include <cmath>
#include <cstring>
#include <cfloat>
#include <new>
#ifndef VX_POLAR_PROBE
#ifndef VX_POLAR_HEADER_READY
#include "VxMath.h"
#endif
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef double MPMatrix[3][3];
typedef double MPVector[3];
typedef float (*MPPolarFn)(const VxMatrix &, VxMatrix &, VxMatrix &);
typedef double (*MPDotFn)(const double *, const double *);
typedef int (*MPMaxFn)(const double (&)[3][3]);
typedef void (*MPReflectorFn)(double (&)[3]);
typedef void (*MPReflectFn)(double (&)[3][3], const double (&)[3]);
typedef void (*MPRank1Fn)(double (&)[3][3], double (&)[3][3]);
typedef void (*MPRank2Fn)(double (&)[3][3], const double (&)[3][3], double (&)[3][3]);
struct MPResult { int calls=0, errors=0, faults=0; };
static void MPCopy(const MPMatrix &a, MPMatrix &b) { std::memcpy(b,a,sizeof(b)); }
static void MPFrom(const VxMatrix &m, MPMatrix &a) { for(int r=0;r<3;++r)for(int c=0;c<3;++c)a[r][c]=m[r][c]; }
static void MPTo(const MPMatrix &a, VxMatrix &m) { for(int r=0;r<3;++r)for(int c=0;c<3;++c)m[r][c]=float(a[r][c]); }
static double MPAbsMax(const MPMatrix &a) { double x=0;for(int r=0;r<3;++r)for(int c=0;c<3;++c)if(std::fabs(a[r][c])>x)x=std::fabs(a[r][c]);return x; }
static bool MPNear(double a,double b,double scale=1) { return std::isfinite(a)&&std::isfinite(b)&&std::fabs(a-b)<=3e-5*scale+8*std::ldexp(1.0,-149); }
static bool MPFactors(const MPMatrix &a,const MPMatrix &q,const MPMatrix &s) {
    const double scale=MPAbsMax(a), ss=MPAbsMax(s), tolerance=6e-5+32*std::ldexp(1.0,-149)/(ss?ss:1);
    for(int r=0;r<3;++r)for(int c=0;c<3;++c) {
        double dot=0,mapped=0;
        for(int k=0;k<3;++k) { dot+=q[k][r]*q[k][c];mapped+=q[r][k]*s[k][c]; }
        if(!MPNear(dot,r==c?1:0)||!MPNear(mapped,a[r][c],scale)||!MPNear(s[r][c],s[c][r],ss))return false;
    }
    // A symmetric3x3 is PSD iff all principal minors are nonnegative.
    if(ss) {
        MPMatrix n;for(int r=0;r<3;++r)for(int c=0;c<3;++c)n[r][c]=s[r][c]/ss;
        for(int i=0;i<3;++i) { if(n[i][i]<-tolerance)return false;for(int j=i+1;j<3;++j)if(n[i][i]*n[j][j]-n[i][j]*n[j][i]<-tolerance)return false; }
        const double d=n[0][0]*(n[1][1]*n[2][2]-n[1][2]*n[2][1])-n[0][1]*(n[1][0]*n[2][2]-n[1][2]*n[2][0])+n[0][2]*(n[1][0]*n[2][1]-n[1][1]*n[2][0]);
        if(d<-tolerance)return false;
    }
    return true;
}
static bool MPOrthogonalFactor(const MPMatrix &a,const MPMatrix &q) {
    MPMatrix s={};for(int r=0;r<3;++r)for(int c=0;c<3;++c)for(int k=0;k<3;++k)s[r][c]+=q[k][r]*a[k][c];
    const double det=q[0][0]*(q[1][1]*q[2][2]-q[1][2]*q[2][1])-q[0][1]*(q[1][0]*q[2][2]-q[1][2]*q[2][0])+q[0][2]*(q[1][0]*q[2][1]-q[1][1]*q[2][0]);
    return MPFactors(a,q,s)&&MPNear(det,1);
}
static void MPCofactor(const MPMatrix &a,MPMatrix &c) {
    for(int r=0;r<3;++r)for(int j=0;j<3;++j) {
        int rows[2],cols[2],nr=0,nc=0;for(int i=0;i<3;++i){if(i!=r)rows[nr++]=i;if(i!=j)cols[nc++]=i;}
        c[r][j]=(a[rows[0]][cols[0]]*a[rows[1]][cols[1]]-a[rows[0]][cols[1]]*a[rows[1]][cols[0]])*((r+j)&1?-1:1);
    }
}
static MPResult MPDot(MPDotFn fn) {
    MPResult result;const int powers[]={20,30,40};
    for(int e:powers)for(int shift=0;shift<3;++shift)for(int sign=-1;sign<=1;sign+=2) {
        double a[3],b[3]={1,1,1};a[shift]=std::ldexp(1.0,e);a[(shift+1)%3]=sign;a[(shift+2)%3]=-a[shift];
        const double value=fn(a,b);++result.calls;result.errors+=value!=sign;
    }
    return result;
}
static MPResult MPMax(MPMaxFn fn) {
    MPResult result;MPMatrix a={};++result.calls;result.errors+=fn(a)!=-1;
    for(int r=0;r<3;++r)for(int c=0;c<3;++c) { MPMatrix m={};m[r][c]=-7;++result.calls;result.errors+=fn(m)!=c; }
    a[0][2]=-7;a[2][0]=7;++result.calls;result.errors+=fn(a)!=2;return result;
}
static MPResult MPReflector(MPReflectorFn fn,bool extreme) {
    const int exponents[]={-140,-100,0,100,126};MPResult result;
    for(int e:exponents)for(int axis=0;axis<3;++axis)for(int sign=-1;sign<=1;sign+=2) {
        if(!extreme&&e!=0)continue;
        double v[3]={std::ldexp(1.0,e),std::ldexp(2.0,e),std::ldexp(3.0,e)};v[axis]*=sign;
        double before[3];std::memcpy(before,v,sizeof(v));const double norm=std::hypot(v[0],v[1],v[2]);fn(v);++result.calls;
        const double square=v[0]*v[0]+v[1]*v[1]+v[2]*v[2];bool ok=MPNear(square,2);
        double dot=0;for(int i=0;i<3;++i)dot+=v[i]*(before[i]/norm);
        for(int i=0;i<3;++i)ok=ok&&MPNear(before[i]/norm-v[i]*dot,i==2?(before[2]<0?1:-1):0);
        result.errors+=!ok;
    }
    if(extreme){double zero[3]={};fn(zero);++result.calls;result.errors+=zero[0]!=0||zero[1]!=0||zero[2]!=0;}
    return result;
}
static MPResult MPReflect(MPReflectFn fn,bool rows) {
    MPResult result;const double vectors[][3]={{1,1,0},{0,1,1},{1,0,1},{2,-1,0.5},{0,0,0}};
    for(const auto &u:vectors)for(int exponent=-100;exponent<=100;exponent+=100) {
        MPMatrix m,expected={};for(int r=0;r<3;++r)for(int c=0;c<3;++c)m[r][c]=std::ldexp(double(1+3*r+c),exponent);
        for(int r=0;r<3;++r)for(int c=0;c<3;++c)for(int k=0;k<3;++k) {
            const double h=(rows?(k==c):(r==k))?1:0;
            expected[r][c]+=rows?m[r][k]*(h-u[k]*u[c]):(h-u[r]*u[k])*m[k][c];
        }
        const double scale=MPAbsMax(expected);double v[3]={u[0],u[1],u[2]};fn(m,v);++result.calls;bool ok=true;
        for(int r=0;r<3;++r)for(int c=0;c<3;++c)ok=ok&&MPNear(m[r][c],expected[r][c],scale);
        ok=ok&&std::memcmp(v,u,sizeof(v))==0;result.errors+=!ok;
    }
    MPMatrix m={};double u[3]={1,1,1};for(int i=0;i<3;++i) { if(rows)m[0][i]=i==1?1:std::ldexp(i==0?1.0:-1.0,40);else m[i][0]=i==1?1:std::ldexp(i==0?1.0:-1.0,40); }
    fn(m,u);++result.calls;result.errors+=(rows?m[0][1]:m[1][0])!=0;return result;
}
static MPResult MPRank1(MPRank1Fn fn,bool extreme) {
    MPResult result;const int exponents[]={-100,0,100};
    for(int e:exponents)for(int mode=0;mode<2;++mode) {
        if(!extreme&&e!=0)continue;
        MPMatrix a,m,q;const int u[]={1,2,-3},v[]={2,-1,3};
        for(int r=0;r<3;++r)for(int c=0;c<3;++c)a[r][c]=std::ldexp(double(u[r]*v[c]),e);
        MPCopy(a,m);fn(m,mode?m:q);++result.calls;result.errors+=!MPOrthogonalFactor(a,mode?m:q);
    }
    MPMatrix zero={},q;fn(zero,q);++result.calls;result.errors+=!MPOrthogonalFactor(zero,q);return result;
}
static MPResult MPRank2(MPRank2Fn fn,bool extreme) {
    MPResult result;const int exponents[]={-100,0,100};
    for(int e:exponents)for(int shift=0;shift<3;++shift)for(int mode=0;mode<3;++mode) {
        if(!extreme&&e!=0)continue;
        MPMatrix a={},m,c,q;const int values[3][2]={{1,2},{0,1},{1,3}};
        for(int r=0;r<3;++r)for(int j=0;j<2;++j)a[r][(j+shift)%3]=std::ldexp(double(values[r][j]),e);
        MPCopy(a,m);MPCofactor(a,c);MPMatrix *dest=mode==1?&m:mode==2?&c:&q;fn(m,c,*dest);++result.calls;
        result.errors+=!MPOrthogonalFactor(a,*dest);
    }
    for(int sign=-1;sign<=1;sign+=2)for(int mode=0;mode<3;++mode){MPMatrix a={{2,0,0},{0,double(3*sign),0},{0,0,0}},m,c,q;MPCopy(a,m);MPCofactor(a,c);MPMatrix *dest=mode==1?&m:mode==2?&c:&q;fn(m,c,*dest);++result.calls;result.errors+=!MPOrthogonalFactor(a,*dest);}
    MPMatrix a={},q,c={};fn(a,c,q);++result.calls;result.errors+=!MPOrthogonalFactor(a,q);return result;
}
static bool MPAffine(const VxMatrix &m) { for(int i=0;i<3;++i)if(m[i][3]!=0||m[3][i]!=0)return false;return m[3][3]==1; }
static void MPObserve(MPResult &result,MPPolarFn fn,const MPMatrix &a,int rank,int sign,int alias,const MPMatrix *knownQ=nullptr,const MPMatrix *knownS=nullptr) {
    VxMatrix input,q,s;input.SetIdentity();MPTo(a,input);unsigned before[16];std::memcpy(before,&input,64);
    VxMatrix &qo=alias==1?input:q,&so=alias==2?input:s;const float det=fn(input,qo,so);++result.calls;
    MPMatrix qm,sm;MPFrom(qo,qm);MPFrom(so,sm);bool ok=MPFactors(a,qm,sm)&&MPAffine(qo)&&MPAffine(so);
    ok=ok&&(rank<3?det==0:MPNear(det,sign));
    if(!alias)ok=ok&&std::memcmp(before,&input,64)==0;
    if(knownQ)for(int r=0;r<3;++r)for(int c=0;c<3;++c)ok=ok&&MPNear(qm[r][c],(*knownQ)[r][c]);
    if(knownS)for(int r=0;r<3;++r)for(int c=0;c<3;++c)ok=ok&&MPNear(sm[r][c],(*knownS)[r][c],std::fabs((*knownS)[r][c]));
    result.errors+=!ok;
}
static MPResult MPDiagonal(MPPolarFn fn,bool extreme) {
    MPResult result;const int exponents[]={-100,0,100};
    for(int x:exponents)for(int y:exponents)for(int z:exponents)for(int signs=0;signs<8;++signs)for(int alias=0;alias<3;++alias) {
        if(!extreme&&(x||y||z||alias))continue;
        MPMatrix a={},q={},s={};const int powers[]={x,y,z};int sign=1;
        for(int i=0;i<3;++i){q[i][i]=signs&(1<<i)?-1:1;sign*=int(q[i][i]);s[i][i]=std::ldexp(1.0,powers[i]);a[i][i]=q[i][i]*s[i][i];}
        MPObserve(result,fn,a,3,sign,alias,&q,&s);
    }
    if(extreme)for(int alias=0;alias<3;++alias){MPMatrix a={},q={},s={};a[0][0]=std::ldexp(1.0,-149);a[1][1]=std::ldexp(1.0,127);a[2][2]=-1;q[0][0]=q[1][1]=1;q[2][2]=-1;s[0][0]=a[0][0];s[1][1]=a[1][1];s[2][2]=1;MPObserve(result,fn,a,3,-1,alias,&q,&s);}
    return result;
}
static MPResult MPKnown(MPPolarFn fn,bool extreme) {
    MPResult result;const int exponents[]={-120,-60,0,60,120};const double base[3][3]={{4,1,-1},{1,3,0.5},{-1,0.5,2}};
    for(int e:exponents)for(int shift=0;shift<3;++shift)for(int sign=-1;sign<=1;sign+=2)for(int alias=0;alias<3;++alias) {
        if(!extreme&&(e||alias))continue;
        MPMatrix a={},q={},s={};for(int r=0;r<3;++r)for(int c=0;c<3;++c)s[r][c]=std::ldexp(base[r][c],e);
        for(int r=0;r<3;++r){q[r][(r+shift)%3]=r==0?sign:1;for(int c=0;c<3;++c)a[r][c]=q[r][(r+shift)%3]*s[(r+shift)%3][c];}
        MPObserve(result,fn,a,3,sign,alias,&q,&s);
    }
    return result;
}
static MPResult MPSingular(MPPolarFn fn,int rank,bool extreme) {
    MPResult result;const int exponents[]={-100,0,100};
    for(int e:exponents)for(int shift=0;shift<3;++shift)for(int sign=-1;sign<=1;sign+=2)for(int alias=0;alias<3;++alias) {
        if(!extreme&&(e||alias))continue;
        MPMatrix a={};const int u[]={1,2,-3},v[]={2,-1,3},two[3][2]={{1,2},{0,1},{1,3}};
        for(int r=0;r<3;++r)for(int c=0;c<3;++c)a[r][(c+shift)%3]=std::ldexp(double(rank==1?u[r]*v[c]:c<2?two[r][c]:0)*(r==0?sign:1),e);
        MPObserve(result,fn,a,rank,0,alias);
    }
    MPMatrix zero={};for(int alias=0;alias<(extreme?3:1);++alias)MPObserve(result,fn,zero,0,0,alias);return result;
}
static MPResult MPCondition(MPPolarFn fn) {
    MPResult result;const int powers[]={10,20,23};
    for(int e:powers)for(int alias=0;alias<3;++alias) {
        const double n=std::ldexp(1.0,e),d=std::hypot(2*n,2.0);MPMatrix a={{n,n-1,0},{n+1,n,0},{0,0,1}},q={{2*n/d,-2/d,0},{2/d,2*n/d,0},{0,0,1}};
        MPObserve(result,fn,a,3,1,alias,&q);
    }
    for(int alias=0;alias<3;++alias){const double h=std::ldexp(1.0,100),t=std::ldexp(1.0,-100);MPMatrix a={{h,t,0},{0,h,t},{t,0,0}};MPObserve(result,fn,a,3,1,alias);}
    return result;
}
static MPResult MPIgnored(MPPolarFn fn) {
    MPResult result;MPMatrix a={{4,1,-1},{1,3,0.5},{-1,0.5,2}};const unsigned words[]={0,0x80000000u,0x7f800000u,0xff800000u,0x7fc12345u,0x7f800001u};const int fields[]={3,7,11,12,13,14,15};
    for(unsigned word:words)for(int field:fields){VxMatrix input,q,s;input.SetIdentity();MPTo(a,input);std::memcpy(reinterpret_cast<unsigned char *>(&input)+4*field,&word,4);unsigned before[16];std::memcpy(before,&input,64);const float d=fn(input,q,s);++result.calls;MPMatrix qm,sm;MPFrom(q,qm);MPFrom(s,sm);result.errors+=!MPFactors(a,qm,sm)||!MPNear(d,1)||!MPAffine(q)||!MPAffine(s)||std::memcmp(before,&input,64)!=0;}
    return result;
}
#if defined(_WIN32) && defined(_MSC_VER)
static bool MPProtected(MPPolarFn fn,const VxMatrix *m,VxMatrix *q,VxMatrix *s,float *d) { __try{*d=fn(*m,*q,*s);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;} }
#endif
static MPResult MPFootprint(MPPolarFn fn) {
    MPResult result;
#if defined(_WIN32) && defined(_MSC_VER)
    SYSTEM_INFO info;GetSystemInfo(&info);const unsigned size=info.dwPageSize;unsigned char *memory=static_cast<unsigned char *>(VirtualAlloc(nullptr,9*size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));if(!memory){++result.errors;return result;}DWORD old;
    MPMatrix a={{4,1,-1},{1,3,0.5},{-1,0.5,2}};
    for(int layout=0;layout<8;++layout){VxMatrix *m=new(memory+size+(layout&1?size-44:0))VxMatrix,*q=new(memory+4*size+(layout&2?size-64:0))VxMatrix,*s=new(memory+7*size+(layout&4?size-64:0))VxMatrix;m->SetIdentity();MPTo(a,*m);unsigned before[16];std::memcpy(before,m,64);bool ok=true;const unsigned guards[]={0,2,3,5,6,8};for(unsigned page:guards)ok=ok&&VirtualProtect(memory+page*size,size,PAGE_NOACCESS,&old);ok=ok&&VirtualProtect(memory+size,size,PAGE_READONLY,&old);if(!ok){++result.errors;break;}float d=0;const bool ran=MPProtected(fn,m,q,s,&d);++result.calls;result.faults+=!ran;VirtualProtect(memory,9*size,PAGE_READWRITE,&old);if(ran){MPMatrix qm,sm;MPFrom(*q,qm);MPFrom(*s,sm);result.errors+=!MPFactors(a,qm,sm)||!MPNear(d,1)||!MPAffine(*q)||!MPAffine(*s)||std::memcmp(before,m,64)!=0;}m->~VxMatrix();q->~VxMatrix();s->~VxMatrix();}VirtualFree(memory,0,MEM_RELEASE);
#else
    MPMatrix a={{4,1,-1},{1,3,0.5},{-1,0.5,2}};MPObserve(result,fn,a,3,1,0);
#endif
    return result;
}
#ifndef VX_POLAR_PROBE
static void MPExpect(MPResult r){EXPECT_GT(r.calls,0);EXPECT_EQ(r.errors,0);EXPECT_EQ(r.faults,0);}
TEST(MatrixPolarReference, ThreeComponentDotWithCancellation){MPExpect(MPDot(VxMatrixPolarDot3));}
TEST(MatrixPolarReference, LargestColumnUsesFirstStrictMaximum){MPExpect(MPMax(VxMatrixPolarMaxColumn));}
TEST(MatrixPolarReference, HouseholderSignScaleAndZero){MPExpect(MPReflector(VxMatrixPolarMakeReflector,true));}
TEST(MatrixPolarReference, ColumnReflectionIsLeftHouseholder){MPExpect(MPReflect(VxMatrixPolarReflectColumns,false));}
TEST(MatrixPolarReference, RowReflectionIsRightHouseholder){MPExpect(MPReflect(VxMatrixPolarReflectRows,true));}
TEST(MatrixPolarReference, RankOnePreservesInputBeforeAliasedOutput){MPExpect(MPRank1(VxMatrixPolarOrthogonalRank1,true));}
TEST(MatrixPolarReference, RankTwoCofactorScaleAndWholeObjectAliases){MPExpect(MPRank2(VxMatrixPolarOrthogonalRank2,true));}
TEST(MatrixPolarReference, SignedDiagonalScaleAndInputAliases){MPExpect(MPDiagonal(Vx3DMatrixPolarDecomposition,true));}
TEST(MatrixPolarReference, KnownNonCommutingOrthogonalAndPositiveFactors){MPExpect(MPKnown(Vx3DMatrixPolarDecomposition,true));}
TEST(MatrixPolarReference, NonsymmetricRankOneHasOrthogonalCompletion){MPExpect(MPSingular(Vx3DMatrixPolarDecomposition,1,true));}
TEST(MatrixPolarReference, RankTwoHasSymmetricPositiveStretch){MPExpect(MPSingular(Vx3DMatrixPolarDecomposition,2,true));}
TEST(MatrixPolarReference, CancellationAndWideConditionRange){MPExpect(MPCondition(Vx3DMatrixPolarDecomposition));}
TEST(MatrixPolarReference, SevenNonlinearInputFieldsAreIgnored){MPExpect(MPIgnored(Vx3DMatrixPolarDecomposition));}
TEST(MatrixPolarReference, ReadOnly44ByteInputAnd64ByteOutputs){MPExpect(MPFootprint(Vx3DMatrixPolarDecomposition));}
#endif
