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

static VxVector4 &Vector4Mixed(int operation, VxVector4 &value, const VxVector &operand) {
    switch (operation) {
    case 0: return value += operand;
    case 1: return value -= operand;
    case 2: return value *= operand;
    default: return value /= operand;
    }
}

static unsigned Vector4WBits(const VxVector4 &value) {
    unsigned bits;
    memcpy(&bits, &value.w, sizeof(bits));
    return bits;
}

static Vector4Checks Vector4MixedCases() {
    Vector4Checks checks;
    const float expected[][3] = {{8,-4,9},{4,-12,15},{12,-32,-36},{3,-2,-4}};
    const unsigned values[] = {0u,0x80000000u,0x80000001u,0xff800000u,0xffc12345u};
    for (int operation = 0; operation < 4; ++operation) {
        for (unsigned w : values) {
            VxVector4 value(6,-8,12,0);
            memcpy(&value.w, &w, sizeof(w));
            const VxVector operand(2,4,-3);
            checks.Check(&Vector4Mixed(operation, value, operand) == &value);
            checks.Check(value.x == expected[operation][0] && value.y == expected[operation][1] && value.z == expected[operation][2]);
            checks.Check(Vector4WBits(value) == w);
        }
    }
    VxVector4 value(2,4,8,-7);
    Vector4Mixed(0, value, static_cast<const VxVector &>(value));
    checks.Vector(value, 4,8,16,-7);
    return checks;
}

#if defined(_WIN32)
static bool Vector4CallMixed(int operation, VxVector4 *value, const VxVector *operand) {
    __try {
        return &Vector4Mixed(operation, *value, *operand) == value;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static Vector4Checks Vector4InaccessibleW() {
    Vector4Checks checks;
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    const size_t page = info.dwPageSize;
    unsigned char *memory = static_cast<unsigned char *>(VirtualAlloc(nullptr, 4 * page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!memory) { checks.Check(false); return checks; }
    VxVector4 *value = ::new(memory + 2 * page - 12) VxVector4(6,-8,12,-7);
    DWORD old;
    if (!VirtualProtect(memory + 2 * page, page, PAGE_NOACCESS, &old)) {
        VirtualFree(memory, 0, MEM_RELEASE); checks.Check(false); return checks;
    }
    const VxVector operand(2,4,-3);
    const float expected[][3] = {{8,-4,9},{4,-12,15},{12,-32,-36},{3,-2,-4}};
    for (int operation = 0; operation < 4; ++operation) {
        value->Set(6,-8,12);
        const bool called = Vector4CallMixed(operation, value, &operand);
        checks.Check(called);
        if (called) checks.Check(value->x == expected[operation][0] && value->y == expected[operation][1] && value->z == expected[operation][2]);
    }
    VirtualFree(memory, 0, MEM_RELEASE);
    return checks;
}
#endif

#ifndef VX_VECTOR4_PROBE
static void ExpectVector4(Vector4Checks checks) { EXPECT_GT(checks.calls, 0); EXPECT_EQ(checks.errors, 0); }
TEST(Vector4Reference, ConstructorsAreExplicitAndLayoutHasFourFloats) { ExpectVector4(Vector4Construction()); }
TEST(Vector4Reference, MixedArithmeticPreservesEveryWBit) { ExpectVector4(Vector4MixedCases()); }
#if defined(_WIN32)
TEST(Vector4Reference, MixedArithmeticDoesNotReadOrWriteTheWField) { ExpectVector4(Vector4InaccessibleW()); }
#endif
#endif
