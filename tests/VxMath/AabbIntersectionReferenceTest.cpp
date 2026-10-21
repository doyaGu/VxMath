#include <cmath>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <new>
#ifndef VX_AABB_INTERSECTION_NATIVE_PROBE
#ifndef VX_LIB
#define VX_LIB 1
#endif
#include "../../src/VxIntersect_BoxBox.cpp"
#include <gtest/gtest.h>
// This fixture selects only its private box table. Other modules' rebuild
// hooks are link dependencies of the unused all-module dispatcher; fail if
// one is accidentally exercised rather than pretending to test that module.
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

typedef int (*ABFunction)(const VxBbox &, const VxBbox &);
struct ABResults { int samples = 0, errors = 0; unsigned digest = 2166136261u; };
static void ABObserve(ABResults &r, int actual, bool expected) {
    ++r.samples;
    if (actual != int(expected)) ++r.errors;
    r.digest = (r.digest ^ unsigned(actual)) * 16777619u;
}
static const int ABBases[][6] = {
    {-2, -2, -2, 2, 2, 2}, {-1, -1, -1, 1, 1, 1}, {0, 0, 0, 0, 0, 0},
    {-2, 0, 0, 2, 0, 0}, {0, -2, -2, 0, 2, 2}, {-2, 1, -1, -1, 2, 1},
    {1, -1, -1, 2, 1, 1}, {-1, -2, -1, 1, -1, 2}
};
static VxBbox ABMakeBox(const int *bounds) {
    return VxBbox(VxVector(float(bounds[0]), float(bounds[1]), float(bounds[2])),
        VxVector(float(bounds[3]), float(bounds[4]), float(bounds[5])));
}
// Every intersection of these integer-bounded boxes contains an integer
// lattice point. A bitset of those points gives an independent oracle, without
// repeating the production six endpoint comparisons or computing centers.
static void ABMask(const int *bounds, std::uint64_t *mask) {
    mask[0] = mask[1] = 0;
    int index = 0;
    for (int x = -2; x <= 2; ++x) for (int y = -2; y <= 2; ++y) for (int z = -2; z <= 2; ++z, ++index)
        if (x >= bounds[0] && x <= bounds[3] && y >= bounds[1] && y <= bounds[4] && z >= bounds[2] && z <= bounds[5])
            mask[index / 64] |= std::uint64_t(1) << (index % 64);
}
static ABResults ABGrid(ABFunction fn) {
    ABResults r;
    for (const auto &base : ABBases) {
        const VxBbox first = ABMakeBox(base);
        std::uint64_t firstMask[2]; ABMask(base, firstMask);
        for (int x = -2; x <= 2; ++x) for (int endX = x; endX <= 2; ++endX)
            for (int y = -2; y <= 2; ++y) for (int endY = y; endY <= 2; ++endY)
                for (int z = -2; z <= 2; ++z) for (int endZ = z; endZ <= 2; ++endZ) {
                    const int bounds[] = {x, y, z, endX, endY, endZ};
                    const VxBbox second = ABMakeBox(bounds);
                    std::uint64_t secondMask[2]; ABMask(bounds, secondMask);
                    const bool expected = ((firstMask[0] & secondMask[0]) | (firstMask[1] & secondMask[1])) != 0;
                    ABObserve(r, fn(first, second), expected);
                    ABObserve(r, fn(second, first), expected);
                }
    }
    return r;
}
static ABResults ABBoundaries(ABFunction fn) {
    ABResults r;
    const int powers[] = {-120, -90, -40, -10, 0, 10, 40, 90, 120};
    for (int exponent : powers) {
        const float size = std::ldexp(1.0f, exponent);
        const VxBbox box(VxVector(-size, -size, -size), VxVector(size, size, size));
        for (int axis = 0; axis < 3; ++axis) for (int side = -1; side <= 1; side += 2)
            for (int neighbor = -1; neighbor <= 1; ++neighbor) {
                const float boundary = side * size;
                VxVector point(0, 0, 0);
                point[axis] = neighbor == 0 ? boundary : std::nextafter(boundary, neighbor < 0 ? -FLT_MAX : FLT_MAX);
                const VxBbox other(point, point);
                const bool expected = neighbor == 0 || (side < 0 ? neighbor > 0 : neighbor < 0);
                ABObserve(r, fn(box, other), expected);
                ABObserve(r, fn(other, box), expected);
            }
    }
    const VxBbox huge(VxVector(-FLT_MAX, -FLT_MAX, -FLT_MAX), VxVector(FLT_MAX, FLT_MAX, FLT_MAX));
    const VxBbox positive(VxVector(FLT_MAX / 2, FLT_MAX / 2, FLT_MAX / 2), VxVector(FLT_MAX, FLT_MAX, FLT_MAX));
    const VxBbox negative(VxVector(-FLT_MAX, -FLT_MAX, -FLT_MAX), VxVector(-FLT_MAX / 2, -FLT_MAX / 2, -FLT_MAX / 2));
    const VxBbox endpoint(VxVector(FLT_MAX, FLT_MAX, FLT_MAX), VxVector(FLT_MAX, FLT_MAX, FLT_MAX));
    const VxBbox negativeZero(VxVector(-0.0f, -0.0f, -0.0f), VxVector(-0.0f, -0.0f, -0.0f));
    const VxBbox positiveZero(VxVector(0, 0, 0), VxVector(0, 0, 0));
    ABObserve(r, fn(huge, positive), true); ABObserve(r, fn(positive, huge), true);
    ABObserve(r, fn(negative, positive), false); ABObserve(r, fn(positive, negative), false);
    ABObserve(r, fn(positive, endpoint), true); ABObserve(r, fn(endpoint, positive), true);
    ABObserve(r, fn(negativeZero, positiveZero), true); ABObserve(r, fn(positiveZero, negativeZero), true);
    return r;
}
static ABResults ABSelf(ABFunction fn) {
    ABResults r;
    for (const auto &bounds : ABBases) {
        const VxBbox box = ABMakeBox(bounds);
        ABObserve(r, fn(box, box), true);
    }
    return r;
}
#if defined(_WIN32)
static ABResults ABFootprint(ABFunction fn) {
    ABResults r; SYSTEM_INFO system; GetSystemInfo(&system);
    unsigned char *pages = static_cast<unsigned char *>(VirtualAlloc(nullptr, system.dwPageSize * 4,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!pages) { ABObserve(r, 0, true); return r; }
    VxBbox *first = new(pages + system.dwPageSize - sizeof(VxBbox)) VxBbox(VxVector(-1, -1, -1), VxVector(1, 1, 1));
    VxBbox *second = new(pages + system.dwPageSize * 3 - sizeof(VxBbox)) VxBbox(VxVector(1, 1, 1), VxVector(2, 2, 2));
    DWORD old;
    if (VirtualProtect(pages + system.dwPageSize, system.dwPageSize, PAGE_NOACCESS, &old) &&
        VirtualProtect(pages + system.dwPageSize * 3, system.dwPageSize, PAGE_NOACCESS, &old) &&
        VirtualProtect(pages, system.dwPageSize, PAGE_READONLY, &old) &&
        VirtualProtect(pages + system.dwPageSize * 2, system.dwPageSize, PAGE_READONLY, &old)) {
        ABObserve(r, fn(*first, *second), true); ABObserve(r, fn(*second, *first), true); ABObserve(r, fn(*second, *second), true);
    } else ABObserve(r, 0, true);
    VirtualFree(pages, 0, MEM_RELEASE);
    return r;
}
#endif
#ifndef VX_AABB_INTERSECTION_NATIVE_PROBE
static void ABCheck(ABResults r) {
    EXPECT_EQ(r.errors, 0);
    ::testing::Test::RecordProperty("samples", r.samples);
    ::testing::Test::RecordProperty("digest", std::to_string(r.digest));
}
TEST(AabbIntersectionReference, ScalarLatticeAndSwappedArguments) { ABCheck(ABGrid(AABBAABBCore<false>)); }
TEST(AabbIntersectionReference, ScalarOneUlpBoundaryAndFiniteExtremes) { ABCheck(ABBoundaries(AABBAABBCore<false>)); }
TEST(AabbIntersectionReference, ScalarIdenticalInputAddresses) { ABCheck(ABSelf(AABBAABBCore<false>)); }
#if defined(VX_SIMD_SSE)
TEST(AabbIntersectionReference, SimdLatticeAndSwappedArguments) { ABCheck(ABGrid(AABBAABBCore<true>)); }
TEST(AabbIntersectionReference, SimdOneUlpBoundaryAndFiniteExtremes) { ABCheck(ABBoundaries(AABBAABBCore<true>)); }
TEST(AabbIntersectionReference, SimdIdenticalInputAddresses) { ABCheck(ABSelf(AABBAABBCore<true>)); }
#endif
TEST(AabbIntersectionReference, PublicForwarderUsesSelectedBoxTable) {
    const VxIntersectBoxDispatchTable *saved = GetVxIntersectBoxDispatchTable();
    for (int n = 0; n < 4; ++n) {
        const VxIntersectBoxDispatchTable *table = &kVxIntersectBoxDispatchScalar;
#if defined(VX_SIMD_SSE)
        if (n & 1) table = &kVxIntersectBoxDispatchSIMD;
#endif
        g_VxIntersectBoxDispatch.store(table, std::memory_order_release);
        ABCheck(ABGrid(VxIntersect::AABBAABB));
        EXPECT_EQ(ABBoundaries(VxIntersect::AABBAABB).errors, 0);
    }
    g_VxIntersectBoxDispatch.store(saved, std::memory_order_release);
}
#if defined(_WIN32)
TEST(AabbIntersectionReference, ScalarReadonlyExact24ByteInputs) { ABCheck(ABFootprint(AABBAABBCore<false>)); }
#if defined(VX_SIMD_SSE)
TEST(AabbIntersectionReference, SimdReadonlyExact24ByteInputs) { ABCheck(ABFootprint(AABBAABBCore<true>)); }
#endif
TEST(AabbIntersectionReference, ActualCurrentDllExport) {
    HMODULE dll = LoadLibraryA("VxMath.dll"); ASSERT_NE(dll, nullptr);
#if defined(_WIN64)
    const char *symbol = "?AABBAABB@VxIntersect@@SAHAEBUVxBbox@@0@Z";
#else
    const char *symbol = "?AABBAABB@VxIntersect@@SAHABUVxBbox@@0@Z";
#endif
    ABFunction fn = reinterpret_cast<ABFunction>(GetProcAddress(dll, symbol)); ASSERT_NE(fn, nullptr);
    ABCheck(ABGrid(fn)); EXPECT_EQ(ABBoundaries(fn).errors, 0); EXPECT_EQ(ABSelf(fn).errors, 0); EXPECT_EQ(ABFootprint(fn).errors, 0);
    EXPECT_NE(FreeLibrary(dll), 0);
}
#endif
#endif
