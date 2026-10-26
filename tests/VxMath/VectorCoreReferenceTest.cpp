#include <math.h>
#include <stddef.h>
#include <new>
#include <string.h>
#ifndef VX_VECTOR_CORE_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

struct VectorCoreChecks {
    int calls = 0, errors = 0;
    void Check(bool valid) { ++calls; errors += !valid; }
    void Vector(const VxVector &value, float x, float y, float z) {
        Check(value.x == x && value.y == y && value.z == z);
    }
};

static char AcceptVector(VxVector);
static long AcceptVector(...);

static VectorCoreChecks CoreLifecycle() {
    VectorCoreChecks checks;
    checks.Check(sizeof(VxVector) == 12 && alignof(VxVector) == 4);
    checks.Check(offsetof(VxVector, x) == 0 && offsetof(VxVector, y) == 4 && offsetof(VxVector, z) == 8);
    checks.Check(offsetof(VxVector, v) == 0 && sizeof(((VxVector *)0)->v) == 12);
    checks.Check(__is_standard_layout(VxVector) && __is_trivially_copyable(VxVector));
    checks.Check(sizeof(AcceptVector(1.0f)) == sizeof(long));
    const float input[] = {2, -3, 5};
    VxVector zero, uniform(-2), components(2, -3, 5), array(input);
    checks.Vector(zero, 0, 0, 0);
    checks.Vector(uniform, -2, -2, -2);
    checks.Vector(components, 2, -3, 5);
    checks.Vector(array, 2, -3, 5);
    array.Set(6, -8, 12);
    checks.Vector(array, 6, -8, 12);
    const VxVector &constant = array;
    checks.Check(&array[0] == &array.x && &array[1] == &array.y && &array[2] == &array.z);
    checks.Check(&constant[0] == &constant.x && &constant[1] == &constant.y && &constant[2] == &constant.z);
    array[1] = 4;
    checks.Check(array.y == 4 && array.v[1] == 4);
    array.v[2] = -7;
    checks.Check(array.z == -7 && constant[2] == -7);
    VxVector copy(array), moved(static_cast<VxVector &&>(copy));
    checks.Vector(moved, 6, 4, -7);
    copy = components;
    moved = static_cast<VxVector &&>(copy);
    checks.Vector(moved, 2, -3, 5);
    VxVector sequence[3];
    checks.Check(sizeof(sequence) == 36 && reinterpret_cast<char *>(&sequence[1]) - reinterpret_cast<char *>(&sequence[0]) == 12);
    return checks;
}

static const float CoreArithmeticResults[][3] = {
    {8, -4, 9}, {4, -12, 15}, {12, -32, -36}, {3, -2, -4},
    {3, -4, 6}, {3, -4, 6}, {3, -4, 6}, {6, -8, 12}, {-6, 8, -12},
    {8, -6, 14}, {4, -10, 10}, {8, -4, 9}, {4, -12, 15},
    {12, -32, -36}, {3, -2, -4}, {3, -4, 6}, {3, -4, 6}
};

static VxVector CoreArithmetic(int operation, VxVector &a, const VxVector &b, bool &reference) {
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

static VectorCoreChecks CoreArithmeticCases() {
    VectorCoreChecks checks;
    for (int operation = 0; operation < 17; ++operation) {
        VxVector a(6, -8, 12), b(2, 4, -3);
        bool reference;
        const VxVector result = CoreArithmetic(operation, a, b, reference);
        const float *expected = CoreArithmeticResults[operation];
        checks.Vector(result, expected[0], expected[1], expected[2]);
        checks.Check(reference);
        checks.Vector(b, 2, 4, -3);
        if (operation < 11) checks.Vector(a, 6, -8, 12);
    }
    VxVector value(2, -4, 8);
    value += value; checks.Vector(value, 4, -8, 16);
    value -= value; checks.Vector(value, 0, 0, 0);
    value.Set(2, -4, 8); value *= value; checks.Vector(value, 4, 16, 64);
    value.Set(2, -4, 8); value /= value; checks.Vector(value, 1, 1, 1);
    return checks;
}

static VectorCoreChecks CoreComparisons() {
    VectorCoreChecks checks;
    const VxVector a(1, 2, 3), greater(2, 3, 4), mixed(0, 5, 6), tied(1, 3, 4);
    checks.Check(a < greater && a <= greater && !(greater < a));
    checks.Check(!(a < mixed) && !(a <= mixed) && !(mixed < a));
    checks.Check(!(a < tied) && a <= tied);
    checks.Check(!(a < a) && a <= a && a == a && !(a != a));
    checks.Check(VxVector(-0.0f, 1, 2) == VxVector(0.0f, 1, 2));
    const VxVector unordered(nanf(""), 1, 2);
    checks.Check(!(unordered == unordered) && unordered != unordered);
    checks.Check(!(unordered < a) && !(unordered <= a));
    return checks;
}

#ifndef VX_VECTOR_CORE_PROBE
static void ExpectCore(VectorCoreChecks checks) {
    EXPECT_GT(checks.calls, 0);
    EXPECT_EQ(checks.errors, 0);
}

TEST(VectorCoreReference, LayoutConstructionIndexingAndImplicitSpecialMembers) { ExpectCore(CoreLifecycle()); }
TEST(VectorCoreReference, ArithmeticUsesComponentsAndReturnsTheModifiedObject) { ExpectCore(CoreArithmeticCases()); }
TEST(VectorCoreReference, DominanceAndNumericEqualityFollowTheOriginalContract) { ExpectCore(CoreComparisons()); }

TEST(VectorCoreReference, StaticAxesHaveStableReferencesAndExpectedValues) {
    const VxVector *axes[] = {&VxVector::axisX(), &VxVector::axisY(), &VxVector::axisZ(), &VxVector::axis0(), &VxVector::axis1()};
    const VxVector *again[] = {&VxVector::axisX(), &VxVector::axisY(), &VxVector::axisZ(), &VxVector::axis0(), &VxVector::axis1()};
    const float expected[][3] = {{1,0,0},{0,1,0},{0,0,1},{0,0,0},{1,1,1}};
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(axes[i], again[i]);
        EXPECT_EQ(memcmp(axes[i], expected[i], 12), 0);
    }
}

TEST(VectorCoreReference, MembersModifyOnlyXyzOfADerivedObject) {
    VxVector4 derived(6, -8, 12, -42);
    VxVector &base = derived;
    const VxVector operand(2, 4, -3);
    base += operand; base -= operand; base *= operand; base /= operand;
    base *= 0.5f; base /= 0.5f; base.Set(1, 2, 3);
    EXPECT_FLOAT_EQ(derived.x, 1);
    EXPECT_FLOAT_EQ(derived.y, 2);
    EXPECT_FLOAT_EQ(derived.z, 3);
    EXPECT_FLOAT_EQ(derived.w, -42);
}

TEST(VectorCoreReference, MatrixRotationIgnoresTranslationAndSupportsMatrixRowAliases) {
    const float values[4][4] = {{2,1,-1,7},{3,-2,4,-3},{-1,5,2,9},{7,11,-13,2}};
    VxMatrix matrix;
    memcpy(&matrix, values, sizeof(matrix));
    VxVector value(2, -3, 5);
    value.Rotate(matrix);
    EXPECT_EQ(value, VxVector(-10, 33, -4));
    VxVector &row = matrix[0];
    row.Rotate(matrix);
    EXPECT_EQ(row, VxVector(8, -5, 0));
}

TEST(VectorCoreReference, CompressedAssignmentUsesTheOriginalAngularConvention) {
    const short angles[][2] = {{0,0},{0,4096},{4096,0},{-4096,0},{8192,4096}};
    const float expected[][3] = {{0,0,1},{1,0,0},{0,-1,0},{0,1,0},{-1,0,0}};
    for (int i = 0; i < 5; ++i) {
        VxCompressedVector packed;
        packed.xa = angles[i][0]; packed.ya = angles[i][1];
        VxVector value;
        EXPECT_EQ(&(value = packed), &value);
        for (int k = 0; k < 3; ++k) EXPECT_NEAR(value[k], expected[i][k], 2e-6f);
    }
}

#if defined(_WIN32)
TEST(VectorCoreReference, ConstructorsArithmeticAndDestructionRespectTwelveByteObjects) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    const size_t page = info.dwPageSize;
    unsigned char *memory = static_cast<unsigned char *>(VirtualAlloc(nullptr, 5 * page, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
    ASSERT_NE(memory, nullptr);
    DWORD old;
    ASSERT_TRUE(VirtualProtect(memory + page, page, PAGE_READWRITE, &old));
    ASSERT_TRUE(VirtualProtect(memory + 3 * page, page, PAGE_READWRITE, &old));
    float *input = reinterpret_cast<float *>(memory + 2 * page - 12);
    input[0] = 6; input[1] = -8; input[2] = 12;
    ASSERT_TRUE(VirtualProtect(memory + page, page, PAGE_READONLY, &old));
    VxVector *output = ::new(memory + 4 * page - 12) VxVector(input);
    EXPECT_EQ(*output, VxVector(6, -8, 12));
    for (int operation = 0; operation < 17; ++operation) {
        *output = VxVector(input);
        bool reference;
        *output = CoreArithmetic(operation, *output, VxVector(2,4,-3), reference);
        const float *expected = CoreArithmeticResults[operation];
        EXPECT_EQ(*output, VxVector(expected));
        EXPECT_TRUE(reference);
    }
    output->~VxVector();
    output = ::new(output) VxVector;
    EXPECT_EQ(*output, VxVector(0,0,0));
    output->~VxVector();
    output = ::new(output) VxVector(2);
    EXPECT_EQ(*output, VxVector(2,2,2));
    output->~VxVector();
    VirtualFree(memory, 0, MEM_RELEASE);
}
#endif
#endif
