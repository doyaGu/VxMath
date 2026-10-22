#include <cmath>
#include <cfloat>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <new>
#include <initializer_list>
#ifndef VX_LINE_FACE_NATIVE_PROBE
#ifndef VX_LIB
#define VX_LIB 1
#endif
#include "VxRay.h"
#ifndef VX_LINE_FACE_IMPLEMENTATION
#define VX_LINE_FACE_IMPLEMENTATION "../../src/VxIntersect_FaceFace.cpp"
#endif
#include VX_LINE_FACE_IMPLEMENTATION
#include <gtest/gtest.h>
void VxIntersectPlaneDispatchRebuild(bool) { FAIL() << "Unrelated plane rebuild"; }
void VxIntersectSphereDispatchRebuild(bool) { FAIL() << "Unrelated sphere rebuild"; }
void VxIntersectFrustumDispatchRebuild(bool) { FAIL() << "Unrelated frustum rebuild"; }
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef int (*LFBasicFn)(const VxRay &, const VxVector &, const VxVector &, const VxVector &, const VxVector &, VxVector &, float &);
typedef int (*LFAxesFn)(const VxRay &, const VxVector &, const VxVector &, const VxVector &, const VxVector &, VxVector &, float &, int &, int &);
struct LFEntries { LFBasicFn basic; LFAxesFn axes; };
struct LFCase {
    VxRay ray;
    VxVector a, b, c, normal, hit;
    float time;
    int code, stage, first, second;
    bool alias = false;
};
struct LFSamples { int samples = 0, errors = 0, codeErrors = 0, pointErrors = 0, timeErrors = 0, axisErrors = 0, inputErrors = 0; unsigned digest = 2166136261u; };
static VxVector LFProject(float x, float y, float plane, int drop) {
    VxVector p(0, 0, 0); p[drop == 0 ? 1 : 0] = x; p[drop == 2 ? 1 : 2] = y; p[drop] = plane; return p;
}
static unsigned LFBits(float f) { unsigned u; std::memcpy(&u, &f, 4); return u; }
// Expectations below are constructed from known hit points and parameters,
// integer lattice inclusion, or literal threshold cases. No production solver
// or PointInFace is used to manufacture the expected result.
static void LFObserve(LFSamples &r, const LFEntries &fn, const LFCase &s) {
    for (int entry = 0; entry < 2; ++entry) {
        const int stage = s.stage, code = s.code;
        const VxVector before[] = {s.ray.m_Origin, s.ray.m_Direction, s.a, s.b, s.c, s.normal};
        VxVector p(71, 83, 97); float time = 109; int first = 127, second = 131;
        const int actual = entry == 0 ? fn.basic(s.ray, s.a, s.b, s.c, s.normal, p, time) :
            fn.axes(s.ray, s.a, s.b, s.c, s.normal, p, time, first, s.alias ? first : second);
        const VxVector expected = stage == 2 ? s.hit : VxVector(71, 83, 97);
        const bool pointBad = p.x != expected.x || p.y != expected.y || p.z != expected.z;
        const bool timeBad = time != (stage ? s.time : 109);
        const bool axesBad = entry && (stage == 2 ?
            (s.alias ? first != s.second : first != s.first || second != s.second) : first != 127 || second != 131);
        const bool changed = std::memcmp(&before[0], &s.ray.m_Origin, 12) || std::memcmp(&before[1], &s.ray.m_Direction, 12) ||
            std::memcmp(&before[2], &s.a, 12) || std::memcmp(&before[3], &s.b, 12) || std::memcmp(&before[4], &s.c, 12) || std::memcmp(&before[5], &s.normal, 12);
        ++r.samples; r.codeErrors += actual != code; r.pointErrors += pointBad; r.timeErrors += timeBad; r.axisErrors += axesBad; r.inputErrors += changed;
        r.errors += actual != code || pointBad || timeBad || axesBad || changed;
        for (unsigned value : {unsigned(actual), LFBits(p.x), LFBits(p.y), LFBits(p.z), LFBits(time), unsigned(first), unsigned(s.alias ? first : second)})
            r.digest = (r.digest ^ value) * 16777619u;
    }
}
static LFCase LFAxisCase(int drop, int winding, int x, int y, float size, float plane, float speed, float normal) {
    LFCase s; s.a = LFProject(0, 0, plane, drop); s.b = LFProject(winding ? 0 : 4 * size, winding ? 4 * size : 0, plane, drop);
    s.c = LFProject(winding ? 4 * size : 0, winding ? 0 : 4 * size, plane, drop); s.normal = LFProject(0, 0, normal, drop);
    s.hit = LFProject(x * size, y * size, plane, drop); s.ray.m_Direction = LFProject(0, 0, speed, drop);
    s.ray.m_Origin = s.hit; s.ray.m_Origin[drop] -= 2 * speed; s.time = 2; s.stage = 2;
    s.code = x >= 0 && y >= 0 && x + y <= 4 ? (winding ? 2 : 1) : 0;
    s.first = drop == 0 ? 1 : 0; s.second = drop == 2 ? 1 : 2; return s;
}
static LFSamples LFGrid(const LFEntries &fn) {
    LFSamples r;
    for (int drop = 0; drop < 3; ++drop) for (int winding = 0; winding < 2; ++winding)
        for (float normal : {-3.0f, -1.0f, 1.0f, 3.0f}) for (float speed : {-2.0f, 2.0f})
            for (int x = -1; x <= 5; ++x) for (int y = -1; y <= 5; ++y)
                LFObserve(r, fn, LFAxisCase(drop, winding, x, y, 1, 7, speed, normal));
    return r;
}
static LFSamples LFThresholds(const LFEntries &fn) {
    LFSamples r; const float eps = 0x1p-23f;
    for (int drop = 0; drop < 3; ++drop) {
        for (float speed : {0.0f, std::nextafter(eps, 0.0f), eps, std::nextafter(eps, 1.0f), -std::nextafter(eps, 0.0f), -eps, -std::nextafter(eps, 1.0f)}) {
            LFCase s = LFAxisCase(drop, 0, 1, 1, 1, 0, speed, 1);
            if (std::fabs(speed) < eps) { s.stage = 0; s.code = 0; }
            LFObserve(r, fn, s);
        }
        for(float side:{0.0f,1.0f}) {
            LFCase parallel=LFAxisCase(drop,0,1,1,1,0,0,1);parallel.ray.m_Direction=LFProject(1,.5f,0,drop);parallel.ray.m_Origin[drop]=side;parallel.stage=0;parallel.code=0;LFObserve(r,fn,parallel);
        }
        LFCase zero = LFAxisCase(drop, 0, 1, 1, 1, 0, -1, 0); zero.stage = 0; zero.code = 0; LFObserve(r, fn, zero);
    }
    return r;
}
static LFSamples LFTranslated(const LFEntries &fn) {
    LFSamples r;
    for (float base : {0.0f,65536.0f,1048576.0f,-1048576.0f}) for (int winding = 0; winding < 2; ++winding)
        for (int sign : {-1,1}) for (float time : {-2.0f,2.0f}) {
            LFCase s; s.a.Set(base,base,base+.125f); s.b.Set(base+4,base-4,base+.125f); s.c.Set(base+4,base,base-3.875f);
            if(winding){VxVector swap=s.b;s.b=s.c;s.c=swap;}
            s.normal.Set(float(sign),float(sign),float(sign));s.hit.Set(base+2,base-1,base-.875f);s.ray.m_Direction.Set(.5f,.25f,-1);
            if(time>0)s.ray.m_Origin.Set(base+1,base-1.5f,base+1.125f);else s.ray.m_Origin.Set(base+3,base-.5f,base-2.875f);
            s.time=time;s.stage=2;s.code=winding?2:1;s.first=1;s.second=2;LFObserve(r,fn,s);
        }
    return r;
}
static LFSamples LFExtreme(const LFEntries &fn) {
    LFSamples r;
    for(int drop=0;drop<3;++drop)for(int winding=0;winding<2;++winding)for(int sign:{-1,1})for(int kind=0;kind<2;++kind)for(int timeSign:{-1,1}) {
        LFCase s=kind?LFAxisCase(drop,winding,1,1,0x1p120f,0,-0x1p120f,float(sign)):
            LFAxisCase(drop,winding,1,1,1,0x1p125f,-0x1p122f,sign*0x1p120f);
        s.time=2.0f*timeSign;s.ray.m_Origin[drop]=s.hit[drop]-s.ray.m_Direction[drop]*s.time;LFObserve(r,fn,s);
    }
    return r;
}
static LFSamples LFSignedParameters(const LFEntries &fn) {
    LFSamples r;const float eps=0x1p-23f;
    for(int drop=0;drop<3;++drop)for(int winding=0;winding<2;++winding)for(float normal:{-1.0f,1.0f})for(float speed:{-1.0f,1.0f})
        for(int x:{-1,0,1,4})for(float time:{-0x1p120f,-4.0f,-2.0f,-1.0f,-eps,-std::nextafter(eps,0.0f),-0.0f,0.0f,eps,.5f,1.0f,1.0f+eps,2.0f,4.0f,0x1p120f}) {
            LFCase s=LFAxisCase(drop,winding,x,1,1,0,speed,normal);s.time=time;s.ray.m_Origin[drop]=-speed*time;LFObserve(r,fn,s);
        }
    return r;
}
static LFSamples LFFailuresAndAliases(const LFEntries &fn) {
    LFSamples r;
    for (int drop = 0; drop < 3; ++drop) for (int winding = 0; winding < 2; ++winding) for (int outside = 0; outside < 2; ++outside) {
        LFCase s = LFAxisCase(drop, winding, outside ? 4 : 1, 1, 1, 0, -1, 1); s.alias = true; LFObserve(r, fn, s);
        // The projected face can collapse to a finite segment or point; the
        // supplied normal still defines the tested plane, as in the original.
        s.alias = false; s.b = s.a; s.c = s.a; s.hit = s.a; s.ray.m_Origin = s.hit - s.ray.m_Direction * 2; s.code = 2; LFObserve(r, fn, s);
        s.hit[s.first] = 1; s.ray.m_Origin = s.hit - s.ray.m_Direction * 2; s.code = 0; LFObserve(r, fn, s);
    }
    return r;
}
#if defined(_WIN32)
static LFSamples LFFootprint(const LFEntries &fn) {
    LFSamples r; SYSTEM_INFO info; GetSystemInfo(&info); const size_t page = info.dwPageSize;
    unsigned char *pages = static_cast<unsigned char *>(VirtualAlloc(nullptr, page * 18, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!pages) { r.errors = 1; return r; }
    LFCase s = LFAxisCase(2, 0, 1, 1, 1, 0, -1, 1);
    VxRay *ray = new(pages + page - 24) VxRay(s.ray);
    VxVector *v[5]; const VxVector inputs[] = {s.a, s.b, s.c, s.normal, VxVector(71,83,97)};
    for (int i = 0; i < 5; ++i) v[i] = new(pages + page * (3 + 2 * i) - 12) VxVector(inputs[i]);
    float *time = new(pages + page * 13 - 4) float(109); int *first = new(pages + page * 15 - 4) int(127), *second = new(pages + page * 17 - 4) int(131);
    DWORD old; bool good = true;
    for (int i = 0; i < 9; ++i) good = VirtualProtect(pages + page * (2 * i + 1), page, PAGE_NOACCESS, &old) && good;
    for (int i = 0; i < 5; ++i) good = VirtualProtect(pages + page * 2 * i, page, PAGE_READONLY, &old) && good;
    if (good) for (int entry = 0; entry < 2; ++entry) {
        v[4]->Set(71,83,97); *time = 109; *first = 127; *second = 131;
        const int actual = entry == 0 ? fn.basic(*ray,*v[0],*v[1],*v[2],*v[3],*v[4],*time) :
            fn.axes(*ray,*v[0],*v[1],*v[2],*v[3],*v[4],*time,*first,*second);
        ++r.samples; r.errors += actual != 1 || v[4]->x != 1 || v[4]->y != 1 || v[4]->z != 0 || *time != 2 || (entry && (*first != 0 || *second != 1));
    }
    else r.errors = 1;
    VirtualFree(pages, 0, MEM_RELEASE); return r;
}
#endif
#ifndef VX_LINE_FACE_NATIVE_PROBE
static LFEntries LFLocalEntries() { return {static_cast<LFBasicFn>(VxIntersect::LineFace), static_cast<LFAxesFn>(VxIntersect::LineFace)}; }
static void LFCheck(LFSamples r) {
    EXPECT_EQ(r.errors, 0) << "codes=" << r.codeErrors << " points=" << r.pointErrors << " times=" << r.timeErrors << " axes=" << r.axisErrors << " inputs=" << r.inputErrors;
    ::testing::Test::RecordProperty("samples", r.samples); ::testing::Test::RecordProperty("digest", std::to_string(r.digest));
}
static void LFSelect(bool simd) {
#if defined(VX_SIMD_SSE)
    g_VxIntersectFaceDispatch.store(simd ? &kVxIntersectFaceDispatchSIMD : &kVxIntersectFaceDispatchScalar, std::memory_order_release);
#else
    (void) simd;
#endif
}
#define LF_TESTS(prefix, simd) \
TEST(LineFaceReference, prefix##LatticeBothOverloads) { LFSelect(simd); LFCheck(LFGrid(LFLocalEntries())); } \
TEST(LineFaceReference, prefix##ParallelThresholdOutputs) { LFSelect(simd); LFCheck(LFThresholds(LFLocalEntries())); } \
TEST(LineFaceReference, prefix##TranslatedTiltedPlanes) { LFSelect(simd); LFCheck(LFTranslated(LFLocalEntries())); } \
TEST(LineFaceReference, prefix##FiniteExtremePlanesAndGeometry) { LFSelect(simd); LFCheck(LFExtreme(LFLocalEntries())); } \
TEST(LineFaceReference, prefix##RejectedFacesDegeneratesAndAliasedAxes) { LFSelect(simd); LFCheck(LFFailuresAndAliases(LFLocalEntries())); } \
TEST(LineFaceReference, prefix##ReadonlyInputsAndExactOutputFootprints) { LFSelect(simd); LFCheck(LFFootprint(LFLocalEntries())); }
TEST(LineFaceReference, ScalarUnboundedSignedParameters) { LFSelect(false); LFCheck(LFSignedParameters(LFLocalEntries())); }
LF_TESTS(Scalar, false)
#if defined(VX_SIMD_SSE)
TEST(LineFaceReference, SimdUnboundedSignedParameters) { LFSelect(true); LFCheck(LFSignedParameters(LFLocalEntries())); }
LF_TESTS(Simd, true)
#endif
TEST(LineFaceReference, ActualCurrentDllExports) {
    HMODULE dll = LoadLibraryA("VxMath.dll"); ASSERT_NE(dll, nullptr);
#if defined(_WIN64)
    const char *basic = "?LineFace@VxIntersect@@SAHAEBVVxRay@@AEBUVxVector@@111AEAU3@AEAM@Z";
    const char *axes = "?LineFace@VxIntersect@@SAHAEBVVxRay@@AEBUVxVector@@111AEAU3@AEAMAEAH4@Z";
#else
    const char *basic = "?LineFace@VxIntersect@@SAHABVVxRay@@ABUVxVector@@111AAU3@AAM@Z";
    const char *axes = "?LineFace@VxIntersect@@SAHABVVxRay@@ABUVxVector@@111AAU3@AAMAAH4@Z";
#endif
    const LFEntries fn = {reinterpret_cast<LFBasicFn>(GetProcAddress(dll,basic)), reinterpret_cast<LFAxesFn>(GetProcAddress(dll,axes))};
    ASSERT_NE(fn.basic,nullptr); ASSERT_NE(fn.axes,nullptr);
    LFCheck(LFGrid(fn)); EXPECT_EQ(LFThresholds(fn).errors,0); EXPECT_EQ(LFTranslated(fn).errors,0); EXPECT_EQ(LFSignedParameters(fn).errors,0); EXPECT_EQ(LFExtreme(fn).errors,0);
    EXPECT_EQ(LFFailuresAndAliases(fn).errors,0); EXPECT_EQ(LFFootprint(fn).errors,0); EXPECT_NE(FreeLibrary(dll),0);
}
#endif
