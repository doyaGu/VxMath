#include <cmath>
#include <cstring>
#include <cfloat>
#include <new>
#ifndef VX_OPERATOR_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef void (*MVO3Fn)(VxVector *,const VxMatrix &,const VxVector *);
typedef void (*MVO4PointFn)(VxVector4 *,const VxMatrix &,const VxVector *);
typedef void (*MVO4FullFn)(VxVector4 *,const VxMatrix &,const VxVector4 *);
struct MVOResult { int calls=0,errors=0,faults=0,hits[4]={},apiCalls[3]={}; };
static void MVOInvoke(MVOResult &r,int which,float *out,const VxMatrix &m,const VxVector4 &v) {
    ++r.calls;++r.hits[which];
    if (which==0) { const VxVector q=static_cast<const VxVector &>(v)*m;std::memcpy(out,&q,12); }
    if (which==1) { const VxVector q=m*static_cast<const VxVector &>(v);std::memcpy(out,&q,12); }
    if (which==2) { const VxVector4 q=m*v;std::memcpy(out,&q,16); }
    if (which==3) { const VxVector4 q=v*m;std::memcpy(out,&q,16); }
}
static void MVOLiteral(VxMatrix &m) {
    const float values[4][4]={{2,3,5,7},{11,13,17,19},{23,29,31,37},{41,43,47,53}};
    std::memcpy(&m,values,64);
}
static bool MVOExpected(const float *out,int count,const VxMatrix &m,const VxVector4 &v,bool actualW=false) {
    // Bounded integer/half-integer lattice images are exact in double.
    for (int c=0;c<count;++c) {
        double expected=double(m[3][c])*(actualW?v.w:1);
        for(int row=0;row<3;++row)expected+=double(m[row][c])*v[row];
        if(out[c]!=float(expected))return false;
    }
    return true;
}
static unsigned MVONext(unsigned &seed) { seed=1664525u*seed+1013904223u;return seed; }
static MVOResult MVOInteger() {
    MVOResult r;unsigned seed=0x31614abdu;
    for(int n=0;n<256;++n) {
        VxMatrix m;for(int i=0;i<4;++i)for(int j=0;j<4;++j)m[i][j]=float(int(MVONext(seed)%35)-17);
        const VxVector4 v(float(int(MVONext(seed)%15)-7),float(int(MVONext(seed)%15)-7),float(int(MVONext(seed)%15)-7),float(n%9-4));
        unsigned mb[16],vb[4];std::memcpy(mb,&m,64);std::memcpy(vb,&v,16);
        for(int which=0;which<4;++which) { float out[4];MVOInvoke(r,which,out,m,v);r.errors+=!MVOExpected(out,which<2?3:4,m,v); }
        r.errors+=std::memcmp(mb,&m,64)!=0||std::memcmp(vb,&v,16)!=0;
    }
    return r;
}
static MVOResult MVOWBits() {
    MVOResult r;const unsigned words[]={0,0x80000000u,0x3f000000u,0x3f800000u,0xc0400000u,0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,0x7fc12345u,0x7f800001u,1};
    VxMatrix m;MVOLiteral(m);
    for(unsigned word:words)for(int which=0;which<4;++which) {
        VxVector4 v(2,-3,5,1);std::memcpy(&v.w,&word,4);float out[4];MVOInvoke(r,which,out,m,v);
        unsigned after;std::memcpy(&after,&v.w,4);r.errors+=!MVOExpected(out,which<2?3:4,m,v)||after!=word;
    }
    return r;
}
static MVOResult MVOAPIs(MVO3Fn point3,MVO4PointFn point4,MVO4FullFn full4) {
    MVOResult r;VxMatrix m;MVOLiteral(m);const float ws[]={-2,0,0.5f,1,3};
    for(float w:ws) {
        const VxVector4 v(2,-3,5,w);VxVector p;VxVector4 q,f;
        point3(&p,m,&v);point4(&q,m,&v);full4(&f,m,&v);
        ++r.apiCalls[0];++r.apiCalls[1];++r.apiCalls[2];
        r.errors+=!MVOExpected(&p.x,3,m,v)||!MVOExpected(&q.x,4,m,v)||!MVOExpected(&f.x,4,m,v,true);
        for(int which=0;which<4;++which) {float out[4];MVOInvoke(r,which,out,m,v);r.errors+=!MVOExpected(out,which<2?3:4,m,v);}
    }
    return r;
}
static MVOResult MVOWide() {
    MVOResult r;
    for(int kind=0;kind<4;++kind) {
        VxMatrix m;VxVector4 v;float expected[4];
        for(int c=0;c<4;++c) {
            if(kind==0) {m[0][c]=2;m[1][c]=-2;m[2][c]=3+c;m[3][c]=5+c;expected[c]=float(8+2*c);}
            if(kind==1) {m[0][c]=1e30f;m[1][c]=-1e30f;m[2][c]=1;m[3][c]=2;expected[c]=3;}
            if(kind==2) {m[0][c]=m[1][c]=0.5f;m[2][c]=0;m[3][c]=std::ldexp(1.0f,-149);expected[c]=std::ldexp(1.0f,-148);}
            if(kind==3) {m[0][c]=m[1][c]=m[2][c]=1;m[3][c]=0;expected[c]=std::nextafter(1.0f,2.0f);}
        }
        if(kind==0)v=VxVector4(FLT_MAX,FLT_MAX,1,0);
        if(kind==1)v=VxVector4(1e30f,1e30f,1,-2);
        if(kind==2)v=VxVector4(std::ldexp(1.0f,-149),std::ldexp(1.0f,-149),0,0);
        if(kind==3)v=VxVector4(1,std::ldexp(1.0f,-24),std::ldexp(1.0f,-50),7);
        for(int which=0;which<4;++which) {
            float out[4];MVOInvoke(r,which,out,m,v);bool ok=true;
            for(int c=0;c<(which<2?3:4);++c)ok=ok&&out[c]==expected[c];r.errors+=!ok;
        }
    }
    return r;
}
static MVOResult MVOIgnoredColumn() {
    MVOResult r;const unsigned words[]={0x7f800000u,0xff800000u,0x7fc12345u,0x7f800001u};
    const VxVector4 v(2,-3,5,1);
    for(unsigned word:words)for(int row=0;row<4;++row) {
        VxMatrix m;MVOLiteral(m);std::memcpy(&m[row][3],&word,4);
        for(int which=0;which<2;++which) {float out[4];MVOInvoke(r,which,out,m,v);r.errors+=!MVOExpected(out,3,m,v);}
    }
    return r;
}
static MVOResult MVOAssignments() {
    MVOResult r;
    for(int which=0;which<4;++which)for(int dest=0;dest<4;++dest)for(int source=-1;source<4;++source) {
        VxMatrix m;MVOLiteral(m);const VxMatrix before=m;
        const VxVector4 v=source<0?VxVector4(2,-3,5,0):m[source];
        float expected[4];for(int c=0;c<4;++c) {long long value=static_cast<long long>(before[3][c]);for(int row=0;row<3;++row)value+=static_cast<long long>(before[row][c])*static_cast<long long>(v[row]);expected[c]=float(value);}
        const VxVector4 &input=source<0?v:m[source];MVOInvoke(r,which,m[dest],m,input);
        const int size=which<2?3:4;for(int c=0;c<size;++c)r.errors+=m[dest][c]!=expected[c];
        if(size==3)r.errors+=m[dest][3]!=before[dest][3];
    }
    for(int which=0;which<4;++which) {
        VxMatrix m;MVOLiteral(m);VxVector4 v(2,-3,5,0);const VxVector4 before=v;
        MVOInvoke(r,which,&v.x,m,v);r.errors+=!MVOExpected(&v.x,which<2?3:4,m,before);
        if(which<2)r.errors+=v.w!=before.w;
    }
    return r;
}
#if defined(_WIN32)
static bool MVOGuardCall(MVOResult *r,int which,float *out,const VxMatrix *m,const VxVector4 *v) {
    __try {MVOInvoke(*r,which,out,*m,*v);return true;} __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
#endif
static MVOResult MVOFootprint() {
    MVOResult r;
#if defined(_WIN32)
    SYSTEM_INFO info;GetSystemInfo(&info);const size_t page=info.dwPageSize;
    unsigned char *matrix=static_cast<unsigned char *>(VirtualAlloc(nullptr,3*page,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    unsigned char *vector=static_cast<unsigned char *>(VirtualAlloc(nullptr,3*page,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!matrix||!vector){++r.errors;if(matrix)VirtualFree(matrix,0,MEM_RELEASE);if(vector)VirtualFree(vector,0,MEM_RELEASE);return r;}
    for(int edge=0;edge<2;++edge) {
        DWORD old;VirtualProtect(matrix,3*page,PAGE_READWRITE,&old);VirtualProtect(vector,3*page,PAGE_READWRITE,&old);
        VxMatrix *m=new(matrix+(edge?2*page-64:page))VxMatrix;MVOLiteral(*m);
        // The complete Vector4 object exists, but its unused w field on the
        // upper edge becomes unreadable. Only XYZ may be inspected.
        VxVector4 *v=new(vector+(edge?2*page-12:page))VxVector4(2,-3,5,0);
        VxMatrix reference=*m;const VxVector4 point(2,-3,5,1);
        VirtualProtect(matrix,page,PAGE_NOACCESS,&old);VirtualProtect(matrix+page,page,PAGE_READONLY,&old);VirtualProtect(matrix+2*page,page,PAGE_NOACCESS,&old);
        VirtualProtect(vector,page,PAGE_NOACCESS,&old);VirtualProtect(vector+page,page,PAGE_READONLY,&old);VirtualProtect(vector+2*page,page,PAGE_NOACCESS,&old);
        for(int which=0;which<4;++which) {
            alignas(VxVector4) unsigned char bytes[32];std::memset(bytes,0xa5,32);float *out=reinterpret_cast<float *>(bytes+4);
            const bool ok=MVOGuardCall(&r,which,out,m,v);r.faults+=!ok;r.errors+=!ok||!MVOExpected(out,which<2?3:4,reference,point);
            const int end=4+(which<2?12:16);for(int i=0;i<32;++i)if(i<4||i>=end)r.errors+=bytes[i]!=0xa5;
        }
    }
    VirtualFree(matrix,0,MEM_RELEASE);VirtualFree(vector,0,MEM_RELEASE);
#endif
    return r;
}
#ifndef VX_OPERATOR_PROBE
TEST(MatrixVectorOperatorReference, BothOrdersUsePointLatticeImages) {EXPECT_EQ(MVOInteger().errors,0);}
TEST(MatrixVectorOperatorReference, IgnoreSourceWIncludingNonfiniteBits) {EXPECT_EQ(MVOWBits().errors,0);}
TEST(MatrixVectorOperatorReference, KeepExplicitHomogeneousApiDistinct) {EXPECT_EQ(MVOAPIs(Vx3DMultiplyMatrixVector,Vx3DMultiplyMatrixVector4,Vx3DMultiplyMatrixVector4).errors,0);}
TEST(MatrixVectorOperatorReference, RetainCancellationRangeAndRounding) {EXPECT_EQ(MVOWide().errors,0);}
TEST(MatrixVectorOperatorReference, IgnoreFourthColumnForVector3) {EXPECT_EQ(MVOIgnoredColumn().errors,0);}
TEST(MatrixVectorOperatorReference, EvaluateReturnBeforeAssignmentAliases) {EXPECT_EQ(MVOAssignments().errors,0);}
TEST(MatrixVectorOperatorReference, ReadOnlyMatrix64Xyz12AndExactResultFootprint) {const MVOResult r=MVOFootprint();EXPECT_EQ(r.errors+r.faults,0);}
#endif
