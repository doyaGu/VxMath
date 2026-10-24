// Reproducible comparison with the frozen Pass309 implementation.
// The copied core is an isolated measurement boundary, not a new runtime API.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include "VxMath.h"
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define MIP_NOINLINE __declspec(noinline)
#else
#include <time.h>
#define MIP_NOINLINE __attribute__((noinline))
#endif
MIP_NOINLINE static void MIPBaselineFull(float step, VxMatrix &Res, const VxMatrix &A, const VxMatrix &B) {
    // Preserve the selected endpoint, including shear and singular matrices,
    // without letting the unused endpoint's decomposition contaminate it.
    if (step == 0.0f) { Res = A; return; }
    if (step == 1.0f) { Res = B; return; }
    VxQuaternion quatA, quatB;
    VxVector posA, posB, scaleA, scaleB;
    Vx3DDecomposeMatrix(A, quatA, posA, scaleA);
    Vx3DDecomposeMatrix(B, quatB, posB, scaleB);
    VxQuaternion rotation = Slerp(step, quatA, quatB);
    VxVector position, scale;
    // Float differences can overflow even when the interpolated value fits.
    // Retain packed SIMD arithmetic for all six interpolated components.
#if defined(VX_SIMD_SSE2)
    const __m128d t = _mm_set1_pd(step);
    const __m128d pa = _mm_cvtps_pd(VxSIMDLoadFloat3(&posA.x)), pb = _mm_cvtps_pd(VxSIMDLoadFloat3(&posB.x));
    const __m128d sa = _mm_cvtps_pd(VxSIMDLoadFloat3(&scaleA.x)), sb = _mm_cvtps_pd(VxSIMDLoadFloat3(&scaleB.x));
    const __m128d za = _mm_set_pd(scaleA.z, posA.z), zb = _mm_set_pd(scaleB.z, posB.z);
    const __m128 pxy = _mm_cvtpd_ps(_mm_add_pd(pa, _mm_mul_pd(_mm_sub_pd(pb, pa), t)));
    const __m128 sxy = _mm_cvtpd_ps(_mm_add_pd(sa, _mm_mul_pd(_mm_sub_pd(sb, sa), t)));
    const __m128 zPair = _mm_cvtpd_ps(_mm_add_pd(za, _mm_mul_pd(_mm_sub_pd(zb, za), t)));
    VxSIMDStoreFloat3(&position.x, _mm_shuffle_ps(pxy, zPair, _MM_SHUFFLE(0,0,1,0)));
    VxSIMDStoreFloat3(&scale.x, _mm_shuffle_ps(sxy, zPair, _MM_SHUFFLE(1,1,1,0)));
#else
    for (int i = 0; i < 3; ++i) {
        position[i] = float(double(posA[i]) + (double(posB[i])-posA[i])*step);
        scale[i] = float(double(scaleA[i]) + (double(scaleB[i])-scaleA[i])*step);
    }
#endif
    rotation.Normalize();
    const double x=rotation.x, y=rotation.y, z=rotation.z, w=rotation.w;
    const double xx=x*x, yy=y*y, zz=z*z, ww=w*w, xy=y*x, wz=w*z;
    const float xz=static_cast<float>(z*x), wx=static_cast<float>(w*x);
    const float yz=static_cast<float>(z*y), wy=static_cast<float>(w*y);
    Res.SetIdentity();
    Res[0][0]=static_cast<float>(ww+xx-yy-zz); Res[0][1]=static_cast<float>(2*xy-2*wz); Res[0][2]=2*wy+2*xz;
    Res[1][0]=static_cast<float>(2*wz+2*xy); Res[1][1]=static_cast<float>(ww-xx+yy-zz); Res[1][2]=2*yz-2*wx;
    Res[2][0]=2*xz-2*wy; Res[2][1]=2*yz+2*wx; Res[2][2]=static_cast<float>(ww-xx-yy+zz);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) Res[i][j] *= scale[i];
    Res[3][0]=position.x; Res[3][1]=position.y; Res[3][2]=position.z;
}
MIP_NOINLINE static void MIPBaselineCore(float step,VxVector &position,VxVector &scale,const VxVector &posA,const VxVector &posB,const VxVector &scaleA,const VxVector &scaleB) {
    // Float differences can overflow even when the interpolated value fits.
    // Retain packed SIMD arithmetic for all six interpolated components.
#if defined(VX_SIMD_SSE2)
    const __m128d t = _mm_set1_pd(step);
    const __m128d pa = _mm_cvtps_pd(VxSIMDLoadFloat3(&posA.x)), pb = _mm_cvtps_pd(VxSIMDLoadFloat3(&posB.x));
    const __m128d sa = _mm_cvtps_pd(VxSIMDLoadFloat3(&scaleA.x)), sb = _mm_cvtps_pd(VxSIMDLoadFloat3(&scaleB.x));
    const __m128d za = _mm_set_pd(scaleA.z, posA.z), zb = _mm_set_pd(scaleB.z, posB.z);
    const __m128 pxy = _mm_cvtpd_ps(_mm_add_pd(pa, _mm_mul_pd(_mm_sub_pd(pb, pa), t)));
    const __m128 sxy = _mm_cvtpd_ps(_mm_add_pd(sa, _mm_mul_pd(_mm_sub_pd(sb, sa), t)));
    const __m128 zPair = _mm_cvtpd_ps(_mm_add_pd(za, _mm_mul_pd(_mm_sub_pd(zb, za), t)));
    VxSIMDStoreFloat3(&position.x, _mm_shuffle_ps(pxy, zPair, _MM_SHUFFLE(0,0,1,0)));
    VxSIMDStoreFloat3(&scale.x, _mm_shuffle_ps(sxy, zPair, _MM_SHUFFLE(1,1,1,0)));
#else
    for (int i = 0; i < 3; ++i) {
        position[i] = float(double(posA[i]) + (double(posB[i])-posA[i])*step);
        scale[i] = float(double(scaleA[i]) + (double(scaleB[i])-scaleA[i])*step);
    }
#endif
}
MIP_NOINLINE static void MIPCurrentCore(float step,VxVector &position,VxVector &scale,const VxVector &posA,const VxVector &posB,const VxVector &scaleA,const VxVector &scaleB) {
    // Float differences can overflow even when the interpolated value fits.
    // Retain packed SIMD arithmetic for all six interpolated components.
#if defined(VX_SIMD_SSE2)
    const __m128d t = _mm_set1_pd(step);
    const __m128d pa = _mm_cvtps_pd(_mm_castsi128_ps(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(&posA.x))));
    const __m128d pb = _mm_cvtps_pd(_mm_castsi128_ps(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(&posB.x))));
    const __m128d sa = _mm_cvtps_pd(_mm_castsi128_ps(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(&scaleA.x))));
    const __m128d sb = _mm_cvtps_pd(_mm_castsi128_ps(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(&scaleB.x))));
    const __m128d za = _mm_cvtps_pd(_mm_unpacklo_ps(_mm_load_ss(&posA.z), _mm_load_ss(&scaleA.z)));
    const __m128d zb = _mm_cvtps_pd(_mm_unpacklo_ps(_mm_load_ss(&posB.z), _mm_load_ss(&scaleB.z)));
    const __m128 pxy = _mm_cvtpd_ps(_mm_add_pd(pa, _mm_mul_pd(_mm_sub_pd(pb, pa), t)));
    const __m128 sxy = _mm_cvtpd_ps(_mm_add_pd(sa, _mm_mul_pd(_mm_sub_pd(sb, sa), t)));
    const __m128 zPair = _mm_cvtpd_ps(_mm_add_pd(za, _mm_mul_pd(_mm_sub_pd(zb, za), t)));
    VxSIMDStoreFloat3(&position.x, _mm_shuffle_ps(pxy, zPair, _MM_SHUFFLE(0,0,1,0)));
    VxSIMDStoreFloat3(&scale.x, _mm_shuffle_ps(sxy, zPair, _MM_SHUFFLE(1,1,1,0)));
#else
    for (int i = 0; i < 3; ++i) {
        position[i] = float(double(posA[i]) + (double(posB[i])-posA[i])*step);
        scale[i] = float(double(scaleA[i]) + (double(scaleB[i])-scaleA[i])*step);
    }
#endif
}

typedef void (*MIPFullFn)(float,VxMatrix &,const VxMatrix &,const VxMatrix &);
typedef void (*MIPCoreFn)(float,VxVector &,VxVector &,const VxVector &,const VxVector &,const VxVector &,const VxVector &);
struct MIPCase { VxMatrix a,b; VxVector pa,pb,sa,sb; float t; };
static MIPCase MIPData[64];
static VxMatrix MIPOutputs[64];
static VxVector MIPPositions[64],MIPScales[64];
static volatile double MIPSink;
static unsigned MIPRandomState=310;
static unsigned MIPNext() { MIPRandomState=MIPRandomState*1664525u+1013904223u;return MIPRandomState; }
static float MIPRandom() { return float((MIPNext()>>8)*(1.0/16777216.0)); }
static float MIPFinite() {
    unsigned word=MIPNext();if((word&0x7f800000u)==0x7f800000u)word^=0x00800000u;
    float value;std::memcpy(&value,&word,4);return value;
}
static void MIPInit(int kind) {
    for(int n=0;n<64;++n) {
        MIPCase &c=MIPData[n];c.a.SetIdentity();c.b.SetIdentity();c.t=0.05f+0.9f*MIPRandom();
        const float aa=MIPRandom()*2.4f,bb=MIPRandom()*2.4f;
        const double ax=std::cos(aa),ay=std::sin(aa),bx=std::cos(bb),by=std::sin(bb);
        for(int i=0;i<3;++i) {
            c.sa[i]=kind==2?std::ldexp(1.0f,n&1?100:-100):kind==1?0.25f+4*MIPRandom():1;
            c.sb[i]=kind==2?std::ldexp(1.0f,n&1?-100:100):kind==1?0.25f+4*MIPRandom():1;
            c.pa[i]=(MIPRandom()-0.5f)*1000;c.pb[i]=(MIPRandom()-0.5f)*1000;
            c.a[3][i]=c.pa[i];c.b[3][i]=c.pb[i];
        }
        if(kind==1&&n&1)c.sb.z=-c.sb.z;
        c.a[0][0]=float(c.sa.x*ax);c.a[0][1]=float(-c.sa.x*ay);c.a[1][0]=float(c.sa.y*ay);c.a[1][1]=float(c.sa.y*ax);c.a[2][2]=c.sa.z;
        c.b[0][0]=float(c.sb.x*bx);c.b[0][1]=float(-c.sb.x*by);c.b[1][0]=float(c.sb.y*by);c.b[1][1]=float(c.sb.y*bx);c.b[2][2]=c.sb.z;
    }
}
static bool MIPValidate() {
    const MIPFullFn functions[]={Vx3DInterpolateMatrixNoScale,Vx3DInterpolateMatrix};
    for(int kind=0;kind<3;++kind) {
        MIPInit(kind);
        for(int n=0;n<64;++n)for(int entry=0;entry<2;++entry)for(int alias=0;alias<3;++alias) {
            VxMatrix a=MIPData[n].a,b=MIPData[n].b,expected,out;
            const float t=n%8==0?0:n%8==1?1:MIPData[n].t;
            MIPBaselineFull(t,expected,a,b);VxMatrix &dest=alias==1?a:alias==2?b:out;
            functions[entry](t,dest,a,b);if(std::memcmp(&expected,&dest,64))return false;
        }
    }
    for(int n=0;n<32768;++n) {
        VxVector a,b,c,d,expectedP,expectedS,baseP,baseS,currentP,currentS;
        for(int i=0;i<3;++i){a[i]=MIPFinite();b[i]=MIPFinite();c[i]=MIPFinite();d[i]=MIPFinite();}
        const float t=n%8==0?std::ldexp(1.0f,-149):n%8==1?0.5f:n%8==2?1:MIPFinite();
        for(int i=0;i<3;++i) {
            expectedP[i]=float(double(a[i])+(double(b[i])-a[i])*double(t));
            expectedS[i]=float(double(c[i])+(double(d[i])-c[i])*double(t));
        }
        MIPBaselineCore(t,baseP,baseS,a,b,c,d);MIPCurrentCore(t,currentP,currentS,a,b,c,d);
        if(std::memcmp(&baseP,&expectedP,12)||std::memcmp(&baseS,&expectedS,12)||
           std::memcmp(&currentP,&expectedP,12)||std::memcmp(&currentS,&expectedS,12))return false;
    }
    return true;
}
static double MIPTick() {
#if defined(_WIN32)
    LARGE_INTEGER value;QueryPerformanceCounter(&value);return double(value.QuadPart);
#else
    timespec value;clock_gettime(CLOCK_MONOTONIC,&value);return value.tv_sec*1e9+value.tv_nsec;
#endif
}
MIP_NOINLINE static double MIPFullRun(MIPFullFn fn,unsigned iterations,double frequency,bool chain) {
    VxMatrix state=MIPData[0].a;const double start=MIPTick();
    if(chain)for(unsigned i=0;i<iterations;++i){const unsigned n=i&63;fn(MIPData[n].t,state,state,MIPData[n].b);}
    else for(unsigned i=0;i<iterations;++i){const unsigned n=i&63;fn(MIPData[n].t,MIPOutputs[n],MIPData[n].a,MIPData[n].b);}
    const double stop=MIPTick();double sum=0;
    if(chain)for(int i=0;i<4;++i)for(int j=0;j<4;++j)sum+=state[i][j];
    else for(int n=0;n<64;++n)for(int i=0;i<4;++i)for(int j=0;j<4;++j)sum+=MIPOutputs[n][i][j];
    MIPSink=sum;return (stop-start)*1e9/(frequency*iterations);
}
MIP_NOINLINE static double MIPCoreRun(MIPCoreFn fn,unsigned iterations,double frequency,bool chain) {
    VxVector p=MIPData[0].pa,s=MIPData[0].sa;const double start=MIPTick();
    if(chain)for(unsigned i=0;i<iterations;++i){const unsigned n=i&63;fn(MIPData[n].t,p,s,p,MIPData[n].pb,s,MIPData[n].sb);}
    else for(unsigned i=0;i<iterations;++i){const unsigned n=i&63;const MIPCase &c=MIPData[n];fn(c.t,MIPPositions[n],MIPScales[n],c.pa,c.pb,c.sa,c.sb);}
    const double stop=MIPTick();double sum=0;
    if(chain)for(int i=0;i<3;++i)sum+=double(p[i])+s[i];
    else for(int n=0;n<64;++n)for(int i=0;i<3;++i)sum+=double(MIPPositions[n][i])+MIPScales[n][i];
    MIPSink=sum;return (stop-start)*1e9/(frequency*iterations);
}
int main(int argc,char **argv) {
    const int samples=argc>1?std::atoi(argv[1]):15;if(samples<3)return 2;
#if defined(_WIN32)
    DWORD_PTR allowed,system;if(!GetProcessAffinityMask(GetCurrentProcess(),&allowed,&system)||!allowed)return 10;
    const DWORD_PTR selected=(allowed&4)?4:allowed&(~allowed+1),old=SetThreadAffinityMask(GetCurrentThread(),selected);if(!old)return 11;
    const int oldPriority=GetThreadPriority(GetCurrentThread());if(!SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_HIGHEST))return 12;
    LARGE_INTEGER counter;QueryPerformanceFrequency(&counter);const double frequency=double(counter.QuadPart);
    std::printf("{\"type\":\"environment\",\"pointer_bits\":%u,\"affinity\":%llu,\"priority_before\":%d,\"priority_active\":%d,\"qpc\":%.0f}\n",unsigned(8*sizeof(void *)),(unsigned long long)selected,oldPriority,GetThreadPriority(GetCurrentThread()),frequency);
#else
    const double frequency=1e9;
    std::printf("{\"type\":\"environment\",\"pointer_bits\":%u,\"qpc\":1000000000}\n",unsigned(8*sizeof(void *)));
#endif
    if(!MIPValidate())return 20;
    std::puts("{\"type\":\"validation\",\"full_queries\":1152,\"core_queries\":32768,\"core_components\":6,\"errors\":0}");
    const char *kinds[]={"rigid","scaled","wide"};
    for(int kind=0;kind<3;++kind) {
        MIPInit(kind);
        for(int scope=0;scope<2;++scope)for(int chain=0;chain<2;++chain) {
            const unsigned iterations=scope?2097152:262144;
            for(int warm=0;warm<2;++warm) {
                if(scope){MIPCoreRun(MIPBaselineCore,16384,frequency,chain!=0);MIPCoreRun(MIPCurrentCore,16384,frequency,chain!=0);}
                else{MIPFullRun(MIPBaselineFull,16384,frequency,chain!=0);MIPFullRun(Vx3DInterpolateMatrixNoScale,16384,frequency,chain!=0);}
            }
            for(int sample=0;sample<samples;++sample) {
                double values[4];
                for(int run=0;run<4;++run) {
                    const bool baseline=(sample&1)?run==1||run==2:run==0||run==3;
                    values[run]=scope?MIPCoreRun(baseline?MIPBaselineCore:MIPCurrentCore,iterations,frequency,chain!=0):
                        MIPFullRun(baseline?MIPBaselineFull:Vx3DInterpolateMatrixNoScale,iterations,frequency,chain!=0);
                }
                const double b0=values[sample&1?1:0],b1=values[sample&1?2:3],c0=values[sample&1?0:1],c1=values[sample&1?3:2];
                std::printf("{\"type\":\"sample\",\"scope\":\"%s\",\"chain\":%s,\"kind\":\"%s\",\"sample\":%d,\"iterations\":%u,\"base0\":%.9f,\"base1\":%.9f,\"current0\":%.9f,\"current1\":%.9f}\n",scope?"core":"full",chain?"true":"false",kinds[kind],sample,iterations,b0,b1,c0,c1);
            }
        }
    }
#if defined(_WIN32)
    const bool restored=SetThreadPriority(GetCurrentThread(),oldPriority)!=0&&SetThreadAffinityMask(GetCurrentThread(),old)!=0;
    std::printf("{\"type\":\"finish\",\"restored\":%s,\"priority_after\":%d,\"sink\":%.17g}\n",restored?"true":"false",GetThreadPriority(GetCurrentThread()),MIPSink);return restored?0:30;
#else
    std::printf("{\"type\":\"finish\",\"sink\":%.17g}\n",MIPSink);return 0;
#endif
}
