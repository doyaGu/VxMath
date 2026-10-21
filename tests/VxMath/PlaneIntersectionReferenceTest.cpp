#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <new>
#ifndef VX_PLANE_INTERSECTION_NATIVE_PROBE
#ifndef VX_LIB
#define VX_LIB 1
#endif
#ifndef VX_PLANE_INTERSECTION_IMPLEMENTATION
#define VX_PLANE_INTERSECTION_IMPLEMENTATION "../../src/VxIntersect_Plane.cpp"
#endif
#include VX_PLANE_INTERSECTION_IMPLEMENTATION
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef XBOOL (*PIFn)(const VxRay &, const VxPlane &, VxVector &, float &);
struct PIResults {
    int samples = 0, errors = 0;
    unsigned digest = 2166136261u;
};

static VxRay PIRay(const VxVector &origin, const VxVector &direction) {
    VxRay ray;
    ray.m_Origin = origin;
    ray.m_Direction = direction;
    return ray;
}

static void PIObserve(PIResults &r, PIFn fn, int mode, const VxRay &ray, const VxPlane &plane) {
    // Endpoint signed distances give the crossing parameter independently of
    // the production dot/distance evaluation and its float rounding stages.
    double q0 = plane.m_D, q1 = plane.m_D;
    for (int i = 0; i < 3; ++i) {
        q0 += double(plane.m_Normal[i]) * ray.m_Origin[i];
        q1 += double(plane.m_Normal[i]) * (double(ray.m_Origin[i]) + ray.m_Direction[i]);
    }
    const double delta = q1 - q0;
    const double epsilon = std::ldexp(1.0, -23);
    const bool culled = mode == 1 || mode == 3;
    const bool parallel = culled ? delta > -epsilon : std::fabs(delta) < epsilon;
    const double t = parallel ? 0.0 : -q0 / delta;
    const bool hit = !parallel && (mode == 4 || t >= -epsilon) && ((mode != 2 && mode != 3) || t <= 1.0 + epsilon);
    VxVector point(101, -203, 307);
    float distance = 509;
    const int actual = fn(ray, plane, point, distance);
    bool good = actual == int(hit);
    if (parallel) good = good && distance == 509;
    else good = good && std::isfinite(distance) && std::fabs(double(distance) - t) <= 4e-6 * (1.0 + std::fabs(t));
    for (int i = 0; i < 3; ++i) {
        const double expected = double(ray.m_Origin[i]) + double(ray.m_Direction[i]) * t;
        if (hit) good = good && std::isfinite(point[i]) && std::fabs(double(point[i]) - expected) <= 4e-6 * (1.0 + std::fabs(expected));
        else good = good && point[i] == (i == 0 ? 101 : i == 1 ? -203 : 307);
    }
    if (!good) {
        if (r.errors < 3) std::fprintf(stderr, "mode=%d hit=%d/%d t=%.17g/%.17g point=(%.9g,%.9g,%.9g) n=(%.9g,%.9g,%.9g) D=%.9g O=(%.9g,%.9g,%.9g) V=(%.9g,%.9g,%.9g)\n", mode, actual, int(hit), double(distance), t, double(point.x), double(point.y), double(point.z), double(plane.m_Normal.x), double(plane.m_Normal.y), double(plane.m_Normal.z), double(plane.m_D), double(ray.m_Origin.x), double(ray.m_Origin.y), double(ray.m_Origin.z), double(ray.m_Direction.x), double(ray.m_Direction.y), double(ray.m_Direction.z));
        ++r.errors;
    }
    r.digest = (r.digest ^ unsigned(actual)) * 16777619u;
    // Include the output-mutation contract while avoiding irrelevant last-bit
    // differences in geometric coordinates between x87 and SSE2.
    r.digest = (r.digest ^ unsigned(distance != 509)) * 16777619u;
    r.digest = (r.digest ^ unsigned(point.x != 101 || point.y != -203 || point.z != 307)) * 16777619u;
    ++r.samples;
}

static PIResults PIOrdinary(const PIFn *functions) {
    PIResults r;
    const VxVector normals[] = {VxVector(1, 0, 0), VxVector(0, -1, 0), VxVector(0, 0, 2), VxVector(1, 2, -1), VxVector(-2, 1, 3)};
    for (const VxVector &n : normals) {
        VxPlane plane;
        plane.m_Normal = n;
        for (int d = -4; d <= 4; d += 2) {
            plane.m_D = float(d);
            for (int x = -2; x <= 2; ++x)
                for (int y = -2; y <= 2; ++y)
                    for (int z = -2; z <= 2; ++z)
                        for (int direction = -1; direction <= 1; ++direction) {
                            const VxRay ray = PIRay(VxVector(float(x), float(y), float(z)), n * float(direction));
                            for (int mode = 0; mode < 5; ++mode) PIObserve(r, functions[mode], mode, ray, plane);
                        }
        }
    }
    return r;
}

static PIResults PIBoundaries(const PIFn *functions) {
    PIResults r;
    const float e = std::ldexp(1.0f, -23);
    const float speeds[] = {-2 * e, -e, std::nextafter(-e, 0.0f), 0, std::nextafter(e, 0.0f), e, 2 * e};
    VxPlane plane;
    plane.m_Normal = VxVector(1, 0, 0);
    plane.m_D = 0;
    for (float speed : speeds)
        for (int mode = 0; mode < 5; ++mode)
            PIObserve(r, functions[mode], mode, PIRay(VxVector(0, 1, -2), VxVector(speed, 0, 0)), plane);
    const float parameters[] = {std::nextafter(-e, -1.0f), -e, std::nextafter(-e, 0.0f), 0, 1, 1 + e, std::nextafter(1 + e, 2.0f)};
    for (float t : parameters)
        for (int sign = -1; sign <= 1; sign += 2)
            for (int mode = 0; mode < 5; ++mode)
                PIObserve(r, functions[mode], mode, PIRay(VxVector(-t * sign, 1, -2), VxVector(float(sign), 0, 0)), plane);
    // Exact parameter is a fraction of an ULP outside the original range,
    // although storing it into the public float parameter rounds to the edge.
    plane.m_Normal = VxVector(-1, e / 4, 0);
    plane.m_D = -e;
    for (int mode = 0; mode < 5; ++mode)
        PIObserve(r, functions[mode], mode, PIRay(VxVector(0, 0, 0), VxVector(1, 1, 0)), plane);
    plane.m_D = 1.0f + e;
    for (int mode = 0; mode < 5; ++mode)
        PIObserve(r, functions[mode], mode, PIRay(VxVector(0, 0, 0), VxVector(1, 1, 0)), plane);
    return r;
}

static PIResults PIWide(const PIFn *functions) {
    PIResults r;
    VxPlane plane;
    plane.m_Normal = VxVector(-FLT_MAX, 0, 0);
    plane.m_D = 0;
    for (int mode = 0; mode < 5; ++mode)
        PIObserve(r, functions[mode], mode, PIRay(VxVector(-2, 1, -1), VxVector(4, 0, 0)), plane);
    plane.m_Normal = VxVector(-2, 0, 0);
    for (int mode = 0; mode < 5; ++mode)
        PIObserve(r, functions[mode], mode, PIRay(VxVector(-FLT_MAX / 2, 1, -1), VxVector(FLT_MAX, 0, 0)), plane);
    // Dot cancellation: float products become infinities despite a small,
    // finite signed distance and a crossing exactly at t = 1/2.
    plane.m_Normal = VxVector(FLT_MAX, -FLT_MAX, -1);
    for (int mode = 0; mode < 5; ++mode)
        PIObserve(r, functions[mode], mode, PIRay(VxVector(2, 2, -1), VxVector(0, 0, 2)), plane);
    return r;
}

#ifndef VX_PLANE_INTERSECTION_NATIVE_PROBE
static const PIFn piScalar[] = {RayPlaneScalarCore, RayPlaneCulledScalarCore, SegmentPlaneScalarCore, SegmentPlaneCulledScalarCore, LinePlaneScalarCore};
static const PIFn piPublic[] = {VxIntersect::RayPlane, VxIntersect::RayPlaneCulled, VxIntersect::SegmentPlane, VxIntersect::SegmentPlaneCulled, VxIntersect::LinePlane};
static void PICheck(PIResults r) {
    EXPECT_EQ(r.errors, 0);
    ::testing::Test::RecordProperty("samples", r.samples);
    ::testing::Test::RecordProperty("digest", std::to_string(r.digest));
}
TEST(PlaneIntersectionReference, ScalarOrdinaryGeometryAndOutputMutation) { PICheck(PIOrdinary(piScalar)); }
TEST(PlaneIntersectionReference, ScalarThresholdsAndUnroundedParameter) { PICheck(PIBoundaries(piScalar)); }
TEST(PlaneIntersectionReference, ScalarFiniteProductsAndCancellation) { PICheck(PIWide(piScalar)); }
TEST(PlaneIntersectionReference, PublicScalarTableMatchesContract) {
    VxIntersectPlaneDispatchRebuild(false);
    PICheck(PIOrdinary(piPublic));
    EXPECT_EQ(PIBoundaries(piPublic).errors, 0);
    EXPECT_EQ(PIWide(piPublic).errors, 0);
}
#if defined(VX_SIMD_SSE)
static const PIFn piSimd[] = {RayPlaneSIMDCore, RayPlaneCulledSIMDCore, SegmentPlaneSIMDCore, SegmentPlaneCulledSIMDCore, LinePlaneSIMDCore};
TEST(PlaneIntersectionReference, SimdOrdinaryGeometryAndOutputMutation) { PICheck(PIOrdinary(piSimd)); }
TEST(PlaneIntersectionReference, SimdThresholdsAndUnroundedParameter) { PICheck(PIBoundaries(piSimd)); }
TEST(PlaneIntersectionReference, SimdFiniteProductsAndCancellation) { PICheck(PIWide(piSimd)); }
TEST(PlaneIntersectionReference, PublicSimdTableAndRepeatedTransitions) {
    for (int i = 0; i < 4; ++i) {
        VxIntersectPlaneDispatchRebuild(i & 1);
        EXPECT_EQ(PIBoundaries(piPublic).errors, 0);
        EXPECT_EQ(PIWide(piPublic).errors, 0);
    }
    VxIntersectPlaneDispatchRebuild(false);
    PICheck(PIOrdinary(piPublic));
}
#endif
#if defined(_WIN32)
TEST(PlaneIntersectionReference, ActualCurrentDllFiveExports) {
    HMODULE dll = LoadLibraryA("VxMath.dll");
    ASSERT_NE(dll, nullptr);
    const char *names[] = {"RayPlane", "RayPlaneCulled", "SegmentPlane", "SegmentPlaneCulled", "LinePlane"};
    PIFn functions[5];
    for (int i = 0; i < 5; ++i) {
        char symbol[160];
#if defined(_WIN64)
        std::snprintf(symbol, sizeof(symbol), "?%s@VxIntersect@@SAHAEBVVxRay@@AEBVVxPlane@@AEAUVxVector@@AEAM@Z", names[i]);
#else
        std::snprintf(symbol, sizeof(symbol), "?%s@VxIntersect@@SAHABVVxRay@@ABVVxPlane@@AAUVxVector@@AAM@Z", names[i]);
#endif
        functions[i] = reinterpret_cast<PIFn>(GetProcAddress(dll, symbol));
        ASSERT_NE(functions[i], nullptr) << symbol;
    }
    PICheck(PIOrdinary(functions));
    EXPECT_EQ(PIBoundaries(functions).errors, 0);
    EXPECT_EQ(PIWide(functions).errors, 0);
    EXPECT_NE(FreeLibrary(dll), 0);
}
TEST(PlaneIntersectionReference, ExactReadonlyInputsAndWritableOutputsAtGuardPages) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    const std::size_t page = info.dwPageSize;
    unsigned char *regions[4] = {};
    for (int i = 0; i < 4; ++i) {
        regions[i] = static_cast<unsigned char *>(VirtualAlloc(nullptr, page * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        ASSERT_NE(regions[i], nullptr);
        DWORD old;
        ASSERT_NE(VirtualProtect(regions[i] + page, page, PAGE_NOACCESS, &old), 0);
    }
    VxRay *ray = new(regions[0] + page - sizeof(VxRay)) VxRay;
    *ray = PIRay(VxVector(-1, 2, 3), VxVector(2, 0, 0));
    VxPlane *plane = new(regions[1] + page - sizeof(VxPlane)) VxPlane;
    plane->m_Normal = VxVector(-1, 0, 0);
    plane->m_D = 0;
    VxVector *point = new(regions[2] + page - sizeof(VxVector)) VxVector;
    float *t = new(regions[3] + page - sizeof(float)) float;
    DWORD old;
    for (int i = 0; i < 2; ++i) ASSERT_NE(VirtualProtect(regions[i], page, PAGE_READONLY, &old), 0);
    for (int mode = 0; mode < 5; ++mode) {
        EXPECT_EQ(piScalar[mode](*ray, *plane, *point, *t), 1);
#if defined(VX_SIMD_SSE)
        EXPECT_EQ(piSimd[mode](*ray, *plane, *point, *t), 1);
#endif
        EXPECT_FLOAT_EQ(*t, 0.5f);
        EXPECT_EQ(point->x, 0); EXPECT_EQ(point->y, 2); EXPECT_EQ(point->z, 3);
    }
    for (unsigned char *region : regions) EXPECT_NE(VirtualFree(region, 0, MEM_RELEASE), 0);
}
#endif
#endif
