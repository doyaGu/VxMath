#include <cmath>
#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>

#if defined(VX_DISTANCE_ABI_SDK_PROBE)
#include "C:/Users/kakut/Works/Virtools/Virtools-SDK-2.1/Include/VxRay.h"
#include "C:/Users/kakut/Works/Virtools/Virtools-SDK-2.1/Include/VxDistance.h"
#else
#include "VxRay.h"
#include "VxDistance.h"
#include <gtest/gtest.h>
#endif

typedef float (*DistanceAbiPairFn)(const VxRay &, const VxRay &, float *, float *);
typedef float (*DistanceAbiPointFn)(const VxVector &, const VxRay &, float *);

#define DISTANCE_PAIR_SIGNATURE(name) \
    static_assert(std::is_same<decltype(&VxDistance::name), DistanceAbiPairFn>::value, #name " static float ABI")
#define DISTANCE_POINT_SIGNATURE(name) \
    static_assert(std::is_same<decltype(&VxDistance::name), DistanceAbiPointFn>::value, #name " static float ABI")
DISTANCE_PAIR_SIGNATURE(LineLineSquareDistance);
DISTANCE_PAIR_SIGNATURE(LineRaySquareDistance);
DISTANCE_PAIR_SIGNATURE(LineSegmentSquareDistance);
DISTANCE_PAIR_SIGNATURE(RayRaySquareDistance);
DISTANCE_PAIR_SIGNATURE(RaySegmentSquareDistance);
DISTANCE_PAIR_SIGNATURE(SegmentSegmentSquareDistance);
DISTANCE_PAIR_SIGNATURE(LineLineDistance);
DISTANCE_PAIR_SIGNATURE(LineRayDistance);
DISTANCE_PAIR_SIGNATURE(LineSegmentDistance);
DISTANCE_PAIR_SIGNATURE(RayRayDistance);
DISTANCE_PAIR_SIGNATURE(RaySegmentDistance);
DISTANCE_PAIR_SIGNATURE(SegmentSegmentDistance);
DISTANCE_POINT_SIGNATURE(PointLineSquareDistance);
DISTANCE_POINT_SIGNATURE(PointRaySquareDistance);
DISTANCE_POINT_SIGNATURE(PointSegmentSquareDistance);
DISTANCE_POINT_SIGNATURE(PointLineDistance);
DISTANCE_POINT_SIGNATURE(PointRayDistance);
DISTANCE_POINT_SIGNATURE(PointSegmentDistance);
#undef DISTANCE_PAIR_SIGNATURE
#undef DISTANCE_POINT_SIGNATURE

static bool DistanceAbiLayout() {
    return sizeof(VxDistance) == 1 && alignof(VxDistance) == 1
        && std::is_empty<VxDistance>::value && std::is_standard_layout<VxDistance>::value
        && !std::is_polymorphic<VxDistance>::value && !std::has_virtual_destructor<VxDistance>::value
        && std::is_trivially_default_constructible<VxDistance>::value
        && std::is_trivially_copy_constructible<VxDistance>::value
        && std::is_trivially_copy_assignable<VxDistance>::value
        && std::is_trivially_destructible<VxDistance>::value
        && sizeof(VxVector) == 12 && alignof(VxVector) == 4
        && offsetof(VxVector, x) == 0 && offsetof(VxVector, y) == 4 && offsetof(VxVector, z) == 8
        && sizeof(VxRay) == 24 && alignof(VxRay) == 4
        && offsetof(VxRay, m_Origin) == 0 && offsetof(VxRay, m_Direction) == 12;
}

static bool DistanceAbiLifecycle() {
    unsigned char bytes[4] = {0x31, 0x42, 0x53, 0x64};
    VxDistance *first = new (bytes + 1) VxDistance;
    VxDistance *second = new (bytes + 2) VxDistance(*first);
    *first = *second;
    first->~VxDistance();
    second->~VxDistance();
    return bytes[0] == 0x31 && bytes[3] == 0x64;
}

struct DistanceAbiOutput {
    float before = 31.0f, value = 137.0f, after = 47.0f;
};

static bool DistanceAbiPair(int index, DistanceAbiPairFn square, DistanceAbiPairFn wrapper) {
    VxRay a, b;
    a.m_Origin = VxVector(0, 0, 0); a.m_Direction = VxVector(1, 0, 0);
    b.m_Origin = VxVector(.5f, index < 2 ? .5f : -.5f, 2); b.m_Direction = VxVector(0, 1, 0);
    const float expectedS = .5f, expectedT = index == 0 ? -.5f : index == 1 ? 0.0f : .5f;
    const float expectedSquare = index == 1 ? 4.25f : 4.0f;
    const VxRay beforeA = a, beforeB = b;
    for (int root = 0; root < 2; ++root) for (int mask = 0; mask < 4; ++mask) {
        DistanceAbiOutput s, t;
        const float result = (root ? wrapper : square)(a, b, mask & 1 ? &s.value : nullptr, mask & 2 ? &t.value : nullptr);
        if (result != (root ? std::sqrt(expectedSquare) : expectedSquare)
            || s.value != (mask & 1 ? expectedS : 137.0f) || t.value != (mask & 2 ? expectedT : 137.0f)
            || s.before != 31 || s.after != 47 || t.before != 31 || t.after != 47
            || std::memcmp(&a, &beforeA, sizeof(a)) || std::memcmp(&b, &beforeB, sizeof(b))) return false;
    }
    return true;
}

static bool DistanceAbiPoint(int index, DistanceAbiPointFn square, DistanceAbiPointFn wrapper) {
    VxRay ray; ray.m_Origin = VxVector(0, 0, 0); ray.m_Direction = VxVector(1, 0, 0);
    VxVector point(index == 0 ? .5f : index == 1 ? -.5f : 2.0f, .5f, 2);
    const VxRay beforeRay = ray; const VxVector beforePoint = point;
    const float expectedParameter = index == 0 ? .5f : index == 1 ? 0.0f : 1.0f;
    const float expectedSquare = index == 0 ? 4.25f : index == 1 ? 4.5f : 5.25f;
    for (int root = 0; root < 2; ++root) for (int output = 0; output < 2; ++output) {
        DistanceAbiOutput parameter;
        const float result = (root ? wrapper : square)(point, ray, output ? &parameter.value : nullptr);
        if (result != (root ? std::sqrt(expectedSquare) : expectedSquare)
            || parameter.value != (output ? expectedParameter : 137.0f)
            || parameter.before != 31 || parameter.after != 47
            || std::memcmp(&ray, &beforeRay, sizeof(ray)) || std::memcmp(&point, &beforePoint, sizeof(point))) return false;
    }
    return true;
}

#if !defined(VX_DISTANCE_ABI_SDK_PROBE)
TEST(DistanceAbiReference,EmptyStaticClassAndArgumentLayouts) { EXPECT_TRUE(DistanceAbiLayout()); }
TEST(DistanceAbiReference,ImplicitLifecycleKeepsAdjacentStorage) { EXPECT_TRUE(DistanceAbiLifecycle()); }
#define DISTANCE_PAIR_CASE(name, index) \
    TEST(DistanceAbiReference, name##ExportAndInlineWrapper) { EXPECT_TRUE(DistanceAbiPair(index, VxDistance::name##SquareDistance, VxDistance::name##Distance)); }
DISTANCE_PAIR_CASE(LineLine, 0)
DISTANCE_PAIR_CASE(LineRay, 1)
DISTANCE_PAIR_CASE(LineSegment, 2)
DISTANCE_PAIR_CASE(RayRay, 3)
DISTANCE_PAIR_CASE(RaySegment, 4)
DISTANCE_PAIR_CASE(SegmentSegment, 5)
#define DISTANCE_POINT_CASE(name, index) \
    TEST(DistanceAbiReference, name##ExportAndInlineWrapper) { EXPECT_TRUE(DistanceAbiPoint(index, VxDistance::name##SquareDistance, VxDistance::name##Distance)); }
DISTANCE_POINT_CASE(PointLine, 0)
DISTANCE_POINT_CASE(PointRay, 1)
DISTANCE_POINT_CASE(PointSegment, 2)
#undef DISTANCE_PAIR_CASE
#undef DISTANCE_POINT_CASE
#endif
