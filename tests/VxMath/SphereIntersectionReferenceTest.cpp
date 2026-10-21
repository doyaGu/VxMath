#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <new>
#include <initializer_list>
#ifndef VX_SPHERE_INTERSECTION_NATIVE_PROBE
#ifndef VX_LIB
#define VX_LIB 1
#endif
#ifndef VX_SPHERE_INTERSECTION_IMPLEMENTATION
#define VX_SPHERE_INTERSECTION_IMPLEMENTATION "../../src/VxIntersect_Sphere.cpp"
#endif
#include VX_SPHERE_INTERSECTION_IMPLEMENTATION
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef XBOOL (*SIBox)(const VxSphere &, const VxBbox &);
typedef int (*SIRay)(const VxRay &, const VxSphere &, VxVector *, VxVector *);
typedef XBOOL (*SIMove)(const VxSphere &, const VxVector &, const VxSphere &, const VxVector &, float *, float *);
struct SIResults { int samples = 0, errors = 0; unsigned digest = 2166136261u; };
static void SIObserve(SIResults &r, int actual, bool good) { if (!good) ++r.errors; r.digest = (r.digest ^ unsigned(actual)) * 16777619u; ++r.samples; }
static VxRay SIMakeRay(const VxVector &origin, const VxVector &direction) { VxRay r; r.m_Origin = origin; r.m_Direction = direction; return r; }

// Exhaustive candidates on faces/edges/corners, independent of the production
// componentwise clamp and squared-distance arithmetic.
static bool SIBoxOracle(const VxSphere &s, const VxBbox &b) {
    double best = DBL_MAX;
    for (int x = 0; x < 3; ++x) for (int y = 0; y < 3; ++y) for (int z = 0; z < 3; ++z) {
        const int selection[] = {x, y, z};
        double distance[3]; bool valid = true;
        for (int i = 0; i < 3; ++i) {
            const double value = selection[i] == 0 ? s.Center()[i] : selection[i] == 1 ? b.Min[i] : b.Max[i];
            if (value < b.Min[i] || value > b.Max[i]) valid = false;
            distance[i] = value - s.Center()[i];
        }
        if (valid) {
            const double length = std::hypot(distance[0], distance[1], distance[2]);
            if (length < best) best = length;
        }
    }
    return best <= s.Radius();
}
static SIResults SIBoxes(SIBox fn) {
    SIResults r;
    const VxVector extents[] = {VxVector(0, 0, 0), VxVector(1, 1, 1), VxVector(2, 0, 1)};
    for (const VxVector &extent : extents) for (int x = -4; x <= 4; ++x) for (int y = -4; y <= 4; ++y) for (int z = -4; z <= 4; ++z) for (int radius = 0; radius <= 3; ++radius) {
        const VxBbox box(-extent, extent); const VxSphere sphere{VxVector(float(x), float(y), float(z)), float(radius)};
        const int actual = fn(sphere, box); SIObserve(r, actual, actual == int(SIBoxOracle(sphere, box)));
    }
    return r;
}
static SIResults SIBoxSpecial(SIBox fn) {
    SIResults r;
    const VxBbox unit(VxVector(-1, -1, -1), VxVector(1, 1, 1));
    const VxSphere approximation(VxVector(3, 1.25f, 0), 2);
    int actual = fn(approximation, unit); SIObserve(r, actual, actual == 0);
    const VxBbox point(VxVector(0, 0, 0), VxVector(0, 0, 0));
    const VxSphere large(VxVector(FLT_MAX, 0, 0), FLT_MAX / 2);
    actual = fn(large, point); SIObserve(r, actual, actual == 0);
    const VxSphere tiny(VxVector(std::ldexp(1.0f, -90), 0, 0), std::ldexp(1.0f, -91));
    actual = fn(tiny, point); SIObserve(r, actual, actual == 0);
    const VxBbox opposite(VxVector(-FLT_MAX, 0, 0), VxVector(-FLT_MAX, 0, 0));
    const VxSphere distant(VxVector(FLT_MAX, 0, 0), FLT_MAX);
    actual = fn(distant, opposite); SIObserve(r, actual, actual == 0);
    return r;
}

// Independent quadratic oracle uses the original non-unit direction directly;
// no normalization/projection or cross-product-distance implementation copied.
static int SIRayOracle(const VxRay &ray, const VxSphere &sphere, double *first, double *second) {
    double a = 0, halfB = 0, c = -double(sphere.Radius()) * sphere.Radius();
    for (int i = 0; i < 3; ++i) {
        const double delta = double(ray.m_Origin[i]) - sphere.Center()[i];
        a += double(ray.m_Direction[i]) * ray.m_Direction[i];
        halfB += delta * ray.m_Direction[i]; c += delta * delta;
    }
    if (a == 0) return 0;
    const double discriminant = halfB * halfB - a * c;
    if (discriminant < 0) return 0;
    const double root = std::sqrt(discriminant);
    const double t1 = (-halfB - root) / a, t2 = (-halfB + root) / a;
    for (int i = 0; i < 3; ++i) {
        first[i] = double(ray.m_Origin[i]) + ray.m_Direction[i] * t1;
        second[i] = double(ray.m_Origin[i]) + ray.m_Direction[i] * t2;
    }
    return discriminant == 0 ? 1 : 2;
}
static void SIRayObserve(SIResults &r, SIRay fn, const VxRay &ray, const VxSphere &sphere) {
    double first[3], second[3];
    const int expected = SIRayOracle(ray, sphere, first, second);
    VxVector one(101, -203, 307), two(-401, 503, -607);
    const int actual = fn(ray, sphere, &one, &two);
    bool good = actual == expected;
    for (int i = 0; i < 3; ++i) {
        good = good && (expected ? std::isfinite(one[i]) && std::fabs(double(one[i]) - first[i]) <= 2e-5 * (1 + std::fabs(first[i])) : one[i] == (i == 0 ? 101 : i == 1 ? -203 : 307));
        good = good && (expected == 2 ? std::isfinite(two[i]) && std::fabs(double(two[i]) - second[i]) <= 2e-5 * (1 + std::fabs(second[i])) : two[i] == (i == 0 ? -401 : i == 1 ? 503 : -607));
    }
    SIObserve(r, actual, good);
}
static SIResults SIRays(SIRay fn) {
    SIResults r;
    const VxVector directions[] = {VxVector(1, 0, 0), VxVector(-2, 0, 0), VxVector(0, 3, 0), VxVector(0, 0, -4), VxVector(3, 4, 0)};
    for (int radius = 1; radius <= 2; ++radius) for (int x = -4; x <= 4; x += 2) for (int y = -4; y <= 4; y += 2) for (int z = -4; z <= 4; z += 2) for (const VxVector &direction : directions)
        SIRayObserve(r, fn, SIMakeRay(VxVector(float(x), float(y), float(z)), direction), VxSphere(VxVector(0, 0, 0), float(radius)));
    return r;
}
static SIResults SIRayScales(SIRay fn) {
    SIResults r;
    const int powers[] = {-120, -90, -40, -10, 0, 10, 40, 90, 120};
    const VxSphere sphere(VxVector(0, 0, 0), 1);
    for (int power : powers) for (int sign = -1; sign <= 1; sign += 2) for (int side = 0; side < 3; ++side) {
        const VxRay ray = SIMakeRay(VxVector(-4, float(side), 0), VxVector(std::ldexp(float(sign), power), 0, 0));
        SIRayObserve(r, fn, ray, sphere);
    }
    return r;
}
static SIResults SIRayWide(SIRay fn) {
    SIResults r;
    const VxSphere sphere(VxVector(0, 0, 0), 1);
    // The near/far point offsets remain representable even when a combined t
    // cannot retain the unit-radius difference from a very large origin.
    for (int power : {40, 90, 120}) {
        const float start = std::ldexp(1.0f, power);
        const VxRay ray = SIMakeRay(VxVector(-start, 0, 0), VxVector(start, 0, 0));
        VxVector one(101, 203, 307), two(401, 503, 607);
        const int actual = fn(ray, sphere, &one, &two);
        SIObserve(r, actual, actual == 2 && one.x == -1 && two.x == 1 && one.y == 0 && two.y == 0 && one.z == 0 && two.z == 0);
    }
    return r;
}

static void SIMoveOracle(const VxSphere &one, const VxVector &endOne, const VxSphere &two, const VxVector &endTwo, bool &hit, bool &write, double &enter, double &exit) {
    const double radius = double(one.Radius()) + two.Radius();
    double a = 0, b = 0, c = -radius * radius;
    for (int i = 0; i < 3; ++i) {
        const double delta = double(two.Center()[i]) - one.Center()[i];
        const double speed = (double(endTwo[i]) - two.Center()[i]) - (double(endOne[i]) - one.Center()[i]);
        a += speed * speed; b += 2 * delta * speed; c += delta * delta;
    }
    enter = exit = 0; write = true;
    if (c <= 0) { hit = true; return; }
    if (a == 0) { hit = false; return; }
    const double discriminant = b * b - 4 * a * c;
    if (discriminant < 0) { hit = false; write = false; return; }
    const double root = std::sqrt(discriminant);
    const double q = -0.5 * (b + std::copysign(root, b));
    enter = q / a; exit = q == 0 ? enter : c / q;
    if (enter > exit) { const double saved = enter; enter = exit; exit = saved; }
    hit = enter >= 0 && enter <= 1;
}
static void SIMoveObserve(SIResults &r, SIMove fn, const VxSphere &one, const VxVector &endOne, const VxSphere &two, const VxVector &endTwo) {
    double enter, exit; bool expected, write;
    SIMoveOracle(one, endOne, two, endTwo, expected, write, enter, exit);
    float first = 101, second = -203;
    const int actual = fn(one, endOne, two, endTwo, &first, &second);
    const bool good = actual == int(expected) && (write ? std::isfinite(first) && std::isfinite(second) && std::fabs(double(first) - enter) <= 2e-5 * (1 + std::fabs(enter)) && std::fabs(double(second) - exit) <= 2e-5 * (1 + std::fabs(exit)) : first == 101 && second == -203);
    if (!good && r.errors < 3) std::fprintf(stderr, "moving result=%d/%d t=(%.9g,%.9g)/(%.17g,%.17g) C2=(%.9g,%.9g,%.9g) P1=(%.9g,%.9g,%.9g) P2=(%.9g,%.9g,%.9g) R=(%.9g,%.9g)\n", actual, int(expected), double(first), double(second), enter, exit, double(two.Center().x), double(two.Center().y), double(two.Center().z), double(endOne.x), double(endOne.y), double(endOne.z), double(endTwo.x), double(endTwo.y), double(endTwo.z), double(one.Radius()), double(two.Radius()));
    SIObserve(r, actual, good);
}
static SIResults SIMovements(SIMove fn) {
    SIResults r;
    const VxVector speeds[] = {VxVector(0, 0, 0), VxVector(4, 0, 0), VxVector(-4, 0, 0), VxVector(0, 4, 0), VxVector(0, -4, 0), VxVector(3, 4, 0)};
    for (int x = -6; x <= 6; x += 3) for (int y = -3; y <= 3; y += 3) for (int z = -3; z <= 3; z += 3) for (int radius = 1; radius <= 2; ++radius) for (const VxVector &speed : speeds) for (int common = 0; common < 2; ++common) {
        const VxSphere one(VxVector(0, 0, 0), 1);
        const VxSphere two{VxVector(float(x), float(y), float(z)), float(radius)};
        const VxVector commonMovement(float(common), -float(common), 0);
        SIMoveObserve(r, fn, one, one.Center() + commonMovement, two, two.Center() + speed + commonMovement);
    }
    return r;
}
static SIResults SIMovementScales(SIMove fn) {
    SIResults r;
    const int powers[] = {-120, -90, -40, -10, 0, 10, 40, 90, 120};
    for (int power : powers) for (int sign = -1; sign <= 1; sign += 2) {
        const float scale = std::ldexp(1.0f, power);
        const VxSphere one(VxVector(0, 0, 0), scale), two(VxVector(5 * scale * sign, 0, 0), scale);
        SIMoveObserve(r, fn, one, one.Center(), two, VxVector(0, 0, 0));
        const VxSphere large(VxVector(FLT_MAX * sign, 0, 0), FLT_MAX / 4);
        if (power == 0) SIMoveObserve(r, fn, one, one.Center(), large, large.Center());
    }
    return r;
}
#if defined(_WIN32)
static SIResults SIFootprints(SIBox boxFn, SIRay rayFn, SIMove moveFn) {
    SIResults r; SYSTEM_INFO info; GetSystemInfo(&info); const std::size_t page = info.dwPageSize; unsigned char *regions[10] = {};
    for (int i = 0; i < 10; ++i) { regions[i] = static_cast<unsigned char *>(VirtualAlloc(nullptr, page * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)); if (!regions[i]) { ++r.errors; break; } DWORD old; if (!VirtualProtect(regions[i] + page, page, PAGE_NOACCESS, &old)) ++r.errors; }
    if (!r.errors) {
        VxSphere *one = new(regions[0] + page - sizeof(VxSphere)) VxSphere(VxVector(0, 0, 0), 1), *two = new(regions[1] + page - sizeof(VxSphere)) VxSphere(VxVector(4, 0, 0), 1);
        VxVector *endOne = new(regions[2] + page - sizeof(VxVector)) VxVector(0, 0, 0), *endTwo = new(regions[3] + page - sizeof(VxVector)) VxVector(0, 0, 0);
        VxRay *ray = new(regions[4] + page - sizeof(VxRay)) VxRay; *ray = SIMakeRay(VxVector(-4, 0, 0), VxVector(2, 0, 0));
        VxBbox *box = new(regions[5] + page - sizeof(VxBbox)) VxBbox(VxVector(-1, -1, -1), VxVector(1, 1, 1));
        VxVector *nearPoint = new(regions[6] + page - sizeof(VxVector)) VxVector, *farPoint = new(regions[7] + page - sizeof(VxVector)) VxVector;
        float *enter = new(regions[8] + page - sizeof(float)) float, *exit = new(regions[9] + page - sizeof(float)) float;
        for (int i = 0; i < 6; ++i) { DWORD old; if (!VirtualProtect(regions[i], page, PAGE_READONLY, &old)) ++r.errors; }
        int actual = boxFn(*one, *box); SIObserve(r, actual, actual == 1);
        actual = rayFn(*ray, *one, nearPoint, farPoint); SIObserve(r, actual, actual == 2 && nearPoint->x == -1 && farPoint->x == 1);
        actual = moveFn(*one, *endOne, *two, *endTwo, enter, exit); SIObserve(r, actual, actual == 1 && *enter == 0.5f && *exit == 1.5f);
    }
    for (unsigned char *region : regions) if (region && !VirtualFree(region, 0, MEM_RELEASE)) ++r.errors;
    return r;
}
#endif
#ifndef VX_SPHERE_INTERSECTION_NATIVE_PROBE
static void SICheck(SIResults r) { EXPECT_EQ(r.errors, 0); ::testing::Test::RecordProperty("samples", r.samples); ::testing::Test::RecordProperty("digest", std::to_string(r.digest)); }
TEST(SphereIntersectionReference, ScalarBoxCandidatesAndTangency) { SICheck(SIBoxes(SphereAABBScalarCore)); }
TEST(SphereIntersectionReference, ScalarBoxExactDistanceAndFiniteExtremes) { SICheck(SIBoxSpecial(SphereAABBScalarCore)); }
TEST(SphereIntersectionReference, ScalarLineSphereRootsAndOutputMutation) { SICheck(SIRays(RaySphereScalarCore)); }
TEST(SphereIntersectionReference, ScalarLineDirectionMagnitudeDoesNotChangeGeometry) { SICheck(SIRayScales(RaySphereScalarCore)); }
TEST(SphereIntersectionReference, ScalarLineWideOriginPreservesNearAndFarPoints) { SICheck(SIRayWide(RaySphereScalarCore)); }
TEST(SphereIntersectionReference, ScalarMovingSphereEndpointsAndTimeInterval) { SICheck(SIMovements(SphereSphereScalarCore)); }
TEST(SphereIntersectionReference, ScalarMovingSphereEquivalentScalesAndStationaryMiss) { SICheck(SIMovementScales(SphereSphereScalarCore)); }
#if defined(VX_SIMD_SSE)
TEST(SphereIntersectionReference, SimdBoxCandidatesAndTangency) { SICheck(SIBoxes(SphereAABBMSIMDCore)); }
TEST(SphereIntersectionReference, SimdBoxExactDistanceAndFiniteExtremes) { SICheck(SIBoxSpecial(SphereAABBMSIMDCore)); }
TEST(SphereIntersectionReference, SimdLineSphereRootsAndOutputMutation) { SICheck(SIRays(RaySphereSIMDCore)); }
TEST(SphereIntersectionReference, SimdLineDirectionMagnitudeDoesNotChangeGeometry) { SICheck(SIRayScales(RaySphereSIMDCore)); }
TEST(SphereIntersectionReference, SimdLineWideOriginPreservesNearAndFarPoints) { SICheck(SIRayWide(RaySphereSIMDCore)); }
TEST(SphereIntersectionReference, SimdMovingSphereEndpointsAndTimeInterval) { SICheck(SIMovements(SphereSphereSIMDCore)); }
TEST(SphereIntersectionReference, SimdMovingSphereEquivalentScalesAndStationaryMiss) { SICheck(SIMovementScales(SphereSphereSIMDCore)); }
#endif
TEST(SphereIntersectionReference, OptionalOutputsAndExactZeroDirection) {
    const VxSphere s(VxVector(0, 0, 0), 1); VxVector point(101, 203, 307);
    EXPECT_EQ(RaySphereScalarCore(SIMakeRay(VxVector(1, 0, -4), VxVector(0, 0, 2)), s, nullptr, &point), 1); EXPECT_EQ(point.x, 1); EXPECT_EQ(point.z, 0);
    EXPECT_EQ(RaySphereScalarCore(SIMakeRay(VxVector(0, 0, 0), VxVector(0, 0, 0)), s, nullptr, nullptr), 0);
    EXPECT_TRUE(SphereSphereScalarCore(s, s.Center(), s, s.Center(), nullptr, nullptr));
#if defined(VX_SIMD_SSE)
    EXPECT_EQ(RaySphereSIMDCore(SIMakeRay(VxVector(1, 0, -4), VxVector(0, 0, 2)), s, nullptr, &point), 1); EXPECT_EQ(point.z, 0);
    EXPECT_EQ(RaySphereSIMDCore(SIMakeRay(VxVector(0, 0, 0), VxVector(0, 0, 0)), s, nullptr, nullptr), 0);
    EXPECT_TRUE(SphereSphereSIMDCore(s, s.Center(), s, s.Center(), nullptr, nullptr));
#endif
}
TEST(SphereIntersectionReference, PublicTablesPreserveGeometry) {
    for (int i = 0; i < 4; ++i) { VxIntersectSphereDispatchRebuild(i & 1); EXPECT_EQ(SIRayScales(VxIntersect::RaySphere).errors, 0); EXPECT_EQ(SIMovementScales(VxIntersect::SphereSphere).errors, 0); EXPECT_EQ(SIBoxSpecial(VxIntersect::SphereAABB).errors, 0); }
    VxIntersectSphereDispatchRebuild(false); SICheck(SIMovements(VxIntersect::SphereSphere));
}
#if defined(_WIN32)
TEST(SphereIntersectionReference, ExactReadonlyInputsAndOutputFootprints) { SICheck(SIFootprints(SphereAABBScalarCore, RaySphereScalarCore, SphereSphereScalarCore));
#if defined(VX_SIMD_SSE)
    EXPECT_EQ(SIFootprints(SphereAABBMSIMDCore, RaySphereSIMDCore, SphereSphereSIMDCore).errors, 0);
#endif
}
TEST(SphereIntersectionReference, ActualCurrentDllExports) {
    HMODULE dll = LoadLibraryA("VxMath.dll"); ASSERT_NE(dll, nullptr);
#if defined(_WIN64)
    const char *boxName = "?SphereAABB@VxIntersect@@SAHAEBVVxSphere@@AEBUVxBbox@@@Z", *rayName = "?RaySphere@VxIntersect@@SAHAEBVVxRay@@AEBVVxSphere@@PEAUVxVector@@2@Z", *moveName = "?SphereSphere@VxIntersect@@SAHAEBVVxSphere@@AEBUVxVector@@01PEAM2@Z";
#else
    const char *boxName = "?SphereAABB@VxIntersect@@SAHABVVxSphere@@ABUVxBbox@@@Z", *rayName = "?RaySphere@VxIntersect@@SAHABVVxRay@@ABVVxSphere@@PAUVxVector@@2@Z", *moveName = "?SphereSphere@VxIntersect@@SAHABVVxSphere@@ABUVxVector@@01PAM2@Z";
#endif
    SIBox box = reinterpret_cast<SIBox>(GetProcAddress(dll, boxName)); SIRay ray = reinterpret_cast<SIRay>(GetProcAddress(dll, rayName)); SIMove move = reinterpret_cast<SIMove>(GetProcAddress(dll, moveName));
    ASSERT_NE(box, nullptr); ASSERT_NE(ray, nullptr); ASSERT_NE(move, nullptr);
    SICheck(SIBoxes(box)); EXPECT_EQ(SIBoxSpecial(box).errors, 0); EXPECT_EQ(SIRays(ray).errors, 0); EXPECT_EQ(SIRayScales(ray).errors, 0); EXPECT_EQ(SIRayWide(ray).errors, 0); EXPECT_EQ(SIMovements(move).errors, 0); EXPECT_EQ(SIMovementScales(move).errors, 0);
    EXPECT_NE(FreeLibrary(dll), 0);
}
#endif
#endif
