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

static Vector4Checks Vector4Access() {
    Vector4Checks checks;
    VxVector4 value(1,2,3,-4);
    const VxVector4 &reference = value;
    checks.Check(&value[0] == &value.x && &value[1] == &value.y && &value[2] == &value.z && &value[3] == &value.w);
    checks.Check(&reference[0] == &reference.x && &reference[3] == &reference.w);
    value[3] = -7;
    checks.Check(reference.w == -7);
    float *pointer = reference;
    checks.Check(pointer == &value.x && &pointer[3] == &value.w);
    pointer[3] = -9;
    checks.Check(value.w == -9);
    value.Set(4,5,6);
    checks.Vector(value, 4,5,6,-9);
    value.Set(1,2,3,4);
    checks.Vector(value, 1,2,3,4);
    const VxVector source(6,-8,12);
    checks.Check(&(value = source) == &value);
    checks.Vector(value, 6,-8,12,4);
    checks.Check(&(value = static_cast<const VxVector &>(value)) == &value);
    checks.Vector(value, 6,-8,12,4);
    return checks;
}

static const float Vector4ArithmeticResults[][4] = {
    {8,-4,9,12}, {4,-12,15,4}, {12,-32,-36,32}, {3,-2,-4,2},
    {3,-4,6,4}, {3,-4,6,4}, {3,-4,6,4}, {6,-8,12,8}, {-6,8,-12,-8},
    {8,-6,14,10}, {4,-10,10,6}, {8,-4,9,12}, {4,-12,15,4},
    {12,-32,-36,32}, {3,-2,-4,2}, {3,-4,6,4}, {3,-4,6,4}
};

static VxVector4 Vector4Arithmetic(int operation, VxVector4 &a, const VxVector4 &b, bool &reference) {
    reference = true;
    switch (operation) {
    case 0: return a + b;
    case 1: return a - b;
    case 2: return a * b;
    case 3: return a / b;
    case 4: return a * 0.5f;
    case 5: return 0.5f * a;
    case 6: return a / 2.0f;
    case 7: return +a;
    case 8: return -a;
    case 9: return a + 2.0f;
    case 10: return a - 2.0f;
    case 11: reference = &(a += b) == &a; return a;
    case 12: reference = &(a -= b) == &a; return a;
    case 13: reference = &(a *= b) == &a; return a;
    case 14: reference = &(a /= b) == &a; return a;
    case 15: reference = &(a *= 0.5f) == &a; return a;
    case 16: reference = &(a /= 2.0f) == &a; return a;
    default: reference = false; return a;
    }
}

static Vector4Checks Vector4ArithmeticCases() {
    Vector4Checks checks;
    for (int operation = 0; operation < 17; ++operation) {
        VxVector4 a(6,-8,12,8), b(2,4,-3,4);
        bool reference;
        const VxVector4 result = Vector4Arithmetic(operation, a, b, reference);
        const float *expected = Vector4ArithmeticResults[operation];
        checks.Vector(result, expected[0], expected[1], expected[2], expected[3]);
        checks.Check(reference);
        checks.Vector(b, 2,4,-3,4);
        if (operation < 11) checks.Vector(a, 6,-8,12,8);
    }
    VxVector4 value(2,-4,8,16);
    value += value; checks.Vector(value, 4,-8,16,32);
    value -= value; checks.Vector(value, 0,0,0,0);
    value.Set(2,-4,8,16); value *= value; checks.Vector(value, 4,16,64,256);
    value.Set(2,-4,8,16); value /= value; checks.Vector(value, 1,1,1,1);
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

static Vector4Checks Vector4DotAndEquality() {
    Vector4Checks checks;
    VxVector4 a(1,2,3,100), b(4,5,6,-200);
    checks.Check(a.Dot(b) == 32);
    a.w = nanf(""); b.w = HUGE_VALF;
    checks.Check(a.Dot(b) == 32);
    checks.Check(VxVector4(1,2,3,4) != VxVector4(1,2,3,5));
    checks.Check(VxVector4(1,2,3,-0.0f) == VxVector4(1,2,3,0.0f));
    checks.Check(!(a == a) && a != a);
    return checks;
}

static Vector4Checks Vector4UnalignedCases() {
    Vector4Checks checks;
    for (int offset = 0; offset < 16; offset += 4) {
        alignas(16) unsigned char storage[96], before[96];
        memset(storage, 0x6d, sizeof(storage));
        VxVector4 *a = ::new(storage + offset) VxVector4(6,-8,12,8);
        VxVector4 *b = ::new(storage + 32 + offset) VxVector4(2,4,-3,4);
        VxVector4 *result = ::new(storage + 64 + offset) VxVector4;
        for (int operation = 0; operation < 17; ++operation) {
            a->Set(6,-8,12,8);
            memcpy(before, storage, sizeof(storage));
            bool reference;
            *result = Vector4Arithmetic(operation, *a, *b, reference);
            const float *expected = Vector4ArithmeticResults[operation];
            checks.Vector(*result, expected[0], expected[1], expected[2], expected[3]);
            checks.Check(reference);
            checks.Check(memcmp(before, storage, offset) == 0);
            checks.Check(memcmp(before + offset + 16, storage + offset + 16, 48) == 0);
            checks.Check(memcmp(before + offset + 80, storage + offset + 80, 16 - offset) == 0);
        }
    }
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
TEST(Vector4Reference, AccessorsSettersAndBaseAssignmentPreserveTheirContracts) { ExpectVector4(Vector4Access()); }
TEST(Vector4Reference, FourComponentArithmeticAndReturnedReferences) { ExpectVector4(Vector4ArithmeticCases()); }
TEST(Vector4Reference, MixedArithmeticPreservesEveryWBit) { ExpectVector4(Vector4MixedCases()); }
TEST(Vector4Reference, DotIgnoresWWhileEqualityIncludesW) { ExpectVector4(Vector4DotAndEquality()); }
TEST(Vector4Reference, FourByteAlignmentAndFullObjectCanaries) { ExpectVector4(Vector4UnalignedCases()); }
#if defined(_WIN32)
TEST(Vector4Reference, MixedArithmeticDoesNotReadOrWriteTheWField) { ExpectVector4(Vector4InaccessibleW()); }
#endif

TEST(Vector4Reference, MatrixOperatorsUseTheOriginalPointConvention) {
    const float rows[4][4] = {{2,1,-1,7},{3,-2,4,-3},{-1,5,2,9},{7,11,-13,2}};
    VxMatrix matrix;
    memcpy(&matrix, rows, sizeof(matrix));
    const float fourth[] = {0,1,2,-5,nanf(""),HUGE_VALF};
    for (float w : fourth) {
        const VxVector4 value(2,-3,5,w);
        const VxVector4 left = matrix * value, right = value * matrix;
        EXPECT_EQ(left, VxVector4(-3,44,-17,70));
        EXPECT_EQ(right, VxVector4(-3,44,-17,70));
    }
}

#if defined(_WIN32)
TEST(Vector4Reference, ConstructionAndFullArithmeticUseExactlySixteenBytes) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    const size_t page = info.dwPageSize;
    unsigned char *memory = static_cast<unsigned char *>(VirtualAlloc(nullptr, 5 * page, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
    ASSERT_NE(memory, nullptr);
    DWORD old;
    ASSERT_TRUE(VirtualProtect(memory + page, page, PAGE_READWRITE, &old));
    ASSERT_TRUE(VirtualProtect(memory + 3 * page, page, PAGE_READWRITE, &old));
    float *input = reinterpret_cast<float *>(memory + 2 * page - 16);
    input[0] = 6; input[1] = -8; input[2] = 12; input[3] = 8;
    ASSERT_TRUE(VirtualProtect(memory + page, page, PAGE_READONLY, &old));
    VxVector4 *output = ::new(memory + 4 * page - 16) VxVector4(input);
    for (int operation = 0; operation < 17; ++operation) {
        *output = VxVector4(input);
        bool reference;
        *output = Vector4Arithmetic(operation, *output, VxVector4(2,4,-3,4), reference);
        const float *expected = Vector4ArithmeticResults[operation];
        EXPECT_EQ(*output, VxVector4(expected));
        EXPECT_TRUE(reference);
    }
    output->~VxVector4();
    output = ::new(output) VxVector4;
    EXPECT_EQ(*output, VxVector4(0,0,0,0));
    output->~VxVector4();
    VirtualFree(memory, 0, MEM_RELEASE);
}
#endif
#endif
