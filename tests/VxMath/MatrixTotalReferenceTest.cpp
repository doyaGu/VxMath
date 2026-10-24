#include <cmath>
#include <cstring>
#include <new>
#ifndef VX_TOTAL_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef double MTMatrix[3][3];
typedef float (*MTFn)(const VxMatrix &,VxQuaternion *,VxVector *,VxVector *,VxQuaternion *);
struct MTResult { int calls=0, errors=0, faults=0; };
static bool MTNear(double a,double b,double scale=1) { return std::isfinite(a)&&std::isfinite(b)&&std::fabs(a-b)<=4e-5*scale+8*std::ldexp(1.0,-149); }
static double MTMax(const MTMatrix &a) { double x=0;for(int r=0;r<3;++r)for(int c=0;c<3;++c)if(std::fabs(a[r][c])>x)x=std::fabs(a[r][c]);return x; }
static bool MTRotation(const VxQuaternion &q,MTMatrix &r) {
    const double x=q.x,y=q.y,z=q.z,w=q.w;
    r[0][0]=1-2*(y*y+z*z);r[0][1]=2*(x*y-w*z);r[0][2]=2*(x*z+w*y);
    r[1][0]=2*(x*y+w*z);r[1][1]=1-2*(x*x+z*z);r[1][2]=2*(y*z-w*x);
    r[2][0]=2*(x*z-w*y);r[2][1]=2*(y*z+w*x);r[2][2]=1-2*(x*x+y*y);
    return MTNear(x*x+y*y+z*z+w*w,1);
}
static bool MTCheck(const VxMatrix &a,const VxQuaternion &q,const VxVector &p,const VxVector &k,const VxQuaternion &u,float sign,const MTMatrix *knownR=nullptr,const MTMatrix *knownS=nullptr) {
    MTMatrix rotation,axes,stretch={},linear;bool ok=MTRotation(q,rotation)&&MTRotation(u,axes)&&(sign==1||sign==-1);
    for(int r=0;r<3;++r)for(int c=0;c<3;++c){linear[r][c]=a[r][c];for(int i=0;i<3;++i)stretch[r][c]+=axes[r][i]*k[i]*axes[c][i];}
    const double scale=MTMax(linear);for(int i=0;i<3;++i)ok=ok&&std::isfinite(k[i])&&k[i]>=-4e-5*scale;
    for(int r=0;r<3;++r)for(int c=0;c<3;++c) {
        double rebuilt=0;for(int i=0;i<3;++i)rebuilt+=sign*rotation[r][i]*stretch[i][c];
        ok=ok&&MTNear(rebuilt,a[r][c],scale);
        if(knownR)ok=ok&&MTNear(rotation[r][c],(*knownR)[r][c]);
        if(knownS)ok=ok&&MTNear(stretch[r][c],(*knownS)[r][c],scale);
    }
    for(int i=0;i<3;++i)ok=ok&&std::memcmp(&p[i],&a[3][i],4)==0;
    return ok;
}
static void MTMake(int seed,int power,int rank,int sign,VxMatrix &a,MTMatrix &rotation,MTMatrix &stretch) {
    MTMatrix axes={};for(int i=0;i<3;++i)axes[i][i]=1;
    const int p=seed%3,q=(p+1)%3;axes[p][p]=axes[q][q]=0.6;axes[p][q]=-0.8;axes[q][p]=0.8;
    VxQuaternion quat(0,0,0,0.8f);quat[seed%3]=0.6f;MTRotation(quat,rotation);
    const double values[]={std::ldexp(1.0,power),std::ldexp(2.0,power),std::ldexp(4.0,power)};
    for(int r=0;r<3;++r)for(int c=0;c<3;++c){stretch[r][c]=0;for(int i=0;i<rank;++i)stretch[r][c]+=axes[r][i]*values[i]*axes[c][i];}
    a.SetIdentity();for(int r=0;r<3;++r)for(int c=0;c<3;++c){double v=0;for(int i=0;i<3;++i)v+=sign*rotation[r][i]*stretch[i][c];a[r][c]=float(v);}
    a[3][0]=-7;a[3][1]=std::ldexp(1.0f,-100);a[3][2]=-0.0f;
}
static MTResult MTKnown(MTFn fn,int rank) {
    MTResult result;const int powers[]={-100,0,100};
    for(int seed=0;seed<6;++seed)for(int power:powers)for(int sign=-1;sign<=1;sign+=2) {
        VxMatrix a;MTMatrix r,s;MTMake(seed,power,rank,sign,a,r,s);unsigned before[16];std::memcpy(before,&a,64);
        VxQuaternion q,u;VxVector pos,k;const float d=fn(a,&q,&pos,&k,&u);++result.calls;
        // Float storage can turn an algebraic rank1/2 construction into a very
        // ill-conditioned full-rank matrix. Exact diagonal rank cases are below.
        result.errors+=!MTCheck(a,q,pos,k,u,d,rank==3?&r:nullptr,&s)||(rank==3&&d!=sign)||std::memcmp(before,&a,64)!=0;
    }
    return result;
}
static MTResult MTRepeated(MTFn fn,bool singular) {
    MTResult result;const int powers[]={-100,0,100};
    for(int axis=0;axis<3;++axis)for(int power:powers)for(int sign=-1;sign<=1;sign+=2) {
        VxMatrix a;a.SetIdentity();MTMatrix r={},s={};for(int i=0;i<3;++i){r[i][i]=1;s[i][i]=std::ldexp(singular?(i==axis?2.0:0):(i==axis?2.0:1),power);a[i][i]=float(sign*s[i][i]);}
        a[3][0]=3;a[3][1]=-0.0f;a[3][2]=-9;VxQuaternion q,u;VxVector pos,k;const float d=fn(a,&q,&pos,&k,&u);++result.calls;
        result.errors+=!MTCheck(a,q,pos,k,u,d,singular?nullptr:&r,&s)||d!=(singular?1:sign);
    }
    return result;
}
static MTResult MTMasks(MTFn fn) {
    MTResult result;
    for(int sample=0;sample<8;++sample) {
        VxMatrix a;MTMatrix r,s;MTMake(sample,0,sample%4,sample&1?-1:1,a,r,s);
        VxQuaternion fullQ,fullU;VxVector fullP,fullK;const float fullSign=fn(a,&fullQ,&fullP,&fullK,&fullU);++result.calls;
        result.errors+=!MTCheck(a,fullQ,fullP,fullK,fullU,fullSign);
        unsigned before[16];std::memcpy(before,&a,64);
        for(int mask=0;mask<16;++mask) {
            VxQuaternion q(31,32,33,34),u(41,42,43,44);VxVector p(51,52,53),k(61,62,63);
            const VxQuaternion q0=q,u0=u;const VxVector p0=p,k0=k;
            const float d=fn(a,mask&1?&q:nullptr,mask&2?&p:nullptr,mask&4?&k:nullptr,mask&8?&u:nullptr);++result.calls;
            bool ok=d==((mask&13)?fullSign:1)&&std::memcmp(before,&a,64)==0;
            ok=ok&&std::memcmp(&q,mask&1?&fullQ:&q0,16)==0&&std::memcmp(&p,mask&2?&fullP:&p0,12)==0;
            ok=ok&&std::memcmp(&k,mask&4?&fullK:&k0,12)==0&&std::memcmp(&u,mask&8?&fullU:&u0,16)==0;
            result.errors+=!ok;
        }
    }
    return result;
}
static MTResult MTShared(MTFn fn) {
    MTResult result;for(int sample=0;sample<6;++sample) {
        VxMatrix a;MTMatrix r,s;MTMake(sample,0,3,sample&1?-1:1,a,r,s);VxQuaternion q,u;VxVector p,k;
        const float d=fn(a,&q,&p,&k,&u);++result.calls;result.errors+=!MTCheck(a,q,p,k,u,d,&r,&s);
        for(int aliases=1;aliases<4;++aliases) {
            VxQuaternion aq,au;VxVector ap,ak;const float value=fn(a,&aq,&ap,aliases&2?&ap:&ak,aliases&1?&aq:&au);++result.calls;
            result.errors+=value!=d||std::memcmp(aliases&1?&aq:&au,&u,16)!=0||std::memcmp(aliases&2?&ap:&ak,&k,12)!=0;
            if(!(aliases&1))result.errors+=std::memcmp(&aq,&q,16)!=0;if(!(aliases&2))result.errors+=std::memcmp(&ap,&p,12)!=0;
        }
    }
    return result;
}
static MTResult MTIgnored(MTFn fn,bool pointer) {
    MTResult result;const unsigned words[]={0,0x80000000u,0x7f800000u,0xff800000u,0x7fc12345u,0x7f800001u};const int fields[]={3,7,11,15};
    for(int field:fields)for(unsigned word:words) {
        VxMatrix a;MTMatrix r,s;MTMake(1,0,3,-1,a,r,s);std::memcpy(reinterpret_cast<unsigned char *>(&a)+4*field,&word,4);
        unsigned before[16];std::memcpy(before,&a,64);VxQuaternion q,u;VxVector p,k;const float d=fn(a,&q,&p,&k,&u);++result.calls;
        result.errors+=!MTCheck(a,q,p,k,u,d,&r,&s)||d!=-1||std::memcmp(before,&a,64)!=0;
    }
    for(unsigned word:words) {
        VxMatrix a;a.SetIdentity();for(int i=0;i<3;++i)std::memcpy(&a[3][i],&word,4);
        VxQuaternion q,u;VxVector p,k;const float d=fn(a,&q,&p,&k,&u);++result.calls;
        for(int i=0;i<3;++i)result.errors+=std::memcmp(&p[i],&word,4)!=0;result.errors+=d!=1;
        if(pointer){const float value=fn(a,&q,nullptr,&k,&u);++result.calls;MTMatrix qr,ur;result.errors+=value!=1||!MTRotation(q,qr)||!MTRotation(u,ur)||k.x!=1||k.y!=1||k.z!=1;}
    }
    return result;
}
#if defined(_WIN32) && defined(_MSC_VER)
static bool MTProtected(MTFn fn,const VxMatrix *a,VxQuaternion *q,VxVector *p,VxVector *k,VxQuaternion *u,float *d) {
    __try{*d=fn(*a,q,p,k,u);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
#endif
static MTResult MTFootprint(MTFn fn,bool pointer) {
    MTResult result;
#if defined(_WIN32) && defined(_MSC_VER)
    SYSTEM_INFO info;GetSystemInfo(&info);const unsigned size=info.dwPageSize;DWORD old;
    unsigned char *memory=static_cast<unsigned char *>(VirtualAlloc(nullptr,15*size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));if(!memory){++result.errors;return result;}
    for(int mask=pointer?0:15;mask<16;++mask)for(int edge=0;edge<2;++edge) {
        const int needed=mask&2?60:44;VxMatrix *a=new(memory+size+(edge?size-needed:0))VxMatrix;a->SetIdentity();a->operator[](0)[0]=-4;a->operator[](1)[1]=2;a->operator[](2)[2]=3;a->operator[](3)[0]=-7;
        VxQuaternion *q=new(memory+4*size+(edge?size-16:0))VxQuaternion(31,32,33,34),*u=new(memory+13*size+(edge?size-16:0))VxQuaternion(41,42,43,44);
        VxVector *p=new(memory+7*size+(edge?size-12:0))VxVector(51,52,53),*k=new(memory+10*size+(edge?size-12:0))VxVector(61,62,63);
        const int guards[]={0,2,3,5,6,8,9,11,12,14};bool ok=true;for(int page:guards)ok=ok&&VirtualProtect(memory+page*size,size,PAGE_NOACCESS,&old);
        ok=ok&&VirtualProtect(memory+size,size,mask?PAGE_READONLY:PAGE_NOACCESS,&old);if(!ok){++result.errors;break;}
        float d=0;const bool ran=MTProtected(fn,a,mask&1?q:nullptr,mask&2?p:nullptr,mask&4?k:nullptr,mask&8?u:nullptr,&d);++result.calls;result.faults+=!ran;
        VirtualProtect(memory,15*size,PAGE_READWRITE,&old);
        if(ran){result.errors+=d!=((mask&13)?-1:1);if(mask==15)result.errors+=!MTCheck(*a,*q,*p,*k,*u,d);if(mask&2)result.errors+=p->x!=-7||p->y!=0||p->z!=0;}
        a->~VxMatrix();q->~VxQuaternion();u->~VxQuaternion();p->~VxVector();k->~VxVector();
    }
    if(pointer){VxMatrix *a=new(memory+size-48)VxMatrix;a->SetIdentity();a->operator[](3)[0]=9;a->operator[](3)[1]=-0.0f;a->operator[](3)[2]=-3;
        VirtualProtect(memory,size,PAGE_NOACCESS,&old);VxVector p;float d=0;const bool ran=MTProtected(fn,a,nullptr,&p,nullptr,nullptr,&d);++result.calls;result.faults+=!ran;
        VirtualProtect(memory,15*size,PAGE_READWRITE,&old);if(ran)result.errors+=d!=1||p.x!=9||p.y!=0||!std::signbit(p.y)||p.z!=-3;a->~VxMatrix();}
    VirtualFree(memory,0,MEM_RELEASE);
#else
    VxMatrix a;a.SetIdentity();VxQuaternion q,u;VxVector p,k;const float d=fn(a,&q,&p,&k,&u);++result.calls;result.errors+=!MTCheck(a,q,p,k,u,d);
#endif
    return result;
}
#ifndef VX_TOTAL_PROBE
static float MTTotal(const VxMatrix &a,VxQuaternion *q,VxVector *p,VxVector *k,VxQuaternion *u){return Vx3DDecomposeMatrixTotal(a,*q,*p,*k,*u);}
static void MTExpect(MTResult r){EXPECT_GT(r.calls,0);EXPECT_EQ(r.errors,0);EXPECT_EQ(r.faults,0);}
TEST(MatrixTotalReference, NonCommutingRotationStretchAndReflection){MTExpect(MTKnown(MTTotal,3));MTExpect(MTKnown(Vx3DDecomposeMatrixTotalPtr,3));}
TEST(MatrixTotalReference, ZeroMatrixHasRepresentableRotationCompletion){MTExpect(MTKnown(MTTotal,0));MTExpect(MTKnown(Vx3DDecomposeMatrixTotalPtr,0));}
TEST(MatrixTotalReference, NearlySingularStoredRankOneConstruction){MTExpect(MTKnown(MTTotal,1));MTExpect(MTKnown(Vx3DDecomposeMatrixTotalPtr,1));}
TEST(MatrixTotalReference, NearlySingularStoredRankTwoConstruction){MTExpect(MTKnown(MTTotal,2));MTExpect(MTKnown(Vx3DDecomposeMatrixTotalPtr,2));}
TEST(MatrixTotalReference, TwoEqualScalesKeepStretchAxisThroughSnuggle){MTExpect(MTRepeated(MTTotal,false));MTExpect(MTRepeated(Vx3DDecomposeMatrixTotalPtr,false));}
TEST(MatrixTotalReference, ExactRankOneReturnsPositiveSign){MTExpect(MTRepeated(MTTotal,true));MTExpect(MTRepeated(Vx3DDecomposeMatrixTotalPtr,true));}
TEST(MatrixTotalReference, AllOptionalMasksMatchFullResultsAndLeaveSentinels){MTExpect(MTMasks(Vx3DDecomposeMatrixTotalPtr));}
TEST(MatrixTotalReference, SharedSameTypeOutputsKeepLastWrittenResult){MTExpect(MTShared(MTTotal));MTExpect(MTShared(Vx3DDecomposeMatrixTotalPtr));}
TEST(MatrixTotalReference, IgnoredWFieldsAndBitPreservingTranslation){MTExpect(MTIgnored(MTTotal,false));MTExpect(MTIgnored(Vx3DDecomposeMatrixTotalPtr,true));}
TEST(MatrixTotalReference, MandatoryOutputExtentsAndReadOnly60ByteInput){MTExpect(MTFootprint(MTTotal,false));}
TEST(MatrixTotalReference, OptionalReadFootprintsAnd12Or16ByteOutputs){MTExpect(MTFootprint(Vx3DDecomposeMatrixTotalPtr,true));}
#endif
