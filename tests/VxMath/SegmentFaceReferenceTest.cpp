#include <cmath>
#include <cfloat>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <new>
#include <initializer_list>
#ifndef VX_SEGMENT_FACE_NATIVE_PROBE
#ifndef VX_LIB
#define VX_LIB 1
#endif
#include "VxRay.h"
#ifndef VX_SEGMENT_FACE_IMPLEMENTATION
#define VX_SEGMENT_FACE_IMPLEMENTATION "../../src/VxIntersect_FaceFace.cpp"
#endif
#include VX_SEGMENT_FACE_IMPLEMENTATION
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

typedef int (*SFBasicFn)(const VxRay &, const VxVector &, const VxVector &, const VxVector &, const VxVector &, VxVector &, float &);
typedef int (*SFAxesFn)(const VxRay &, const VxVector &, const VxVector &, const VxVector &, const VxVector &, VxVector &, float &, int &, int &);
struct SFEntries { SFBasicFn basic; SFAxesFn axes, culled; };
struct SFCase {
    VxRay ray;
    VxVector a, b, c, normal, hit;
    float time;
    int code, stage, first, second;
    bool front, alias = false;
};
struct SFSamples { int samples = 0, errors = 0, codeErrors = 0, pointErrors = 0, timeErrors = 0, axisErrors = 0, inputErrors = 0; unsigned digest = 2166136261u; };
static VxVector SFProject(float x, float y, float plane, int drop) {
    VxVector p(0, 0, 0); p[drop == 0 ? 1 : 0] = x; p[drop == 2 ? 1 : 2] = y; p[drop] = plane; return p;
}
static unsigned SFBits(float f) { unsigned u; std::memcpy(&u, &f, 4); return u; }
// Expectations below are constructed from known hit points and parameters,
// integer lattice inclusion, or literal threshold cases. No production solver
// or PointInFace is used to manufacture the expected result.
static void SFObserve(SFSamples &r, const SFEntries &fn, const SFCase &s) {
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
        for (unsigned value : {unsigned(actual), SFBits(p.x), SFBits(p.y), SFBits(p.z), SFBits(time), unsigned(first), unsigned(s.alias ? first : second)})
            r.digest = (r.digest ^ value) * 16777619u;
    }
}
static SFCase SFAxisCase(int drop, int winding, int x, int y, float size, float plane, float speed, float normal) {
    SFCase s; s.a = SFProject(0, 0, plane, drop); s.b = SFProject(winding ? 0 : 4 * size, winding ? 4 * size : 0, plane, drop);
    s.c = SFProject(winding ? 4 * size : 0, winding ? 0 : 4 * size, plane, drop); s.normal = SFProject(0, 0, normal, drop);
    s.hit = SFProject(x * size, y * size, plane, drop); s.ray.m_Direction = SFProject(0, 0, speed, drop);
    s.ray.m_Origin = s.hit; s.ray.m_Origin[drop] -= .5f * speed; s.time = .5f; s.stage = 2;
    s.code = x >= 0 && y >= 0 && x + y <= 4 ? (winding ? 2 : 1) : 0;
    s.first = drop == 0 ? 1 : 0; s.second = drop == 2 ? 1 : 2; s.front = speed * double(normal) <= -0x1p-23; return s;
}
static SFSamples SFGrid(const SFEntries &fn) {
    SFSamples r;
    for (int drop = 0; drop < 3; ++drop) for (int winding = 0; winding < 2; ++winding)
        for (float normal : {-3.0f, -1.0f, 1.0f, 3.0f}) for (float speed : {-2.0f, 2.0f})
            for (int x = -1; x <= 5; ++x) for (int y = -1; y <= 5; ++y)
                SFObserve(r, fn, SFAxisCase(drop, winding, x, y, 1, 7, speed, normal));
    return r;
}
static SFSamples SFThresholds(const SFEntries &fn) {
    SFSamples r; const float eps = 0x1p-23f;
    for (int drop = 0; drop < 3; ++drop) {
        for (float speed : {0.0f, std::nextafter(eps, 0.0f), eps, std::nextafter(eps, 1.0f), -std::nextafter(eps, 0.0f), -eps, -std::nextafter(eps, 1.0f)}) {
            SFCase s = SFAxisCase(drop, 0, 1, 1, 1, 0, speed, 1);
            if (std::fabs(speed) < eps) { s.stage = 0; s.code = 0; }
            SFObserve(r, fn, s);
        }
        for (float time : {-1.0f, -std::nextafter(eps, 1.0f), -eps, -std::nextafter(eps, 0.0f), 0.0f, eps, .5f, 1.0f, 1.0f + eps, std::nextafter(1.0f + eps, 2.0f), 2.0f}) {
            SFCase s = SFAxisCase(drop, 0, 1, 1, 1, 0, -1, 1); s.time = time; s.ray.m_Origin[drop] = time;
            if (time < -eps || time > 1.0f + eps) { s.stage = 1; s.code = 0; }
            SFObserve(r, fn, s);
        }
        SFCase zero = SFAxisCase(drop, 0, 1, 1, 1, 0, -1, 0); zero.stage = 0; zero.code = 0; SFObserve(r, fn, zero);
    }
    return r;
}
static SFSamples SFTranslated(const SFEntries &fn) {
    SFSamples r;
    // All vertices lie on x+y+z=3*base+.125. Storing D as float
    // moves the plane by .125 although the hit and parameter are representable.
    for (float base : {0.0f, 65536.0f, 1048576.0f, -1048576.0f}) for (int winding = 0; winding < 2; ++winding)
        for (int sign : {-1, 1}) {
            SFCase s; s.a.Set(base, base, base + .125f); s.b.Set(base + 4, base - 4, base + .125f); s.c.Set(base + 4, base, base - 3.875f);
            if (winding) { VxVector swap = s.b; s.b = s.c; s.c = swap; }
            s.normal.Set(float(sign), float(sign), float(sign)); s.hit.Set(base + 2, base - 1, base - .875f);
            s.ray.m_Direction.Set(.5f, .25f, -1); s.ray.m_Origin.Set(base + 1.75f, base - 1.125f, base - .375f);
            s.time = .5f; s.stage = 2; s.code = winding ? 2 : 1; s.first = 1; s.second = 2; s.front = sign > 0; SFObserve(r, fn, s);
        }
    return r;
}
static SFSamples SFExtreme(const SFEntries &fn) {
    SFSamples r;
    for (int drop = 0; drop < 3; ++drop) for (int winding = 0; winding < 2; ++winding) for (int sign : {-1, 1}) {
        SFObserve(r, fn, SFAxisCase(drop, winding, 1, 1, 1, 0x1p125f, -0x1p122f, sign * 0x1p120f));
        SFObserve(r, fn, SFAxisCase(drop, winding, 1, 1, 0x1p120f, 0, -0x1p120f, float(sign)));
    }
    return r;
}
static SFSamples SFFailuresAndAliases(const SFEntries &fn) {
    SFSamples r;
    for (int drop = 0; drop < 3; ++drop) for (int winding = 0; winding < 2; ++winding) for (int outside = 0; outside < 2; ++outside) {
        SFCase s = SFAxisCase(drop, winding, outside ? 4 : 1, 1, 1, 0, -1, 1); s.alias = true; SFObserve(r, fn, s);
        // The projected face can collapse to a finite segment or point; the
        // supplied normal still defines the tested plane, as in the original.
        s.alias = false; s.b = s.a; s.c = s.a; s.hit = s.a; s.ray.m_Origin = s.hit - s.ray.m_Direction * .5f; s.code = 2; SFObserve(r, fn, s);
        s.hit[s.first] = 1; s.ray.m_Origin = s.hit - s.ray.m_Direction * .5f; s.code = 0; SFObserve(r, fn, s);
    }
    return r;
}
static SFSamples SFUnroundedEndpointParameters(const SFEntries &fn) {
    SFSamples r; const float eps = 0x1p-23f, upperOffset = 0x1p-40f, lowerOffset = 0x1p-60f;
    for (int i = 0; i < 4; ++i) {
        SFCase s; s.a.Set(0,0,0); s.b.Set(-4,4,0); s.c.Set(-4,0,4); s.normal.Set(1,1,1);
        s.ray.m_Direction.Set(-1,0,0); s.front = true; s.first = 1; s.second = 2;
        s.stage = i & 1 ? 2 : 1; s.code = i == 3 ? 1 : 0;
        if (i < 2) {
            // Both exact t values round to the same float 1+epsilon.
            s.ray.m_Origin.Set(1,eps,i ? -upperOffset : upperOffset); s.time = 1 + eps;
            s.hit.Set(-eps + upperOffset,eps,-upperOffset);
        } else {
            // Both exact t values round to the same float -epsilon.
            s.ray.m_Origin.Set(-eps,i == 3 ? lowerOffset : -lowerOffset,0); s.time = -eps;
            s.hit.Set(-lowerOffset,lowerOffset,0);
        }
        SFObserve(r,fn,s);
    }
    return r;
}
#if defined(_WIN32)
static SFSamples SFFootprint(const SFEntries &fn) {
    SFSamples r; SYSTEM_INFO info; GetSystemInfo(&info); const size_t page = info.dwPageSize;
    unsigned char *pages = static_cast<unsigned char *>(VirtualAlloc(nullptr, page * 18, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!pages) { r.errors = 1; return r; }
    SFCase s = SFAxisCase(2, 0, 1, 1, 1, 0, -1, 1);
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
        ++r.samples; r.errors += actual != 1 || v[4]->x != 1 || v[4]->y != 1 || v[4]->z != 0 || *time != .5f || (entry && (*first != 0 || *second != 1));
    }
    else r.errors = 1;
    VirtualFree(pages, 0, MEM_RELEASE); return r;
}
#endif
#ifndef VX_SEGMENT_FACE_NATIVE_PROBE
static SFEntries SFLocalEntries() { return {static_cast<SFBasicFn>(VxIntersect::SegmentFace), static_cast<SFAxesFn>(VxIntersect::SegmentFace), VxIntersect::SegmentFaceCulled}; }
static void SFCheck(SFSamples r) {
    EXPECT_EQ(r.errors, 0) << "codes=" << r.codeErrors << " points=" << r.pointErrors << " times=" << r.timeErrors << " axes=" << r.axisErrors << " inputs=" << r.inputErrors;
    ::testing::Test::RecordProperty("samples", r.samples); ::testing::Test::RecordProperty("digest", std::to_string(r.digest));
}
static void SFSelect(bool simd) {
#if defined(VX_SIMD_SSE)
    g_VxIntersectFaceDispatch.store(simd ? &kVxIntersectFaceDispatchSIMD : &kVxIntersectFaceDispatchScalar, std::memory_order_release);
#else
    (void) simd;
#endif
}
#define SF_TESTS(prefix, simd) \
TEST(SegmentFaceReference, prefix##LatticeBothOverloadsAndCulling) { SFSelect(simd); SFCheck(SFGrid(SFLocalEntries())); } \
TEST(SegmentFaceReference, prefix##ParallelAndBehindThresholdOutputs) { SFSelect(simd); SFCheck(SFThresholds(SFLocalEntries())); } \
TEST(SegmentFaceReference, prefix##TranslatedTiltedPlanes) { SFSelect(simd); SFCheck(SFTranslated(SFLocalEntries())); } \
TEST(SegmentFaceReference, prefix##FiniteExtremePlanesAndGeometry) { SFSelect(simd); SFCheck(SFExtreme(SFLocalEntries())); } \
TEST(SegmentFaceReference, prefix##RejectedFacesDegeneratesAndAliasedAxes) { SFSelect(simd); SFCheck(SFFailuresAndAliases(SFLocalEntries())); } \
TEST(SegmentFaceReference, prefix##ReadonlyInputsAndExactOutputFootprints) { SFSelect(simd); SFCheck(SFFootprint(SFLocalEntries())); }
TEST(SegmentFaceReference, ScalarUnroundedEndpointParameters) { SFSelect(false); SFCheck(SFUnroundedEndpointParameters(SFLocalEntries())); }
SF_TESTS(Scalar, false)
#if defined(VX_SIMD_SSE)
TEST(SegmentFaceReference, SimdUnroundedEndpointParameters) { SFSelect(true); SFCheck(SFUnroundedEndpointParameters(SFLocalEntries())); }
SF_TESTS(Simd, true)
#endif
TEST(SegmentFaceReference, ActualCurrentDllExports) {
    HMODULE dll = LoadLibraryA("VxMath.dll"); ASSERT_NE(dll, nullptr);
#if defined(_WIN64)
    const char *basic = "?SegmentFace@VxIntersect@@SAHAEBVVxRay@@AEBUVxVector@@111AEAU3@AEAM@Z";
    const char *axes = "?SegmentFace@VxIntersect@@SAHAEBVVxRay@@AEBUVxVector@@111AEAU3@AEAMAEAH4@Z";
    const char *culled = "?SegmentFaceCulled@VxIntersect@@SAHAEBVVxRay@@AEBUVxVector@@111AEAU3@AEAMAEAH4@Z";
#else
    const char *basic = "?SegmentFace@VxIntersect@@SAHABVVxRay@@ABUVxVector@@111AAU3@AAM@Z";
    const char *axes = "?SegmentFace@VxIntersect@@SAHABVVxRay@@ABUVxVector@@111AAU3@AAMAAH4@Z";
    const char *culled = "?SegmentFaceCulled@VxIntersect@@SAHABVVxRay@@ABUVxVector@@111AAU3@AAMAAH4@Z";
#endif
    const SFEntries fn = {reinterpret_cast<SFBasicFn>(GetProcAddress(dll,basic)), reinterpret_cast<SFAxesFn>(GetProcAddress(dll,axes)), reinterpret_cast<SFAxesFn>(GetProcAddress(dll,culled))};
    ASSERT_NE(fn.basic,nullptr); ASSERT_NE(fn.axes,nullptr); ASSERT_NE(fn.culled,nullptr);
    SFCheck(SFGrid(fn)); EXPECT_EQ(SFThresholds(fn).errors,0); EXPECT_EQ(SFTranslated(fn).errors,0); EXPECT_EQ(SFUnroundedEndpointParameters(fn).errors,0); EXPECT_EQ(SFExtreme(fn).errors,0);
    EXPECT_EQ(SFFailuresAndAliases(fn).errors,0); EXPECT_EQ(SFFootprint(fn).errors,0); EXPECT_NE(FreeLibrary(dll),0);
}
#endif
