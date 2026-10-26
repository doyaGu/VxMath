#include <math.h>
#include <new>
#include <string.h>
#ifndef VX_VECTOR4_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

struct Vector4Checks {
    int calls = 0, errors = 0;
    void Check(bool valid) { ++calls; errors += !valid; }
    void Vector(const VxVector4 &value, float x, float y, float z, float w) {
        Check(value.x == x && value.y == y && value.z == z && value.w == w);
    }
};

static char AcceptVector4(VxVector4);
static long AcceptVector4(...);

static Vector4Checks Vector4Construction() {
    Vector4Checks checks;
    VxVector4 zero, uniform(-2), components(2,-3,5,-7);
    const float values[] = {2,-3,5,-7};
    VxVector4 array(values);
    checks.Vector(zero, 0,0,0,0);
    checks.Vector(uniform, -2,-2,-2,-2);
    checks.Vector(components, 2,-3,5,-7);
    checks.Vector(array, 2,-3,5,-7);
    checks.Check(sizeof(AcceptVector4(1.0f)) == sizeof(long));
    checks.Check(sizeof(AcceptVector4(static_cast<const float *>(nullptr))) == sizeof(long));
    checks.Check(sizeof(VxVector4) == 16 && alignof(VxVector4) == 4);
    checks.Check(reinterpret_cast<const char *>(&array.w) - reinterpret_cast<const char *>(&array) == 12);
    checks.Check(static_cast<VxVector *>(&array) == reinterpret_cast<VxVector *>(&array));
    checks.Check(__is_trivially_copyable(VxVector4));
    VxVector4 copy(array), moved(static_cast<VxVector4 &&>(copy));
    copy = components;
    moved = static_cast<VxVector4 &&>(copy);
    checks.Vector(moved, 2,-3,5,-7);
    VxVector4 sequence[3];
    checks.Check(sizeof(sequence) == 48 && reinterpret_cast<char *>(&sequence[1]) - reinterpret_cast<char *>(&sequence[0]) == 16);
    return checks;
}

#ifndef VX_VECTOR4_PROBE
static void ExpectVector4(Vector4Checks checks) { EXPECT_GT(checks.calls, 0); EXPECT_EQ(checks.errors, 0); }
TEST(Vector4Reference, ConstructorsAreExplicitAndLayoutHasFourFloats) { ExpectVector4(Vector4Construction()); }
#endif
