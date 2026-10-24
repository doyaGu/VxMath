#include <cmath>
#include <cstring>
#include <cfloat>
#include <new>
#ifndef VX_SIMPLE_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef void (*MSDDecomposeFn)(const VxMatrix &,VxQuaternion &,VxVector &,VxVector &);
typedef void (*MSDInterpolateFn)(float,VxMatrix &,const VxMatrix &,const VxMatrix &);
struct MSDResult { int calls=0,errors=0,faults=0; };
static bool MSDNear(double a,double b,double scale=1) {
    if(std::isnan(b))return std::isnan(a);if(std::isinf(b))return a==b;
    return std::isfinite(a)&&std::fabs(a-b)<=3e-5*scale+2*std::ldexp(1.0,-149);
}
static void MSDRotation(int axis,double angle,double (&r)[3][3]) {
    for(int i=0;i<3;++i)for(int j=0;j<3;++j)r[i][j]=i==j?1:0;
    const int p=(axis+1)%3,q=(p+1)%3;r[p][p]=r[q][q]=std::cos(angle);r[p][q]=-std::sin(angle);r[q][p]=std::sin(angle);
}
static void MSDMake(VxMatrix &m,int axis,double angle,const double *scale,const double *position) {
    double r[3][3];MSDRotation(axis,angle,r);m.SetIdentity();for(int i=0;i<3;++i){for(int j=0;j<3;++j)m[i][j]=float(scale[i]*r[i][j]);m[3][i]=float(position[i]);}
}
static bool MSDQuaternion(const VxQuaternion &q,const double (&r)[3][3]) {
    const double x=q.x,y=q.y,z=q.z,w=q.w;
    const double actual[3][3]={{1-2*(y*y+z*z),2*(x*y-w*z),2*(x*z+w*y)},
        {2*(x*y+w*z),1-2*(x*x+z*z),2*(y*z-w*x)}, {2*(x*z-w*y),2*(y*z+w*x),1-2*(x*x+y*y)}};
    bool ok=MSDNear(x*x+y*y+z*z+w*w,1);for(int i=0;i<3;++i)for(int j=0;j<3;++j)ok=ok&&MSDNear(actual[i][j],r[i][j]);return ok;
}
static MSDResult MSDOrthogonal(MSDDecomposeFn fn) {
    MSDResult result;const int powers[]={-100,0,100};const double angles[]={0,0.7,2.7},pos[]={-7,4,-0.0};
    for(int axis=0;axis<3;++axis)for(int power:powers)for(double angle:angles)for(int signs=0;signs<8;++signs) {
        const double size=std::ldexp(1.0,power),s[]={signs&1?-size:size,signs&2?-2*size:2*size,signs&4?-3*size:3*size};VxMatrix a;MSDMake(a,axis,angle,s,pos);unsigned before[16];std::memcpy(before,&a,64);
        VxQuaternion q;VxVector p,k;fn(a,q,p,k);++result.calls;double expected[3][3];MSDRotation(axis,angle,expected);
        const int x=signs&1?-1:1,y=signs&2?-1:1;for(int i=0;i<3;++i)for(int j=0;j<3;++j)expected[i][j]*=i==0?x:i==1?y:x*y;
        bool ok=MSDQuaternion(q,expected)&&MSDNear(k.x,size,size)&&MSDNear(k.y,2*size,2*size)&&MSDNear(k.z,x*y*s[2],3*size)&&std::memcmp(before,&a,64)==0;
        for(int i=0;i<3;++i)ok=ok&&std::memcmp(&p[i],&a[3][i],4)==0;result.errors+=!ok;
    }
    return result;
}
static MSDResult MSDShear(MSDDecomposeFn fn) {
    MSDResult result;
    // Rows0/1 are normalized independently, not Gram-Schmidt/polar; the
    // third row is their cross product and need not have unit length.
    VxMatrix a;a.SetIdentity();a[0][0]=2;a[1][0]=a[1][1]=2;const double h=float(1/std::sqrt(2.0)),root=std::sqrt(2+2*h);
    const double thirds[][3]={{0,0,3},{5,-4,-3},{0,0,0}};
    for(const auto &third:thirds){for(int i=0;i<3;++i)a[2][i]=float(third[i]);VxQuaternion q;VxVector p,k;fn(a,q,p,k);++result.calls;
        result.errors+=!MSDNear(q.x,0)||!MSDNear(q.y,0)||!MSDNear(q.z,h/(2*root))||!MSDNear(q.w,root/2)||!MSDNear(k.x,2)||!MSDNear(k.y,4*h)||!MSDNear(k.z,h*third[2],std::fabs(third[2]));}
    return result;
}
static MSDResult MSDProjectionRange(MSDDecomposeFn fn) {
    MSDResult result;const double unit=std::ldexp(1.0,-149);
    for(int kind=0;kind<2;++kind){VxMatrix a;a.SetIdentity();a[0][0]=1;a[0][1]=-1;a[1][0]=a[1][1]=kind?-1:1;a[1][2]=-2;
        for(int i=0;i<3;++i)a[2][i]=kind?FLT_MAX:float(unit);VxQuaternion q;VxVector p,k;fn(a,q,p,k);++result.calls;
        if(kind)result.errors+=!MSDNear(k.z,double(FLT_MAX)/std::sqrt(3.0),FLT_MAX);
        else result.errors+=k.z!=float(2*unit);
    }
    VxMatrix tiny;tiny.SetIdentity();for(int i=0;i<3;++i)tiny[0][i]=float(unit);tiny[1][0]=-1;tiny[1][1]=1;tiny[1][2]=0;
    VxQuaternion q;VxVector p,k;fn(tiny,q,p,k);++result.calls;result.errors+=k.x!=float(2*unit);return result;
}
static MSDResult MSDFields(MSDDecomposeFn fn) {
    MSDResult result;const unsigned words[]={0,0x80000000u,0x7f800000u,0xff800000u,0x7fc12345u,0x7f800001u};const int fields[]={3,7,11,15};
    for(int field:fields)for(unsigned word:words){VxMatrix a;a.SetIdentity();std::memcpy(reinterpret_cast<unsigned char *>(&a)+4*field,&word,4);VxQuaternion q;VxVector p,k;unsigned before[16];std::memcpy(before,&a,64);fn(a,q,p,k);++result.calls;
        const double identity[3][3]={{1,0,0},{0,1,0},{0,0,1}};result.errors+=!MSDQuaternion(q,identity)||k.x!=1||k.y!=1||k.z!=1||std::memcmp(before,&a,64)!=0;}
    for(unsigned word:words){VxMatrix a;a.SetIdentity();for(int i=0;i<3;++i)std::memcpy(&a[3][i],&word,4);VxQuaternion q;VxVector p,k;fn(a,q,p,k);++result.calls;for(int i=0;i<3;++i)result.errors+=std::memcmp(&p[i],&word,4)!=0;}
    VxMatrix a;const double s[]={2,3,-4},p0[]={7,8,9};MSDMake(a,1,0.7,s,p0);VxQuaternion q;VxVector p,k;fn(a,q,p,k);++result.calls;VxVector shared;fn(a,q,shared,shared);++result.calls;result.errors+=std::memcmp(&shared,&k,12)!=0;return result;
}
static bool MSDMatrix(const VxMatrix &m,const double (&r)[3][3],const double *scale,const double *position) {
    bool ok=true;for(int i=0;i<3;++i){for(int j=0;j<3;++j)ok=ok&&MSDNear(m[i][j],float(scale[i]*r[i][j]),std::fabs(scale[i]));ok=ok&&m[i][3]==0&&MSDNear(m[3][i],position[i],std::fabs(position[i]));}return ok&&m[3][3]==1;
}
static MSDResult MSDAnalytic(MSDInterpolateFn fn) {
    MSDResult result;const float steps[]={-1,-0.25f,0.125f,0.5f,0.875f,1.25f,2};const double pairs[][2]={{0,1.5},{2.8,-2.8},{0,0.1}},sa[]={2,3,-4},sb[]={4,5,6},pa[]={-7,2,-5},pb[]={3,-4,9};
    const double pi=std::acos(-1.0);
    for(int axis=0;axis<3;++axis)for(const auto &pair:pairs)for(float t:steps)for(int alias=0;alias<3;++alias) {
        VxMatrix a,b,out;MSDMake(a,axis,pair[0],sa,pa);MSDMake(b,axis,pair[1],sb,pb);VxMatrix &dest=alias==1?a:alias==2?b:out;
        double delta=pair[1]-pair[0];if(delta>pi)delta-=2*pi;if(delta<-pi)delta+=2*pi;
        double angle=pair[0]+t*delta;
        if(std::cos(delta/2)>=0.99)angle=pair[0]+2*std::atan2(t*std::sin(delta/2),1-t+t*std::cos(delta/2));
        double r[3][3],s[3],p[3];MSDRotation(axis,angle,r);for(int i=0;i<3;++i){s[i]=sa[i]+double(t)*(sb[i]-sa[i]);p[i]=pa[i]+double(t)*(pb[i]-pa[i]);}
        fn(t,dest,a,b);++result.calls;result.errors+=!MSDMatrix(dest,r,s,p);
    }
    return result;
}
static MSDResult MSDValueRange(MSDInterpolateFn fn) {
    MSDResult result;const float steps[]={0.25f,0.5f,0.75f,2};const double identity[3][3]={{1,0,0},{0,1,0},{0,0,1}};
    for(float t:steps)for(int alias=0;alias<3;++alias){VxMatrix a,b,out;a.SetIdentity();b.SetIdentity();for(int i=0;i<3;++i){a[3][i]=FLT_MAX;b[3][i]=-FLT_MAX;}a[2][2]=FLT_MAX;b[2][2]=-FLT_MAX;VxMatrix &dest=alias==1?a:alias==2?b:out;
        fn(t,dest,a,b);++result.calls;const double v=float(double(FLT_MAX)+double(t)*(-2*double(FLT_MAX))),s[]={1,1,v},p[]={v,v,v};result.errors+=!MSDMatrix(dest,identity,s,p);}
    VxMatrix a,b,out;a.SetIdentity();b.SetIdentity();const float unit=std::ldexp(1.0f,-149);a[0][0]=unit;b[0][0]=2*unit;a[3][0]=unit;b[3][0]=2*unit;
    fn(0.5f,out,a,b);++result.calls;result.errors+=out[0][0]!=2*unit||out[3][0]!=2*unit;return result;
}
static MSDResult MSDEndpoints(MSDInterpolateFn fn) {
    MSDResult result;const unsigned words[]={0x80000000u,0x7fc12345u,0x7f800001u,0x7f800000u,0xff800000u,0x3eaaaaabu,0};const float steps[]={0,-0.0f,1};
    for(float t:steps)for(int alias=0;alias<3;++alias)for(int sample=0;sample<4;++sample) {
        VxMatrix a,b,out;for(int i=0;i<16;++i){unsigned x=words[(sample+i)%7],y=words[(sample+2*i+1)%7];std::memcpy(reinterpret_cast<unsigned char *>(&a)+4*i,&x,4);std::memcpy(reinterpret_cast<unsigned char *>(&b)+4*i,&y,4);}
        unsigned expected[16];std::memcpy(expected,t==1?&b:&a,64);VxMatrix &dest=alias==1?a:alias==2?b:out;fn(t,dest,a,b);++result.calls;result.errors+=std::memcmp(expected,&dest,64)!=0;
    }
    return result;
}
static MSDResult MSDInteriorFields(MSDInterpolateFn fn) {
    MSDResult result;const unsigned word=0x7f800001u;const double scale[]={2,3,4},p[]={1,2,3},identity[3][3]={{1,0,0},{0,1,0},{0,0,1}};
    const int fields[]={3,7,11,15};
    for(int same=0;same<2;++same)for(int alias=0;alias<3;++alias){VxMatrix a,b,out;MSDMake(a,0,0,scale,p);b=a;for(int field:fields){std::memcpy(reinterpret_cast<unsigned char *>(&a)+4*field,&word,4);std::memcpy(reinterpret_cast<unsigned char *>(&b)+4*field,&word,4);}unsigned beforeA[16],beforeB[16];std::memcpy(beforeA,&a,64);std::memcpy(beforeB,&b,64);
        VxMatrix &dest=alias==1?a:alias==2?b:out;fn(0.5f,dest,a,same?a:b);++result.calls;result.errors+=!MSDMatrix(dest,identity,scale,p);if(alias!=1)result.errors+=std::memcmp(beforeA,&a,64)!=0;if(alias!=2)result.errors+=std::memcmp(beforeB,&b,64)!=0;}
    return result;
}
#if defined(_WIN32) && defined(_MSC_VER)
static bool MSDProtected(MSDDecomposeFn fn,const VxMatrix *a,VxQuaternion *q,VxVector *p,VxVector *k){__try{fn(*a,*q,*p,*k);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
static bool MSIProtected(MSDInterpolateFn fn,float t,VxMatrix *out,const VxMatrix *a,const VxMatrix *b){__try{fn(t,*out,*a,*b);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
#endif
static MSDResult MSDFootprint(MSDDecomposeFn decompose,MSDInterpolateFn interpolate) {
    MSDResult result;
#if defined(_WIN32) && defined(_MSC_VER)
    SYSTEM_INFO info;GetSystemInfo(&info);const unsigned size=info.dwPageSize;DWORD old;unsigned char *memory=static_cast<unsigned char *>(VirtualAlloc(nullptr,12*size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));if(!memory){++result.errors;return result;}
    for(int edge=0;edge<2;++edge){VxMatrix *a=new(memory+size+(edge?size-64:0))VxMatrix;a->SetIdentity();
        VxMatrix *b=nullptr,*out=nullptr;VxQuaternion *q=nullptr;VxVector *p=nullptr,*k=nullptr;
        if(interpolate){b=new(memory+4*size+(edge?size-64:0))VxMatrix;out=new(memory+7*size+(edge?size-64:0))VxMatrix;b->SetIdentity();b->operator[](3)[0]=2;}
        else {q=new(memory+4*size+(edge?size-16:0))VxQuaternion;p=new(memory+7*size+(edge?size-12:0))VxVector;k=new(memory+10*size+(edge?size-12:0))VxVector;}
        const int guards[]={0,2,3,5,6,8,9,11};for(int page:guards)VirtualProtect(memory+page*size,size,PAGE_NOACCESS,&old);VirtualProtect(memory+size,size,PAGE_READONLY,&old);if(interpolate)VirtualProtect(memory+4*size,size,PAGE_READONLY,&old);
        if(interpolate){const bool ran=MSIProtected(interpolate,0.5f,out,a,b);++result.calls;result.faults+=!ran;if(ran)result.errors+=out->operator[](3)[0]!=1;}
        else {const bool ran=MSDProtected(decompose,a,q,p,k);++result.calls;result.faults+=!ran;if(ran)result.errors+=q->w!=1||p->x!=0||p->y!=0||p->z!=0||k->x!=1||k->y!=1||k->z!=1;}
        VirtualProtect(memory,12*size,PAGE_READWRITE,&old);a->~VxMatrix();if(interpolate){b->~VxMatrix();out->~VxMatrix();}else{q->~VxQuaternion();p->~VxVector();k->~VxVector();}}
    if(interpolate)for(int selected=0;selected<2;++selected){VxMatrix *a=new(memory+size)VxMatrix,*b=new(memory+4*size)VxMatrix,*out=new(memory+7*size)VxMatrix;a->SetIdentity();b->SetIdentity();unsigned expected[16];std::memcpy(expected,selected?b:a,64);VirtualProtect(memory+(selected?size:4*size),size,PAGE_NOACCESS,&old);const bool ran=MSIProtected(interpolate,float(selected),out,a,b);++result.calls;result.faults+=!ran;VirtualProtect(memory,12*size,PAGE_READWRITE,&old);if(ran)result.errors+=std::memcmp(expected,out,64)!=0;}
    VirtualFree(memory,0,MEM_RELEASE);
#else
    VxMatrix a,b,out;a.SetIdentity();b.SetIdentity();if(decompose){VxQuaternion q;VxVector p,k;decompose(a,q,p,k);++result.calls;result.errors+=k.x!=1;}if(interpolate){interpolate(0.5f,out,a,b);++result.calls;result.errors+=out[3][3]!=1;}
#endif
    return result;
}
#ifndef VX_SIMPLE_PROBE
static void MSDExpect(MSDResult r){EXPECT_GT(r.calls,0);EXPECT_EQ(r.errors,0);EXPECT_EQ(r.faults,0);}
TEST(MatrixSimpleReference, RowScaledRotationSignsAndWideScale){MSDExpect(MSDOrthogonal(Vx3DDecomposeMatrix));}
TEST(MatrixSimpleReference, ShearUsesIndependentRowsAndCrossProjection){MSDExpect(MSDShear(Vx3DDecomposeMatrix));}
TEST(MatrixSimpleReference, RowProjectionKeepsFiniteCancellationAndSubnormalTerms){MSDExpect(MSDProjectionRange(Vx3DDecomposeMatrix));}
TEST(MatrixSimpleReference, DecomposeIgnoredWTranslationBitsAndSharedVectors){MSDExpect(MSDFields(Vx3DDecomposeMatrix));}
TEST(MatrixSimpleReference, ShortArcNlerpExtrapolationAndWholeOutputAliases){MSDExpect(MSDAnalytic(Vx3DInterpolateMatrix));MSDExpect(MSDAnalytic(Vx3DInterpolateMatrixNoScale));}
TEST(MatrixSimpleReference, InterpolatedValuesKeepFiniteMidpointsAndSubnormals){MSDExpect(MSDValueRange(Vx3DInterpolateMatrix));MSDExpect(MSDValueRange(Vx3DInterpolateMatrixNoScale));}
TEST(MatrixSimpleReference, EndpointsCopyAllFieldsIncludingNonfiniteBits){MSDExpect(MSDEndpoints(Vx3DInterpolateMatrix));MSDExpect(MSDEndpoints(Vx3DInterpolateMatrixNoScale));}
TEST(MatrixSimpleReference, InteriorIgnoresWAndHandlesIdenticalInputs){MSDExpect(MSDInteriorFields(Vx3DInterpolateMatrix));MSDExpect(MSDInteriorFields(Vx3DInterpolateMatrixNoScale));}
TEST(MatrixSimpleReference, ReadOnlyInputsAndOutputExtents){MSDExpect(MSDFootprint(Vx3DDecomposeMatrix,nullptr));MSDExpect(MSDFootprint(nullptr,Vx3DInterpolateMatrix));MSDExpect(MSDFootprint(nullptr,Vx3DInterpolateMatrixNoScale));}
#endif
