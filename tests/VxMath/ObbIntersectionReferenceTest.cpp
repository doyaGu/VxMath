#ifndef VX_OBB_OBB_NATIVE_PROBE
#ifndef VX_LIB
#define VX_LIB 1
#endif
#ifndef VX_OBB_OBB_IMPLEMENTATION
#define VX_OBB_OBB_IMPLEMENTATION "../../src/VxIntersect_BoxBox.cpp"
#endif
#include VX_OBB_OBB_IMPLEMENTATION
#include <gtest/gtest.h>
void VxIntersectPlaneDispatchRebuild(bool) { FAIL() << "Unrelated plane rebuild"; }
void VxIntersectSphereDispatchRebuild(bool) { FAIL() << "Unrelated sphere rebuild"; }
void VxIntersectFrustumDispatchRebuild(bool) { FAIL() << "Unrelated frustum rebuild"; }
void VxIntersectFaceDispatchRebuild(bool) { FAIL() << "Unrelated face rebuild"; }
#endif
// Reuse only synthetic fixtures and their independent vertex oracle, without
// compiling that test's production implementation or registering its tests.
#define VX_AABB_OBB_NATIVE_PROBE
#include "AabbObbIntersectionReferenceTest.cpp"
#undef VX_AABB_OBB_NATIVE_PROBE

typedef int (*OOFunction)(const VxOBB &, const VxOBB &);
static unsigned OOSeparated(const VxOBB &a, const VxOBB &b) {
    double av[8][3], bv[8][3], axes[15][3];
    for (int i=0;i<3;++i) for (int j=0;j<3;++j) { axes[i][j]=a.m_Axis[i][j]; axes[3+i][j]=b.m_Axis[i][j]; }
    for (int i=0;i<3;++i) for (int j=0;j<3;++j) for (int k=0;k<3;++k)
        axes[6+i*3+j][k]=double(a.m_Axis[i][(k+1)%3])*b.m_Axis[j][(k+2)%3]-double(a.m_Axis[i][(k+2)%3])*b.m_Axis[j][(k+1)%3];
    // Translate the entire fixture to a's center before constructing vertices.
    // This avoids destroying small extents by adding a huge common origin.
    for (int n=0;n<8;++n) for (int i=0;i<3;++i) {
        av[n][i]=0; bv[n][i]=double(b.m_Center[i])-a.m_Center[i];
        for (int j=0;j<3;++j) {
            const int sign=n&(1<<j)?1:-1;
            av[n][i]+=sign*double(a.m_Extents[j])*a.m_Axis[j][i];
            bv[n][i]+=sign*double(b.m_Extents[j])*b.m_Axis[j][i];
        }
    }
    unsigned mask=0;
    for (int axis=0;axis<15;++axis) {
        double loA=DBL_MAX,hiA=-DBL_MAX,loB=DBL_MAX,hiB=-DBL_MAX;
        for (int n=0;n<8;++n) {
            double pa=0,pb=0;
            for (int i=0;i<3;++i) { pa+=av[n][i]*axes[axis][i]; pb+=bv[n][i]*axes[axis][i]; }
            if (pa<loA) loA=pa; if (pa>hiA) hiA=pa;
            if (pb<loB) loB=pb; if (pb>hiB) hiB=pb;
        }
        if (hiA<loB || hiB<loA) mask|=1u<<axis;
    }
    return mask;
}
static AOResults OOGrid(OOFunction fn) {
    AOResults r; const VxVector extents[]={VxVector(.5f,.75f,1.25f),VxVector(1,0,1),VxVector(.25f,1.5f,.375f)};
    for (int first=0;first<5;++first) for (int second=0;second<5;++second) for (const VxVector &extent:extents)
        for (int x=-3;x<=3;++x) for (int y=-3;y<=3;++y) for (int z=-3;z<=3;++z) {
            const VxOBB a=AOMake(first,VxVector(.375f,-.125f,.25f),VxVector(.75f,1,.5f));
            const VxOBB b=AOMake(second,VxVector(x*.75f+.125f,y*.75f+.0625f,z*.75f+.03125f),extent);
            const bool expected=OOSeparated(a,b)==0;
            AOObserve(r,fn(a,b),expected); AOObserve(r,fn(b,a),expected);
        }
    return r;
}
static AOResults OOScales(OOFunction fn) {
    AOResults r; const int powers[]={-120,-90,-40,-10,0,10,40,90,120};
    const VxVector centers[]={VxVector(0,0,0),VxVector(.5f,0,0),VxVector(3,2,0),VxVector(-2.5f,1,.5f),VxVector(1.25f,0,0)};
    for (int power:powers) for (int frame=0;frame<5;++frame) for (const VxVector &center:centers) {
        const float s=std::ldexp(1.0f,power);
        const VxOBB a=AOMake(frame,VxVector(0,0,0),VxVector(.75f,.5f,1)*s);
        const VxOBB b=AOMake((frame+1)%5,center*s,VxVector(.5f,.75f,.25f)*s);
        const bool expected=OOSeparated(a,b)==0;
        AOObserve(r,fn(a,b),expected); AOObserve(r,fn(b,a),expected);
    }
    return r;
}
static AOResults OOWide(OOFunction fn) {
    AOResults r;
    VxOBB a=AOMake(0,VxVector(-FLT_MAX,-FLT_MAX,0),VxVector(1,1,1));
    VxOBB b=AOMake(0,VxVector(FLT_MAX,FLT_MAX,0),VxVector(1,1,1));
    AOObserve(r,fn(a,b),false); AOObserve(r,fn(b,a),false);
    a.m_Center.z=-FLT_MAX; b.m_Center.z=FLT_MAX;
    AOObserve(r,fn(a,b),false); AOObserve(r,fn(b,a),false);
    a=AOMake(0,VxVector(-FLT_MAX,0,0),VxVector(FLT_MAX,1,1));
    b=AOMake(0,VxVector(FLT_MAX,0,0),VxVector(FLT_MAX,1,1));
    AOObserve(r,fn(a,b),true); AOObserve(r,fn(b,a),true);
    a.m_Extents.x=b.m_Extents.x=FLT_MAX*.75f;
    AOObserve(r,fn(a,b),false); AOObserve(r,fn(b,a),false);
    a=AOMake(1,VxVector(FLT_MAX,FLT_MAX,FLT_MAX),VxVector(1,1,1));
    b=AOMake(4,a.m_Center,VxVector(.5f,.75f,1));
    AOObserve(r,fn(a,b),true); AOObserve(r,fn(b,a),true);
    a=AOMake(0,VxVector(0,0,0),VxVector(FLT_MAX,FLT_MAX,FLT_MAX));
    b=AOMake(0,VxVector(FLT_MAX,FLT_MAX,FLT_MAX),VxVector(0,0,0));
    AOObserve(r,fn(a,b),true); AOObserve(r,fn(b,a),true);
    return r;
}
static AOResults OOContacts(OOFunction fn) {
    AOResults r; const VxOBB a=AOMake(0,VxVector(0,0,0),VxVector(1,1,1));
    for (int axis=0;axis<3;++axis) for (int side=-1;side<=1;side+=2) for (int neighbor=-1;neighbor<=1;++neighbor) {
        VxVector center(0,0,0); const float boundary=side*1.5f;
        center[axis]=neighbor==0?boundary:std::nextafter(boundary,neighbor<0?-FLT_MAX:FLT_MAX);
        const VxOBB b=AOMake(0,center,VxVector(.5f,.5f,.5f));
        const bool expected=neighbor==0 || (side<0?neighbor>0:neighbor<0);
        AOObserve(r,fn(a,b),expected); AOObserve(r,fn(b,a),expected);
    }
    const VxOBB thin=AOMake(0,VxVector(0,0,0),VxVector(1,0,1));
    for (int off=0;off<2;++off) {
        const VxOBB point=AOMake(0,VxVector(0,off?FLT_MIN:0,0),VxVector(0,0,0));
        AOObserve(r,fn(thin,point),off==0); AOObserve(r,fn(point,thin),off==0);
    }
    for (int frame=0;frame<5;++frame) {
        const VxOBB point=AOMake(frame,VxVector(-0.0f,0,-0.0f),VxVector(0,0,0));
        const VxOBB full=AOMake(frame,VxVector(0,0,0),VxVector(1,1,1));
        AOObserve(r,fn(point,point),true); AOObserve(r,fn(full,full),true);
    }
    return r;
}
static AOResults OOReparam(OOFunction fn) {
    AOResults r; const int permutations[][3]={{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
    for (int x=-2;x<=2;++x) {
        const VxOBB originalA=AOMake(1,VxVector(0,0,0),VxVector(.75f,.5f,1));
        const VxOBB originalB=AOMake(4,VxVector(x*.75f,.125f,.0625f),VxVector(.5f,1,.25f));
        const bool expected=OOSeparated(originalA,originalB)==0;
        for (int p=0;p<6;++p) for (int signs=0;signs<8;++signs) {
            VxOBB a=originalA,b=originalB;
            for (int j=0;j<3;++j) {
                const int ai=permutations[p][j],bi=permutations[(p+2)%6][j];
                a.m_Axis[j]=originalA.m_Axis[ai]*(signs&(1<<j)?-1.0f:1.0f); a.m_Extents[j]=originalA.m_Extents[ai];
                b.m_Axis[j]=originalB.m_Axis[bi]*(signs&(1<<j)?1.0f:-1.0f); b.m_Extents[j]=originalB.m_Extents[bi];
            }
            const bool fixtureGood=(OOSeparated(a,b)==0)==expected;
            AOObserve(r,fn(a,b),expected,fixtureGood); AOObserve(r,fn(b,a),expected,fixtureGood);
        }
    }
    return r;
}
// Rotate the existing nine independent cross-only witnesses by an exact signed
// permutation. Both boxes now have non-world axes; verify the sole separating
// axis again using the general OBB vertex oracle, and call both argument orders.
static OOFunction OOCrossTarget;
static int OOCrossIndex;
static bool OOCrossGood;
static int OOCrossBridge(const VxBbox &box,const VxOBB &input) {
    const VxOBB a=AOMake(3,VxVector(0,0,0),box.Max);
    VxOBB b=input; b.m_Center=VxVector(input.m_Center.y,-input.m_Center.z,-input.m_Center.x);
    for (int i=0;i<3;++i) b.m_Axis[i]=VxVector(input.m_Axis[i].y,-input.m_Axis[i].z,-input.m_Axis[i].x);
    if (OOSeparated(a,b)!=(1u<<(6+OOCrossIndex))) OOCrossGood=false;
    ++OOCrossIndex;
    const int result=OOCrossTarget(a,b); if (OOCrossTarget(b,a)!=0) OOCrossGood=false;
    return result;
}
static AOResults OOCross(OOFunction fn) {
    const OOFunction saved=OOCrossTarget; OOCrossTarget=fn; OOCrossIndex=0; OOCrossGood=true;
    AOResults r=AOCross(OOCrossBridge); if (OOCrossIndex!=9 || !OOCrossGood) ++r.errors;
    OOCrossTarget=saved; return r;
}
#if defined(_WIN32)
static AOResults OOFootprint(OOFunction fn) {
    AOResults r; SYSTEM_INFO info; GetSystemInfo(&info);
    unsigned char *pages=static_cast<unsigned char *>(VirtualAlloc(nullptr,info.dwPageSize*4,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if (!pages) { AOObserve(r,0,true); return r; }
    VxOBB *a=new(pages+info.dwPageSize-sizeof(VxOBB)) VxOBB;
    VxOBB *b=new(pages+info.dwPageSize*3-sizeof(VxOBB)) VxOBB;
    *a=AOMake(1,VxVector(0,0,0),VxVector(1,1,1)); *b=AOMake(4,VxVector(.125f,.25f,.5f),VxVector(.5f,.75f,.25f));
    DWORD old;
    if (VirtualProtect(pages+info.dwPageSize,info.dwPageSize,PAGE_NOACCESS,&old) && VirtualProtect(pages+info.dwPageSize*3,info.dwPageSize,PAGE_NOACCESS,&old) &&
        VirtualProtect(pages,info.dwPageSize,PAGE_READONLY,&old) && VirtualProtect(pages+info.dwPageSize*2,info.dwPageSize,PAGE_READONLY,&old)) {
        AOObserve(r,fn(*a,*b),true); AOObserve(r,fn(*b,*a),true); AOObserve(r,fn(*b,*b),true);
    } else AOObserve(r,0,true);
    VirtualFree(pages,0,MEM_RELEASE); return r;
}
#endif
#ifndef VX_OBB_OBB_NATIVE_PROBE
static void OOCheck(AOResults r) { EXPECT_EQ(r.errors,0); ::testing::Test::RecordProperty("samples",r.samples); ::testing::Test::RecordProperty("digest",std::to_string(r.digest)); }
TEST(ObbIntersectionReference, ScalarVertexProjectionAndSymmetry) { OOCheck(OOGrid(OBBOBBCore<false>)); }
TEST(ObbIntersectionReference, ScalarEquivalentScales) { OOCheck(OOScales(OBBOBBCore<false>)); }
TEST(ObbIntersectionReference, ScalarFiniteExtremeDisplacement) { OOCheck(OOWide(OBBOBBCore<false>)); }
TEST(ObbIntersectionReference, ScalarClosedContactAndDegeneracy) { OOCheck(OOContacts(OBBOBBCore<false>)); }
TEST(ObbIntersectionReference, ScalarBothFramesReparametrized) { OOCheck(OOReparam(OBBOBBCore<false>)); }
TEST(ObbIntersectionReference, ScalarAllNineCrossAxes) { OOCheck(OOCross(OBBOBBCore<false>)); }
#if defined(VX_SIMD_SSE)
TEST(ObbIntersectionReference, SimdVertexProjectionAndSymmetry) { OOCheck(OOGrid(OBBOBBCore<true>)); }
TEST(ObbIntersectionReference, SimdEquivalentScales) { OOCheck(OOScales(OBBOBBCore<true>)); }
TEST(ObbIntersectionReference, SimdFiniteExtremeDisplacement) { OOCheck(OOWide(OBBOBBCore<true>)); }
TEST(ObbIntersectionReference, SimdClosedContactAndDegeneracy) { OOCheck(OOContacts(OBBOBBCore<true>)); }
TEST(ObbIntersectionReference, SimdBothFramesReparametrized) { OOCheck(OOReparam(OBBOBBCore<true>)); }
TEST(ObbIntersectionReference, SimdAllNineCrossAxes) { OOCheck(OOCross(OBBOBBCore<true>)); }
#endif
TEST(ObbIntersectionReference, PublicForwarderUsesSelectedBoxTable) {
    const VxIntersectBoxDispatchTable *saved=GetVxIntersectBoxDispatchTable();
    for (int n=0;n<4;++n) {
        const VxIntersectBoxDispatchTable *table=&kVxIntersectBoxDispatchScalar;
#if defined(VX_SIMD_SSE)
        if (n&1) table=&kVxIntersectBoxDispatchSIMD;
#endif
        g_VxIntersectBoxDispatch.store(table,std::memory_order_release);
        OOCheck(OOCross(VxIntersect::OBBOBB)); EXPECT_EQ(OOWide(VxIntersect::OBBOBB).errors,0);
    }
    g_VxIntersectBoxDispatch.store(saved,std::memory_order_release);
}
#if defined(_WIN32)
TEST(ObbIntersectionReference, ScalarReadonlyExactFootprints) { OOCheck(OOFootprint(OBBOBBCore<false>)); }
#if defined(VX_SIMD_SSE)
TEST(ObbIntersectionReference, SimdReadonlyExactFootprints) { OOCheck(OOFootprint(OBBOBBCore<true>)); }
#endif
TEST(ObbIntersectionReference, ActualCurrentDllExport) {
    HMODULE dll=LoadLibraryA("VxMath.dll"); ASSERT_NE(dll,nullptr);
#if defined(_WIN64)
    const char *symbol="?OBBOBB@VxIntersect@@SAHAEBVVxOBB@@0@Z";
#else
    const char *symbol="?OBBOBB@VxIntersect@@SAHABVVxOBB@@0@Z";
#endif
    OOFunction fn=reinterpret_cast<OOFunction>(GetProcAddress(dll,symbol)); ASSERT_NE(fn,nullptr);
    OOCheck(OOGrid(fn)); EXPECT_EQ(OOScales(fn).errors,0); EXPECT_EQ(OOWide(fn).errors,0); EXPECT_EQ(OOContacts(fn).errors,0); EXPECT_EQ(OOReparam(fn).errors,0); EXPECT_EQ(OOCross(fn).errors,0); EXPECT_EQ(OOFootprint(fn).errors,0);
    EXPECT_NE(FreeLibrary(dll),0);
}
#endif
#endif
