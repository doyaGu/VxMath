#include <cmath>
#include <cfloat>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <new>
#include <initializer_list>
#ifndef VX_RAY_FACE_NATIVE_PROBE
#ifndef VX_LIB
#define VX_LIB 1
#endif
#include "VxRay.h"
#ifndef VX_RAY_FACE_IMPLEMENTATION
#define VX_RAY_FACE_IMPLEMENTATION "../../src/VxIntersect_FaceFace.cpp"
#endif
#include VX_RAY_FACE_IMPLEMENTATION
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

typedef int (*RFBasicFn)(const VxRay &, const VxVector &, const VxVector &, const VxVector &, const VxVector &, VxVector &, float &);
typedef int (*RFAxesFn)(const VxRay &, const VxVector &, const VxVector &, const VxVector &, const VxVector &, VxVector &, float &, int &, int &);
struct RFEntries { RFBasicFn basic; RFAxesFn axes, culled; };
struct RFCase {
    VxRay ray;
    VxVector a, b, c, normal, hit;
    float time;
    int code, stage, first, second;
    bool front, alias = false;
};
struct RFSamples { int samples = 0, errors = 0, codeErrors = 0, pointErrors = 0, timeErrors = 0, axisErrors = 0, inputErrors = 0; unsigned digest = 2166136261u; };
static VxVector RFProject(float x, float y, float plane, int drop) {
    VxVector p(0, 0, 0); p[drop == 0 ? 1 : 0] = x; p[drop == 2 ? 1 : 2] = y; p[drop] = plane; return p;
}
static unsigned RFBits(float f) { unsigned u; std::memcpy(&u, &f, 4); return u; }
// Expectations below are constructed from known hit points and parameters,
// integer lattice inclusion, or literal threshold cases. No production solver
// or PointInFace is used to manufacture the expected result.
static void RFObserve(RFSamples &r, const RFEntries &fn, const RFCase &s) {
    for (int entry = 0; entry < 3; ++entry) {
        const bool rejectFront = entry == 2 && !s.front;
        const int stage = rejectFront ? 0 : s.stage, code = rejectFront ? 0 : s.code;
        const VxVector before[] = {s.ray.m_Origin, s.ray.m_Direction, s.a, s.b, s.c, s.normal};
        VxVector p(71, 83, 97); float time = 109; int first = 127, second = 131;
        const int actual = entry == 0 ? fn.basic(s.ray, s.a, s.b, s.c, s.normal, p, time) :
            (entry == 1 ? fn.axes : fn.culled)(s.ray, s.a, s.b, s.c, s.normal, p, time, first, s.alias ? first : second);
        const VxVector expected = stage == 2 ? s.hit : VxVector(71, 83, 97);
        const bool pointBad = p.x != expected.x || p.y != expected.y || p.z != expected.z;
        const bool timeBad = time != (stage ? s.time : 109);
        const bool axesBad = entry && (stage == 2 ?
            (s.alias ? first != s.second : first != s.first || second != s.second) : first != 127 || second != 131);
        const bool changed = std::memcmp(&before[0], &s.ray.m_Origin, 12) || std::memcmp(&before[1], &s.ray.m_Direction, 12) ||
            std::memcmp(&before[2], &s.a, 12) || std::memcmp(&before[3], &s.b, 12) || std::memcmp(&before[4], &s.c, 12) || std::memcmp(&before[5], &s.normal, 12);
        ++r.samples; r.codeErrors += actual != code; r.pointErrors += pointBad; r.timeErrors += timeBad; r.axisErrors += axesBad; r.inputErrors += changed;
        r.errors += actual != code || pointBad || timeBad || axesBad || changed;
        for (unsigned value : {unsigned(actual), RFBits(p.x), RFBits(p.y), RFBits(p.z), RFBits(time), unsigned(first), unsigned(s.alias ? first : second)})
            r.digest = (r.digest ^ value) * 16777619u;
    }
}
static RFCase RFAxisCase(int drop, int winding, int x, int y, float size, float plane, float speed, float normal) {
    RFCase s; s.a = RFProject(0, 0, plane, drop); s.b = RFProject(winding ? 0 : 4 * size, winding ? 4 * size : 0, plane, drop);
    s.c = RFProject(winding ? 4 * size : 0, winding ? 0 : 4 * size, plane, drop); s.normal = RFProject(0, 0, normal, drop);
    s.hit = RFProject(x * size, y * size, plane, drop); s.ray.m_Direction = RFProject(0, 0, speed, drop);
    s.ray.m_Origin = s.hit; s.ray.m_Origin[drop] -= 2 * speed; s.time = 2; s.stage = 2;
    s.code = x >= 0 && y >= 0 && x + y <= 4 ? (winding ? 2 : 1) : 0;
    s.first = drop == 0 ? 1 : 0; s.second = drop == 2 ? 1 : 2; s.front = speed * double(normal) <= -0x1p-23; return s;
}
static RFSamples RFGrid(const RFEntries &fn) {
    RFSamples r;
    for (int drop = 0; drop < 3; ++drop) for (int winding = 0; winding < 2; ++winding)
        for (float normal : {-3.0f, -1.0f, 1.0f, 3.0f}) for (float speed : {-2.0f, 2.0f})
            for (int x = -1; x <= 5; ++x) for (int y = -1; y <= 5; ++y)
                RFObserve(r, fn, RFAxisCase(drop, winding, x, y, 1, 7, speed, normal));
    return r;
}
static RFSamples RFThresholds(const RFEntries &fn) {
    RFSamples r; const float eps = 0x1p-23f;
    for (int drop = 0; drop < 3; ++drop) {
        for (float speed : {0.0f, std::nextafter(eps, 0.0f), eps, std::nextafter(eps, 1.0f), -std::nextafter(eps, 0.0f), -eps, -std::nextafter(eps, 1.0f)}) {
            RFCase s = RFAxisCase(drop, 0, 1, 1, 1, 0, speed, 1);
            if (std::fabs(speed) < eps) { s.stage = 0; s.code = 0; }
            RFObserve(r, fn, s);
        }
        for (float time : {-1.0f, -std::nextafter(eps, 1.0f), -eps, -std::nextafter(eps, 0.0f), 0.0f, eps, 2.0f}) {
            RFCase s = RFAxisCase(drop, 0, 1, 1, 1, 0, -1, 1); s.time = time; s.ray.m_Origin[drop] = time;
            if (time < -eps) { s.stage = 1; s.code = 0; }
            RFObserve(r, fn, s);
        }
        RFCase zero = RFAxisCase(drop, 0, 1, 1, 1, 0, -1, 0); zero.stage = 0; zero.code = 0; RFObserve(r, fn, zero);
    }
    return r;
}
static RFSamples RFTranslated(const RFEntries &fn) {
    RFSamples r;
    // All vertices lie on x+y+z=3*base+.125. Storing D as float
    // moves the plane by .125 although the hit and parameter are representable.
    for (float base : {0.0f, 65536.0f, 1048576.0f, -1048576.0f}) for (int winding = 0; winding < 2; ++winding)
        for (int sign : {-1, 1}) {
            RFCase s; s.a.Set(base, base, base + .125f); s.b.Set(base + 4, base - 4, base + .125f); s.c.Set(base + 4, base, base - 3.875f);
            if (winding) { VxVector swap = s.b; s.b = s.c; s.c = swap; }
            s.normal.Set(float(sign), float(sign), float(sign)); s.hit.Set(base + 2, base - 1, base - .875f);
            s.ray.m_Direction.Set(.5f, .25f, -1); s.ray.m_Origin.Set(base + 1, base - 1.5f, base + 1.125f);
            s.time = 2; s.stage = 2; s.code = winding ? 2 : 1; s.first = 1; s.second = 2; s.front = sign > 0; RFObserve(r, fn, s);
        }
    return r;
}
static RFSamples RFExtreme(const RFEntries &fn) {
    RFSamples r;
    for (int drop = 0; drop < 3; ++drop) for (int winding = 0; winding < 2; ++winding) for (int sign : {-1, 1}) {
        RFObserve(r, fn, RFAxisCase(drop, winding, 1, 1, 1, 0x1p125f, -0x1p122f, sign * 0x1p120f));
        RFObserve(r, fn, RFAxisCase(drop, winding, 1, 1, 0x1p120f, 0, -0x1p120f, float(sign)));
    }
    return r;
}
static RFSamples RFFailuresAndAliases(const RFEntries &fn) {
    RFSamples r;
    for (int drop = 0; drop < 3; ++drop) for (int winding = 0; winding < 2; ++winding) for (int outside = 0; outside < 2; ++outside) {
        RFCase s = RFAxisCase(drop, winding, outside ? 4 : 1, 1, 1, 0, -1, 1); s.alias = true; RFObserve(r, fn, s);
        // The projected face can collapse to a finite segment or point; the
        // supplied normal still defines the tested plane, as in the original.
        s.alias = false; s.b = s.a; s.c = s.a; s.hit = s.a; s.ray.m_Origin = s.hit - s.ray.m_Direction * 2; s.code = 2; RFObserve(r, fn, s);
        s.hit[s.first] = 1; s.ray.m_Origin = s.hit - s.ray.m_Direction * 2; s.code = 0; RFObserve(r, fn, s);
    }
    return r;
}
#if defined(_WIN32)
static RFSamples RFFootprint(const RFEntries &fn) {
    RFSamples r; SYSTEM_INFO info; GetSystemInfo(&info); const size_t page = info.dwPageSize;
    unsigned char *pages = static_cast<unsigned char *>(VirtualAlloc(nullptr, page * 18, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!pages) { r.errors = 1; return r; }
    RFCase s = RFAxisCase(2, 0, 1, 1, 1, 0, -1, 1);
    VxRay *ray = new(pages + page - 24) VxRay(s.ray);
    VxVector *v[5]; const VxVector inputs[] = {s.a, s.b, s.c, s.normal, VxVector(71,83,97)};
    for (int i = 0; i < 5; ++i) v[i] = new(pages + page * (3 + 2 * i) - 12) VxVector(inputs[i]);
    float *time = new(pages + page * 13 - 4) float(109); int *first = new(pages + page * 15 - 4) int(127), *second = new(pages + page * 17 - 4) int(131);
    DWORD old; bool good = true;
    for (int i = 0; i < 9; ++i) good = VirtualProtect(pages + page * (2 * i + 1), page, PAGE_NOACCESS, &old) && good;
    for (int i = 0; i < 5; ++i) good = VirtualProtect(pages + page * 2 * i, page, PAGE_READONLY, &old) && good;
    if (good) for (int entry = 0; entry < 3; ++entry) {
        v[4]->Set(71,83,97); *time = 109; *first = 127; *second = 131;
        const int actual = entry == 0 ? fn.basic(*ray,*v[0],*v[1],*v[2],*v[3],*v[4],*time) :
            (entry == 1 ? fn.axes : fn.culled)(*ray,*v[0],*v[1],*v[2],*v[3],*v[4],*time,*first,*second);
        ++r.samples; r.errors += actual != 1 || v[4]->x != 1 || v[4]->y != 1 || v[4]->z != 0 || *time != 2 || (entry && (*first != 0 || *second != 1));
    }
    else r.errors = 1;
    VirtualFree(pages, 0, MEM_RELEASE); return r;
}
#endif
#ifndef VX_RAY_FACE_NATIVE_PROBE
static RFEntries RFLocalEntries() { return {static_cast<RFBasicFn>(VxIntersect::RayFace), static_cast<RFAxesFn>(VxIntersect::RayFace), VxIntersect::RayFaceCulled}; }
static void RFCheck(RFSamples r) {
    EXPECT_EQ(r.errors, 0) << "codes=" << r.codeErrors << " points=" << r.pointErrors << " times=" << r.timeErrors << " axes=" << r.axisErrors << " inputs=" << r.inputErrors;
    ::testing::Test::RecordProperty("samples", r.samples); ::testing::Test::RecordProperty("digest", std::to_string(r.digest));
}
static void RFSelect(bool simd) {
#if defined(VX_SIMD_SSE)
    g_VxIntersectFaceDispatch.store(simd ? &kVxIntersectFaceDispatchSIMD : &kVxIntersectFaceDispatchScalar, std::memory_order_release);
#else
    (void) simd;
#endif
}
#define RF_TESTS(prefix, simd) \
TEST(RayFaceReference, prefix##LatticeBothOverloadsAndCulling) { RFSelect(simd); RFCheck(RFGrid(RFLocalEntries())); } \
TEST(RayFaceReference, prefix##ParallelAndBehindThresholdOutputs) { RFSelect(simd); RFCheck(RFThresholds(RFLocalEntries())); } \
TEST(RayFaceReference, prefix##TranslatedTiltedPlanes) { RFSelect(simd); RFCheck(RFTranslated(RFLocalEntries())); } \
TEST(RayFaceReference, prefix##FiniteExtremePlanesAndGeometry) { RFSelect(simd); RFCheck(RFExtreme(RFLocalEntries())); } \
TEST(RayFaceReference, prefix##RejectedFacesDegeneratesAndAliasedAxes) { RFSelect(simd); RFCheck(RFFailuresAndAliases(RFLocalEntries())); } \
TEST(RayFaceReference, prefix##ReadonlyInputsAndExactOutputFootprints) { RFSelect(simd); RFCheck(RFFootprint(RFLocalEntries())); }
RF_TESTS(Scalar, false)
#if defined(VX_SIMD_SSE)
RF_TESTS(Simd, true)
#endif
TEST(RayFaceReference, ActualCurrentDllExports) {
    HMODULE dll = LoadLibraryA("VxMath.dll"); ASSERT_NE(dll, nullptr);
#if defined(_WIN64)
    const char *basic = "?RayFace@VxIntersect@@SAHAEBVVxRay@@AEBUVxVector@@111AEAU3@AEAM@Z";
    const char *axes = "?RayFace@VxIntersect@@SAHAEBVVxRay@@AEBUVxVector@@111AEAU3@AEAMAEAH4@Z";
    const char *culled = "?RayFaceCulled@VxIntersect@@SAHAEBVVxRay@@AEBUVxVector@@111AEAU3@AEAMAEAH4@Z";
#else
    const char *basic = "?RayFace@VxIntersect@@SAHABVVxRay@@ABUVxVector@@111AAU3@AAM@Z";
    const char *axes = "?RayFace@VxIntersect@@SAHABVVxRay@@ABUVxVector@@111AAU3@AAMAAH4@Z";
    const char *culled = "?RayFaceCulled@VxIntersect@@SAHABVVxRay@@ABUVxVector@@111AAU3@AAMAAH4@Z";
#endif
    const RFEntries fn = {reinterpret_cast<RFBasicFn>(GetProcAddress(dll,basic)), reinterpret_cast<RFAxesFn>(GetProcAddress(dll,axes)), reinterpret_cast<RFAxesFn>(GetProcAddress(dll,culled))};
    ASSERT_NE(fn.basic,nullptr); ASSERT_NE(fn.axes,nullptr); ASSERT_NE(fn.culled,nullptr);
    RFCheck(RFGrid(fn)); EXPECT_EQ(RFThresholds(fn).errors,0); EXPECT_EQ(RFTranslated(fn).errors,0); EXPECT_EQ(RFExtreme(fn).errors,0);
    EXPECT_EQ(RFFailuresAndAliases(fn).errors,0); EXPECT_EQ(RFFootprint(fn).errors,0); EXPECT_NE(FreeLibrary(dll),0);
}
#endif
