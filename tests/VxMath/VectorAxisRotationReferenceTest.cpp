#include <math.h>
#include <float.h>
#include <new>
#include <string.h>
#ifndef VX_VECTOR_AXIS_ROTATION_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef VxVector *(*AxisRotationFunction)(VxVector *, const VxVector *, const VxVector *, float);
static const float AxisHalfPi = 1.57079632679489661923f;

struct AxisRotationExample {
    float vector[3], axis[3], angle, result[3];
};

static const AxisRotationExample AxisRotationExamples[] = {
    {{1, 2, 3}, {1, 0, 0}, AxisHalfPi, {1, 3, -2}},
    {{1, 2, 3}, {0, 1, 0}, AxisHalfPi, {-3, 2, 1}},
    {{1, 2, 3}, {0, 0, 1}, AxisHalfPi, {2, -1, 3}},
    {{1, 2, 3}, {1, 0, 0}, -AxisHalfPi, {1, -3, 2}},
    {{1, 2, 3}, {0, 1, 0}, -AxisHalfPi, {3, 2, -1}},
    {{1, 2, 3}, {0, 0, 1}, -AxisHalfPi, {-2, 1, 3}},
    {{1, 2, 3}, {1, 1, 1}, 2.09439510239319549f, {2, 3, 1}},
    {{1, 2, 3}, {1, 2, 3}, 0, {1, 2, 3}}
};

#ifndef VX_VECTOR_AXIS_ROTATION_PROBE
static VxVector *ReferenceRotateAxis(VxVector *result, const VxVector *vector, const VxVector *axis, float angle) {
    *result = Rotate(*vector, *axis, angle);
    return result;
}

static VxVector *PointerRotateAxis(VxVector *result, const VxVector *vector, const VxVector *axis, float angle) {
    *result = Rotate(vector, axis, angle);
    return result;
}
#endif

static bool CallAxisRotation(AxisRotationFunction function, VxVector *result, const VxVector *vector, const VxVector *axis, float angle) {
#if defined(_WIN32)
    __try {
        return function(result, vector, axis, angle) == result;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    return function(result, vector, axis, angle) == result;
#endif
}

#ifndef VX_VECTOR_AXIS_ROTATION_PROBE
class VectorAxisRotationReference : public testing::TestWithParam<int> {
protected:
    AxisRotationFunction Function() {
        return GetParam() == 0 ? ReferenceRotateAxis : PointerRotateAxis;
    }

    void ExpectVector(const VxVector &result, const float expected[3], float tolerance = 2e-6f) {
        EXPECT_NEAR(result.x, expected[0], tolerance);
        EXPECT_NEAR(result.y, expected[1], tolerance);
        EXPECT_NEAR(result.z, expected[2], tolerance);
    }
};

TEST_P(VectorAxisRotationReference, SignedAnglesUseTheOriginalRotationDirection) {
    for (const AxisRotationExample &example : AxisRotationExamples) {
        VxVector vector(example.vector), axis(example.axis), result;
        ASSERT_TRUE(CallAxisRotation(Function(), &result, &vector, &axis, example.angle));
        ExpectVector(result, example.result);
    }
}

TEST_P(VectorAxisRotationReference, ShortAndLargeAxesAreNormalizedInsteadOfDiscarded) {
    const float scales[] = {0.0001f, 1e-20f, 1e20f, 3.5f};
    for (float scale : scales) {
        for (const AxisRotationExample &example : AxisRotationExamples) {
            VxVector vector(example.vector), axis(example.axis), result;
            axis.x *= scale;
            axis.y *= scale;
            axis.z *= scale;
            Function()(&result, &vector, &axis, example.angle);
            ExpectVector(result, example.result);
        }
    }
    const float extremes[] = {0x1p-149f, FLT_MAX};
    for (float scale : extremes) {
        for (int i = 0; i < 6; ++i) {
            const AxisRotationExample &example = AxisRotationExamples[i];
            VxVector vector(example.vector), axis(example.axis), result;
            axis.x *= scale; axis.y *= scale; axis.z *= scale;
            Function()(&result, &vector, &axis, example.angle);
            ExpectVector(result, example.result);
        }
    }
}

TEST_P(VectorAxisRotationReference, ReversingTheAxisReversesTheAngle) {
    for (const AxisRotationExample &example : AxisRotationExamples) {
        VxVector vector(example.vector), axis(-example.axis[0], -example.axis[1], -example.axis[2]), result;
        Function()(&result, &vector, &axis, -example.angle);
        ExpectVector(result, example.result);
    }
}

TEST_P(VectorAxisRotationReference, TinyAnglesAreNotSnappedToZero) {
    VxVector vector(1, 0, 0), axis(0, 0, 1), result;
    Function()(&result, &vector, &axis, 1e-9f);
    EXPECT_FLOAT_EQ(result.x, 1);
    EXPECT_NEAR(result.y, -1e-9f, 1e-15f);
    EXPECT_FLOAT_EQ(result.z, 0);
}

TEST_P(VectorAxisRotationReference, ZeroAxisKeepsTheInput) {
    const VxVector vector(1, -2, 3);
    const float angles[] = {0, AxisHalfPi, -AxisHalfPi};
    for (unsigned signs = 0; signs < 8; ++signs) {
        const VxVector axis(signs & 1 ? -0.0f : 0, signs & 2 ? -0.0f : 0, signs & 4 ? -0.0f : 0);
        for (float angle : angles) {
            VxVector result;
            ASSERT_TRUE(CallAxisRotation(Function(), &result, &vector, &axis, angle));
            EXPECT_EQ(vector.x, result.x);
            EXPECT_EQ(vector.y, result.y);
            EXPECT_EQ(vector.z, result.z);
        }
    }
}

TEST_P(VectorAxisRotationReference, NonfiniteAngleProducesNanComponents) {
    VxVector vector(1, 2, 3), axis(0, 0, 1), result;
    const float angles[] = {HUGE_VALF, nanf("")};
    for (float angle : angles) {
        ASSERT_TRUE(CallAxisRotation(Function(), &result, &vector, &axis, angle));
        EXPECT_TRUE(isnan(result.x));
        EXPECT_TRUE(isnan(result.y));
        EXPECT_TRUE(isnan(result.z));
    }
}

TEST_P(VectorAxisRotationReference, RotationPreservesLengthAndProjectionOnTheAxis) {
    const VxVector axis(0.6f, 0.8f, 0);
    for (int i = -4; i <= 4; ++i) {
        VxVector vector(float(i), -2, 3), result;
        Function()(&result, &vector, &axis, 0.4f * i);
        const float square = vector.x * vector.x + vector.y * vector.y + vector.z * vector.z;
        EXPECT_NEAR(result.x * result.x + result.y * result.y + result.z * result.z, square, 2e-5f);
        EXPECT_NEAR(result.x * axis.x + result.y * axis.y, vector.x * axis.x + vector.y * axis.y, 2e-6f);
    }
}

TEST_P(VectorAxisRotationReference, OppositeAngleRestoresTheVector) {
    const VxVector axis(1, -2, 3);
    for (int i = -4; i <= 4; ++i) {
        VxVector vector(float(i), -2, 3), result;
        Function()(&result, &vector, &axis, 0.4f * i);
        Function()(&result, &result, &axis, -0.4f * i);
        const float expected[] = {vector.x, vector.y, vector.z};
        ExpectVector(result, expected);
    }
}

TEST_P(VectorAxisRotationReference, ResultCanReplaceEitherInput) {
    for (const AxisRotationExample &example : AxisRotationExamples) {
        VxVector vector(example.vector), axis(example.axis);
        Function()(&vector, &vector, &axis, example.angle);
        ExpectVector(vector, example.result);
        vector = VxVector(example.vector);
        Function()(&axis, &vector, &axis, example.angle);
        ExpectVector(axis, example.result);
    }
}

TEST_P(VectorAxisRotationReference, FourByteAlignmentPreservesInputsAndOutputW) {
    for (int offset = 0; offset < 16; offset += 4) {
        alignas(16) unsigned char storage[96], before[96];
        memset(storage, 0x6d, sizeof(storage));
        VxVector4 *vector = ::new(storage + offset) VxVector4(1, 2, 3, nanf(""));
        VxVector4 *axis = ::new(storage + 32 + offset) VxVector4(0, 0, 1, HUGE_VALF);
        VxVector4 *result = ::new(storage + 64 + offset) VxVector4(0, 0, 0, 42);
        memcpy(before, storage, sizeof(storage));
        Function()(result, vector, axis, AxisHalfPi);
        const float expected[] = {2, -1, 3};
        ExpectVector(*result, expected);
        EXPECT_EQ(memcmp(before, storage, 64 + offset), 0);
        EXPECT_EQ(memcmp(before + 76 + offset, storage + 76 + offset, 20 - offset), 0);
    }
}

#if defined(_WIN32)
TEST_P(VectorAxisRotationReference, ReadonlyXyzInputsAndExactTwelveByteOutput) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    const size_t page = info.dwPageSize;
    unsigned char *memory = static_cast<unsigned char *>(VirtualAlloc(nullptr, 7 * page, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
    ASSERT_NE(memory, nullptr);
    DWORD old;
    ASSERT_TRUE(VirtualProtect(memory + page, page, PAGE_READWRITE, &old));
    ASSERT_TRUE(VirtualProtect(memory + 3 * page, page, PAGE_READWRITE, &old));
    ASSERT_TRUE(VirtualProtect(memory + 5 * page, page, PAGE_READWRITE, &old));
    VxVector *vector = ::new(memory + 2 * page - 12) VxVector(1, 2, 3);
    VxVector *axis = ::new(memory + 4 * page - 12) VxVector(0, 0, 1);
    VxVector *result = ::new(memory + 6 * page - 12) VxVector;
    ASSERT_TRUE(VirtualProtect(memory + page, page, PAGE_READONLY, &old));
    ASSERT_TRUE(VirtualProtect(memory + 3 * page, page, PAGE_READONLY, &old));
    const bool called = CallAxisRotation(Function(), result, vector, axis, AxisHalfPi);
    EXPECT_TRUE(called);
    if (called) {
        const float expected[] = {2, -1, 3};
        ExpectVector(*result, expected);
    }
    VirtualFree(memory, 0, MEM_RELEASE);
}
#endif

INSTANTIATE_TEST_SUITE_P(ReferenceAndPointer, VectorAxisRotationReference, testing::Values(0, 1));
#endif
