#include <stddef.h>
#include <new>
#include <string.h>
#ifndef VX_COMPRESSED_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif

struct CompressedChecks {
    int checks = 0, errors = 0;
    void Check(bool valid) { ++checks; errors += !valid; }
    template<class T> void Angles(const T &value, int pitch, int yaw) {
        Check(value.xa == pitch && value.ya == yaw);
    }
};

template<class T> static CompressedChecks CompressedLayout(int size, int alignment, int yawOffset) {
    CompressedChecks checks;
    checks.Check(sizeof(T) == size && alignof(T) == alignment);
    checks.Check(offsetof(T, xa) == 0 && offsetof(T, ya) == yawOffset);
    checks.Check(__is_standard_layout(T) && __is_trivially_copyable(T));
    T value;
    checks.Angles(value, 0, 0);
    value.xa = -123;
    value.ya = 456;
    T copy(value), moved(static_cast<T &&>(copy));
    checks.Angles(moved, -123, 456);
    copy = value;
    moved = static_cast<T &&>(copy);
    checks.Angles(moved, -123, 456);
    T array[2];
    checks.Check(reinterpret_cast<char *>(&array[1]) - reinterpret_cast<char *>(&array[0]) == size);
    return checks;
}

template<class T> static CompressedChecks CompressedEncoding() {
    CompressedChecks checks;
    // The last input deliberately is not normalized: Set uses asin(Y), atan2(X, Z).
    const float input[][3] = {
        {0,0,1}, {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,-1}, {1,0,1}, {1,.5f,1}
    };
    const int angles[][2] = {{0,0}, {0,4096}, {0,-4096}, {-4096,0}, {4096,0}, {0,8192}, {0,2048}, {-1365,2048}};
    for (int i = 0; i < 8; ++i) {
        T constructed(input[i][0], input[i][1], input[i][2]), assigned, set;
        set.Set(input[i][0], input[i][1], input[i][2]);
        VxVector vector(input[i][0], input[i][1], input[i][2]);
        checks.Check(&(assigned = vector) == &assigned);
        checks.Angles(constructed, angles[i][0], angles[i][1]);
        checks.Angles(set, angles[i][0], angles[i][1]);
        checks.Angles(assigned, angles[i][0], angles[i][1]);
    }
    return checks;
}

static CompressedChecks CompressedConversion() {
    CompressedChecks checks;
    const int values[] = {-32768, -8192, -1, 0, 4096, 32767};
    for (int i = 0; i < 6; ++i) {
        VxCompressedVector packed;
        packed.xa = static_cast<short>(values[i]);
        packed.ya = static_cast<short>(values[5-i]);
        VxCompressedVectorOld old;
        checks.Check(&(old = packed) == &old);
        checks.Angles(old, values[i], values[5-i]);
        VxCompressedVector restored;
        checks.Check(&(restored = old) == &restored);
        checks.Angles(restored, values[i], values[5-i]);
    }
    VxCompressedVectorOld old;
    old.xa = 65537;
    old.ya = -65538;
    VxCompressedVector packed;
    packed = old;
    checks.Angles(packed, 1, -2);
    return checks;
}

template<class T> static CompressedChecks CompressedInterpolation() {
    CompressedChecks checks;
    T a, b, result;
    a.xa = -100; a.ya = 1000;
    b.xa = 300; b.ya = 3000;
    const float steps[] = {0, .25f, .5f, 1, -.5f, 1.5f};
    const int pitch[] = {-100, 0, 100, 300, -300, 500};
    const int yaw[] = {1000, 1500, 2000, 3000, 0, 4000};
    for (int i = 0; i < 6; ++i) {
        result.Slerp(steps[i], a, b);
        checks.Angles(result, pitch[i], yaw[i]);
        checks.Angles(a, -100, 1000);
        checks.Angles(b, 300, 3000);
    }
    // At half a turn the sign of the unwrapped delta selects the direction.
    a.xa = b.xa = 0; a.ya = 0; b.ya = 8192;
    result.Slerp(.5f, a, b);
    checks.Angles(result, 0, 4096);
    result.Slerp(.5f, b, a);
    checks.Angles(result, 0, 4096);
    // Stored high bits do not change the yaw angle.
    a.ya = -16384; b.ya = 20480;
    result.Slerp(.5f, a, b);
    checks.Angles(result, 0, 2048);
    return checks;
}

static CompressedChecks CompressedRounding() {
    CompressedChecks checks;
    VxCompressedVector a, b, result;
    VxCompressedVectorOld oldA, oldB, oldResult;
    b.xa = oldB.xa = -1;
    result.Slerp(.5f, a, b);
    oldResult.Slerp(.5f, oldA, oldB);
    checks.Angles(result, -1, 0);
    checks.Angles(oldResult, 0, 0);
    b.xa = oldB.xa = 1000;
    result.Slerp(.1f, a, b);
    oldResult.Slerp(.1f, oldA, oldB);
    checks.Angles(result, 99, 0);
    checks.Angles(oldResult, 100, 0);
    return checks;
}

template<class T> static CompressedChecks CompressedYawSeam() {
    CompressedChecks checks;
    const int yaw[][3] = {{100,-100,0}, {-100,100,16384}, {8000,-8000,8192}, {-8000,8000,8192}, {16000,200,16292}, {200,16000,-92}};
    for (int i = 0; i < 6; ++i) {
        T a, b, result;
        a.xa = -100; a.ya = yaw[i][0];
        b.xa = 300; b.ya = yaw[i][1];
        result.Slerp(0, a, b);
        checks.Check(result.xa == a.xa && (result.ya & 16383) == (a.ya & 16383));
        result.Slerp(.5f, a, b);
        checks.Angles(result, 100, yaw[i][2]);
        result.Slerp(1, a, b);
        checks.Check(result.xa == b.xa && (result.ya & 16383) == (b.ya & 16383));
        T left(a), right(b);
        left.Slerp(.5f, left, b);
        right.Slerp(.5f, a, right);
        checks.Angles(left, 100, yaw[i][2]);
        checks.Angles(right, 100, yaw[i][2]);
    }
    return checks;
}

template<class T> static CompressedChecks CompressedFootprint() {
    CompressedChecks checks;
    struct Storage { unsigned int before; T value; unsigned int after; } storage;
    memset(&storage, 0xA5, sizeof(storage));
    T *value = new (&storage.value) T;
    checks.Angles(*value, 0, 0);
    T a(0,0,1), b(1,0,0);
    value->Set(0,1,0);
    *value = VxVector(0,0,1);
    value->Slerp(.5f, a, b);
    checks.Angles(*value, 0, 2048);
    value->~T();
    checks.Check(storage.before == 0xA5A5A5A5 && storage.after == 0xA5A5A5A5);
    return checks;
}

#ifndef VX_COMPRESSED_PROBE
TEST(CompressedVectorReferenceTest, LayoutAndImplicitLifecycle) {
    EXPECT_EQ(0, CompressedLayout<VxCompressedVector>(4,2,2).errors);
    EXPECT_EQ(0, CompressedLayout<VxCompressedVectorOld>(8,4,4).errors);
}
TEST(CompressedVectorReferenceTest, ConstructorsSetAndVectorAssignmentUsePolarAngles) {
    EXPECT_EQ(0, CompressedEncoding<VxCompressedVector>().errors);
    EXPECT_EQ(0, CompressedEncoding<VxCompressedVectorOld>().errors);
}
TEST(CompressedVectorReferenceTest, CrossFormatAssignmentPreservesStoredAngleBits) {
    EXPECT_EQ(0, CompressedConversion().errors);
}
TEST(CompressedVectorReferenceTest, InterpolationEndpointsExtrapolationAndHalfTurns) {
    EXPECT_EQ(0, CompressedInterpolation<VxCompressedVector>().errors);
    EXPECT_EQ(0, CompressedInterpolation<VxCompressedVectorOld>().errors);
}
TEST(CompressedVectorReferenceTest, NewAndOldFormatsKeepTheirDistinctRounding) {
    EXPECT_EQ(0, CompressedRounding().errors);
}
TEST(CompressedVectorReferenceTest, PackedYawCrossesTheSeamInBothDirectionsAndAllowsAliasing) {
    EXPECT_EQ(0, CompressedYawSeam<VxCompressedVector>().errors);
}
TEST(CompressedVectorReferenceTest, OldYawCrossesTheSeamInBothDirectionsAndAllowsAliasing) {
    EXPECT_EQ(0, CompressedYawSeam<VxCompressedVectorOld>().errors);
}
TEST(CompressedVectorReferenceTest, WritesStayWithinEachFormatsObjectSize) {
    EXPECT_EQ(0, CompressedFootprint<VxCompressedVector>().errors);
    EXPECT_EQ(0, CompressedFootprint<VxCompressedVectorOld>().errors);
}
#endif
