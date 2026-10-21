#include <cmath>
#include <cfloat>
#include <cstddef>
#include <new>
#include <initializer_list>
#ifndef VX_AABB_OBB_NATIVE_PROBE
#ifndef VX_LIB
#define VX_LIB 1
#endif
#ifndef VX_AABB_OBB_IMPLEMENTATION
#define VX_AABB_OBB_IMPLEMENTATION "../../src/VxIntersect_BoxBox.cpp"
#endif
#include VX_AABB_OBB_IMPLEMENTATION
#include <gtest/gtest.h>
void VxIntersectPlaneDispatchRebuild(bool) { FAIL() << "Unrelated plane rebuild"; }
void VxIntersectSphereDispatchRebuild(bool) { FAIL() << "Unrelated sphere rebuild"; }
void VxIntersectFrustumDispatchRebuild(bool) { FAIL() << "Unrelated frustum rebuild"; }
void VxIntersectFaceDispatchRebuild(bool) { FAIL() << "Unrelated face rebuild"; }
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef int (*AOFunction)(const VxBbox &, const VxOBB &);
struct AOResults { int samples = 0, errors = 0; unsigned digest = 2166136261u; };
static void AOObserve(AOResults &r, int actual, bool expected, bool fixtureGood = true) {
    ++r.samples; if (actual != int(expected) || !fixtureGood) ++r.errors;
    r.digest = (r.digest ^ unsigned(actual)) * 16777619u;
}
static void AOAxes(VxOBB &b, int frame) {
    b.m_Axis[0] = VxVector(1, 0, 0); b.m_Axis[1] = VxVector(0, 1, 0); b.m_Axis[2] = VxVector(0, 0, 1);
    if (frame == 1) { b.m_Axis[0] = VxVector(.6f, .8f, 0); b.m_Axis[1] = VxVector(-.8f, .6f, 0); }
    if (frame == 2) { b.m_Axis[1] = VxVector(0, .6f, .8f); b.m_Axis[2] = VxVector(0, -.8f, .6f); }
    if (frame == 3) { b.m_Axis[0] = VxVector(0, 0, -1); b.m_Axis[1] = VxVector(1, 0, 0); b.m_Axis[2] = VxVector(0, -1, 0); }
    if (frame == 4) { b.m_Axis[0] = VxVector(1.0f/3, 2.0f/3, 2.0f/3); b.m_Axis[1] = VxVector(-2.0f/3, 2.0f/3, -1.0f/3); b.m_Axis[2] = VxVector(-2.0f/3, -1.0f/3, 2.0f/3); }
}
static VxOBB AOMake(int frame, const VxVector &center, const VxVector &extent) {
    VxOBB b; AOAxes(b, frame); b.m_Center = center; b.m_Extents = extent; return b;
}
// Project all eight vertices, independently of the optimized center/radius
// inequalities. Bits 0..5 are the face axes; 6+i*3+j is world_i cross OBB_j.
static unsigned AOSeparated(const VxBbox &a, const VxOBB &b) {
    double av[8][3], bv[8][3], axes[15][3] = {};
    for (int i = 0; i < 3; ++i) { axes[i][i] = 1; for (int j = 0; j < 3; ++j) axes[3+i][j] = b.m_Axis[i][j]; }
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) {
        axes[6+i*3+j][(i+1)%3] = -b.m_Axis[j][(i+2)%3];
        axes[6+i*3+j][(i+2)%3] = b.m_Axis[j][(i+1)%3];
    }
    for (int n = 0; n < 8; ++n) for (int i = 0; i < 3; ++i) {
        av[n][i] = n & (1 << i) ? a.Max[i] : a.Min[i];
        bv[n][i] = b.m_Center[i];
        for (int j = 0; j < 3; ++j) bv[n][i] += (n & (1 << j) ? 1 : -1) * double(b.m_Extents[j]) * b.m_Axis[j][i];
    }
    unsigned mask = 0;
    for (int axis = 0; axis < 15; ++axis) {
        double loA = DBL_MAX, hiA = -DBL_MAX, loB = DBL_MAX, hiB = -DBL_MAX;
        for (int n = 0; n < 8; ++n) {
            double pa = 0, pb = 0;
            for (int i = 0; i < 3; ++i) { pa += av[n][i] * axes[axis][i]; pb += bv[n][i] * axes[axis][i]; }
            if (pa < loA) loA = pa; if (pa > hiA) hiA = pa;
            if (pb < loB) loB = pb; if (pb > hiB) hiB = pb;
        }
        if (hiA < loB || hiB < loA) mask |= 1u << axis;
    }
    return mask;
}
static AOResults AOGrid(AOFunction fn) {
    AOResults r;
    const VxBbox a(VxVector(-1.25f, -.75f, -.5f), VxVector(.75f, 1.75f, 1.5f));
    const VxVector extents[] = {VxVector(.5f, .75f, 1.25f), VxVector(1, 0, 1), VxVector(.25f, 1.5f, .375f)};
    for (int frame = 0; frame < 5; ++frame) for (const VxVector &extent : extents)
        for (int x = -4; x <= 4; ++x) for (int y = -4; y <= 4; ++y) for (int z = -4; z <= 4; ++z) {
            const VxOBB b = AOMake(frame, VxVector(x*.5f+.125f, y*.5f+.0625f, z*.5f+.03125f), extent);
            AOObserve(r, fn(a,b), AOSeparated(a,b) == 0);
        }
    return r;
}
static AOResults AOScales(AOFunction fn) {
    AOResults r;
    const int powers[] = {-120, -90, -40, -10, 0, 10, 40, 90, 120};
    const VxVector centers[] = {VxVector(0,0,0), VxVector(.5f,0,0), VxVector(3,2,0), VxVector(-2.5f,1,.5f), VxVector(1.5f,0,0)};
    for (int power : powers) for (int frame : {0,1,2,4}) for (const VxVector &center : centers) {
        const float s = std::ldexp(1.0f,power);
        const VxBbox a(VxVector(-s,-s,-s), VxVector(s,s,s));
        const VxOBB b = AOMake(frame,center*s,VxVector(.5f,.75f,.25f)*s);
        AOObserve(r, fn(a,b), AOSeparated(a,b) == 0);
    }
    return r;
}
static AOResults AOWide(AOFunction fn) {
    AOResults r;
    const VxBbox same(VxVector(FLT_MAX,0,0), VxVector(FLT_MAX,0,0));
    VxOBB b = AOMake(0,VxVector(FLT_MAX,0,0),VxVector(1,1,1));
    AOObserve(r,fn(same,b),true);
    const VxBbox wide(VxVector(-FLT_MAX,-1,-1),VxVector(FLT_MAX,1,1));
    b = AOMake(2,VxVector(0,1.5f,1.5f),VxVector(.5f,.1f,1));
    AOObserve(r,fn(wide,b),false);
    const VxBbox positive(VxVector(FLT_MAX/2,-1,-1),VxVector(FLT_MAX,1,1));
    b = AOMake(0,VxVector(FLT_MAX*.75f,0,0),VxVector(FLT_MAX*.125f,1,1));
    AOObserve(r,fn(positive,b),true);
    const VxBbox huge(VxVector(-FLT_MAX,-FLT_MAX,-FLT_MAX),VxVector(FLT_MAX,FLT_MAX,FLT_MAX));
    b = AOMake(4,VxVector(0,0,0),VxVector(1,1,1)); AOObserve(r,fn(huge,b),true);
    const VxBbox opposite(VxVector(-FLT_MAX,0,0),VxVector(-FLT_MAX,0,0));
    b = AOMake(0,VxVector(FLT_MAX,0,0),VxVector(FLT_MAX,1,1)); AOObserve(r,fn(opposite,b),false);
    const VxBbox zero(VxVector(0,0,0),VxVector(0,0,0)); AOObserve(r,fn(zero,b),true);
    return r;
}
static AOResults AOReparam(AOFunction fn) {
    AOResults r;
    const int permutations[][3] = {{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
    const VxBbox a(VxVector(-1,-1,-1),VxVector(1,1,1));
    for (int x = -2; x <= 2; ++x) {
        const VxOBB original = AOMake(4,VxVector(x*.75f,.125f,.0625f),VxVector(.5f,1,.25f));
        const bool expected = AOSeparated(a,original) == 0;
        for (const auto &p : permutations) for (int signs = 0; signs < 8; ++signs) {
            VxOBB b = original;
            for (int j = 0; j < 3; ++j) { b.m_Axis[j] = original.m_Axis[p[j]] * (signs & (1<<j) ? -1.0f : 1.0f); b.m_Extents[j] = original.m_Extents[p[j]]; }
            AOObserve(r,fn(a,b),expected,(AOSeparated(a,b) == 0) == expected);
        }
    }
    return r;
}
static AOResults AOCross(AOFunction fn) {
    AOResults r;
    struct Witness { float half[3], extent[3], center[3]; };
    const Witness witnesses[] = {
        {{1.32246017f,.346165568f,.247123718f},{.809705973f,1.13822019f,.372079164f},{.475349993f,1.42321873f,-.823504746f}},
        {{1.72567546f,1.90439534f,.270427823f},{.800524354f,1.10806823f,1.21346629f},{-1.63496768f,1.71368587f,1.82856202f}},
        {{.636725724f,.472693324f,1.70765722f},{1.33098578f,1.86653137f,1.50000918f},{1.31743038f,-2.61965871f,-2.51015925f}},
        {{.132417083f,1.27487493f,.504240155f},{1.35700679f,1.50369632f,1.26316178f},{1.78927267f,-.959698856f,-1.41886401f}},
        {{.809215188f,1.83437335f,.615829527f},{1.61307895f,1.9690491f,.244283348f},{1.46175849f,-2.49411535f,-1.67924333f}},
        {{1.91852105f,1.70456147f,.841339052f},{1.52819014f,.254195154f,.817168474f},{-3.03287435f,-.370337337f,-1.71962762f}},
        {{1.64994788f,1.4963727f,1.15988028f},{1.86770546f,.572196901f,.887714565f},{-2.22446752f,2.52447796f,1.4476862f}},
        {{.707847118f,1.7616899f,1.75194132f},{.183785483f,.478018999f,.802171767f},{1.48008454f,2.20060015f,.387110889f}},
        {{1.65909827f,.510867298f,1.52509928f},{.32814002f,1.47577584f,1.43973422f},{-3.06649542f,1.67206144f,-.556392968f}}
    };
    for (int n = 0; n < 9; ++n) {
        const Witness &w = witnesses[n]; const VxVector half(w.half[0],w.half[1],w.half[2]);
        const VxBbox a(-half,half); const VxOBB b = AOMake(4,VxVector(w.center[0],w.center[1],w.center[2]),VxVector(w.extent[0],w.extent[1],w.extent[2]));
        AOObserve(r,fn(a,b),false,AOSeparated(a,b) == (1u << (6+n)));
    }
    return r;
}
#if defined(_WIN32)
static AOResults AOFootprint(AOFunction fn) {
    AOResults r; SYSTEM_INFO info; GetSystemInfo(&info);
    unsigned char *pages = static_cast<unsigned char *>(VirtualAlloc(nullptr,info.dwPageSize*4,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if (!pages) { AOObserve(r,0,true); return r; }
    VxBbox *a = new(pages+info.dwPageSize-sizeof(VxBbox)) VxBbox(VxVector(-1,-1,-1),VxVector(1,1,1));
    VxOBB *b = new(pages+info.dwPageSize*3-sizeof(VxOBB)) VxOBB;
    *b = AOMake(1,VxVector(.125f,.25f,.5f),VxVector(.5f,.75f,.25f));
    DWORD old;
    if (VirtualProtect(pages+info.dwPageSize,info.dwPageSize,PAGE_NOACCESS,&old) && VirtualProtect(pages+info.dwPageSize*3,info.dwPageSize,PAGE_NOACCESS,&old) &&
        VirtualProtect(pages,info.dwPageSize,PAGE_READONLY,&old) && VirtualProtect(pages+info.dwPageSize*2,info.dwPageSize,PAGE_READONLY,&old))
        AOObserve(r,fn(*a,*b),true);
    else AOObserve(r,0,true);
    VirtualFree(pages,0,MEM_RELEASE); return r;
}
#endif
#ifndef VX_AABB_OBB_NATIVE_PROBE
static void AOCheck(AOResults r) { EXPECT_EQ(r.errors,0); ::testing::Test::RecordProperty("samples",r.samples); ::testing::Test::RecordProperty("digest",std::to_string(r.digest)); }
TEST(AabbObbIntersectionReference, ScalarVertexProjectionGrid) { AOCheck(AOGrid(AABBOBBCore<false>)); }
TEST(AabbObbIntersectionReference, ScalarEquivalentScales) { AOCheck(AOScales(AABBOBBCore<false>)); }
TEST(AabbObbIntersectionReference, ScalarFiniteExtremeCentersAndHalfSizes) { AOCheck(AOWide(AABBOBBCore<false>)); }
TEST(AabbObbIntersectionReference, ScalarAxisPermutationsAndSigns) { AOCheck(AOReparam(AABBOBBCore<false>)); }
TEST(AabbObbIntersectionReference, ScalarAllNineCrossAxesNecessary) { AOCheck(AOCross(AABBOBBCore<false>)); }
#if defined(VX_SIMD_SSE)
TEST(AabbObbIntersectionReference, SimdVertexProjectionGrid) { AOCheck(AOGrid(AABBOBBCore<true>)); }
TEST(AabbObbIntersectionReference, SimdEquivalentScales) { AOCheck(AOScales(AABBOBBCore<true>)); }
TEST(AabbObbIntersectionReference, SimdFiniteExtremeCentersAndHalfSizes) { AOCheck(AOWide(AABBOBBCore<true>)); }
TEST(AabbObbIntersectionReference, SimdAxisPermutationsAndSigns) { AOCheck(AOReparam(AABBOBBCore<true>)); }
TEST(AabbObbIntersectionReference, SimdAllNineCrossAxesNecessary) { AOCheck(AOCross(AABBOBBCore<true>)); }
#endif
TEST(AabbObbIntersectionReference, PublicForwarderUsesSelectedBoxTable) {
    const VxIntersectBoxDispatchTable *saved = GetVxIntersectBoxDispatchTable();
    for (int n=0;n<4;++n) {
        const VxIntersectBoxDispatchTable *table=&kVxIntersectBoxDispatchScalar;
#if defined(VX_SIMD_SSE)
        if (n&1) table=&kVxIntersectBoxDispatchSIMD;
#endif
        g_VxIntersectBoxDispatch.store(table,std::memory_order_release);
        AOCheck(AOCross(VxIntersect::AABBOBB)); EXPECT_EQ(AOWide(VxIntersect::AABBOBB).errors,0);
    }
    g_VxIntersectBoxDispatch.store(saved,std::memory_order_release);
}
#if defined(_WIN32)
TEST(AabbObbIntersectionReference, ScalarReadonlyExactFootprints) { AOCheck(AOFootprint(AABBOBBCore<false>)); }
#if defined(VX_SIMD_SSE)
TEST(AabbObbIntersectionReference, SimdReadonlyExactFootprints) { AOCheck(AOFootprint(AABBOBBCore<true>)); }
#endif
TEST(AabbObbIntersectionReference, ActualCurrentDllExport) {
    HMODULE dll=LoadLibraryA("VxMath.dll"); ASSERT_NE(dll,nullptr);
#if defined(_WIN64)
    const char *symbol="?AABBOBB@VxIntersect@@SAHAEBUVxBbox@@AEBVVxOBB@@@Z";
#else
    const char *symbol="?AABBOBB@VxIntersect@@SAHABUVxBbox@@ABVVxOBB@@@Z";
#endif
    AOFunction fn=reinterpret_cast<AOFunction>(GetProcAddress(dll,symbol)); ASSERT_NE(fn,nullptr);
    AOCheck(AOGrid(fn)); EXPECT_EQ(AOScales(fn).errors,0); EXPECT_EQ(AOWide(fn).errors,0); EXPECT_EQ(AOReparam(fn).errors,0); EXPECT_EQ(AOCross(fn).errors,0); EXPECT_EQ(AOFootprint(fn).errors,0);
    EXPECT_NE(FreeLibrary(dll),0);
}
#endif
#endif
