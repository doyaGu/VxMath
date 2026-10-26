#include <math.h>
#include <new>
#include <string.h>
#ifndef VX_VECTOR_REFLECT_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef void (*ReflectFunction)(VxVector *, const VxVector *, const VxVector *);

static void FreeReflect(VxVector *result, const VxVector *incident, const VxVector *normal) {
    *result = Reflect(*incident, *normal);
}

#if defined(VX_SIMD_SSE)
static void PointerReflect(VxVector *result, const VxVector *incident, const VxVector *normal) {
    VxSIMDReflectVector(result, incident, normal);
}

static void RegisterReflect(VxVector *result, const VxVector *incident, const VxVector *normal) {
    VxSIMDStoreFloat3(&result->x, VxSIMDReflect3(VxSIMDLoadFloat3(&incident->x), VxSIMDLoadFloat3(&normal->x)));
}
#endif

struct ReflectExample {
    float incident[3], normal[3], result[3];
};

// Virtools takes an incident vector pointing away from the surface.
static const ReflectExample ReflectExamples[] = {
    {{1, 2, 3}, {0, 1, 0}, {-1, 2, -3}},
    {{1, 2, 3}, {1, 0, 0}, {1, -2, -3}},
    {{1, 2, 3}, {0, 0, 1}, {-1, -2, 3}},
    {{1, 2, 3}, {0, -1, 0}, {-1, 2, -3}},
    {{0, 1, 0}, {0, 1, 0}, {0, 1, 0}},
    {{2, 0, 3}, {0, 1, 0}, {-2, 0, -3}},
    {{1, 2, 3}, {0.5f, 0.5f, 0.5f}, {2, 1, 0}},
    {{1, 2, 3}, {1, -2, 0.5f}, {-4, 4, -4.5f}},
    {{1, 1, 0}, {0, 2, 0}, {-1, 7, 0}},
    {{1, -2, 3}, {0, 0, 0}, {-1, 2, -3}},
    {{0, 0, 0}, {1, 2, 3}, {0, 0, 0}}
};

static bool CallReflect(ReflectFunction function, VxVector *result, const VxVector *incident, const VxVector *normal) {
#if defined(_WIN32)
    __try {
        function(result, incident, normal);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    function(result, incident, normal);
    return true;
#endif
}

#ifndef VX_VECTOR_REFLECT_PROBE
class VectorReflectReference : public testing::TestWithParam<int> {
protected:
    ReflectFunction Function() {
        switch (GetParam()) {
        case 0: return FreeReflect;
#if defined(VX_SIMD_SSE)
        case 1: return PointerReflect;
        case 2: return RegisterReflect;
#endif
        default: return nullptr;
        }
    }

    void ExpectVector(const VxVector &result, const float expected[3]) {
        EXPECT_FLOAT_EQ(result.x, expected[0]);
        EXPECT_FLOAT_EQ(result.y, expected[1]);
        EXPECT_FLOAT_EQ(result.z, expected[2]);
    }
};

TEST_P(VectorReflectReference, AwayFromSurfaceConventionAndUnnormalizedNormals) {
    for (const ReflectExample &example : ReflectExamples) {
        VxVector incident(example.incident), normal(example.normal), result;
        Function()(&result, &incident, &normal);
        ExpectVector(result, example.result);
    }
}

TEST_P(VectorReflectReference, NegatingTheNormalPreservesTheReflection) {
    for (const ReflectExample &example : ReflectExamples) {
        VxVector incident(example.incident);
        VxVector normal(-example.normal[0], -example.normal[1], -example.normal[2]), result;
        Function()(&result, &incident, &normal);
        ExpectVector(result, example.result);
    }
}

TEST_P(VectorReflectReference, ResultCanReplaceEitherInput) {
    for (const ReflectExample &example : ReflectExamples) {
        VxVector incident(example.incident), normal(example.normal);
        Function()(&incident, &incident, &normal);
        ExpectVector(incident, example.result);
        incident = VxVector(example.incident);
        Function()(&normal, &incident, &normal);
        ExpectVector(normal, example.result);
    }
}

TEST_P(VectorReflectReference, UnitNormalsPreserveNormalProjectionAndReverseTangent) {
    const VxVector normal(0.6f, 0.8f, 0), tangent(-0.8f, 0.6f, 0);
    for (int n = -4; n <= 4; ++n) {
        for (int t = -4; t <= 4; ++t) {
            VxVector incident(n * normal.x + t * tangent.x, n * normal.y + t * tangent.y, 2), result;
            Function()(&result, &incident, &normal);
            EXPECT_NEAR(result.x, n * normal.x - t * tangent.x, 2e-6f);
            EXPECT_NEAR(result.y, n * normal.y - t * tangent.y, 2e-6f);
            EXPECT_FLOAT_EQ(result.z, -2);
            EXPECT_NEAR(result.x * result.x + result.y * result.y + result.z * result.z,
                        incident.x * incident.x + incident.y * incident.y + incident.z * incident.z, 2e-5f);
        }
    }
}

TEST_P(VectorReflectReference, ReflectingTwiceRestoresTheIncidentVector) {
    const VxVector normal(0.6f, 0.8f, 0);
    for (int i = -8; i <= 8; ++i) {
        VxVector incident(float(i), -3, 2), result;
        Function()(&result, &incident, &normal);
        Function()(&result, &result, &normal);
        EXPECT_NEAR(result.x, incident.x, 4e-6f);
        EXPECT_NEAR(result.y, incident.y, 4e-6f);
        EXPECT_FLOAT_EQ(result.z, incident.z);
    }
}

TEST_P(VectorReflectReference, FourByteAlignmentPreservesInputsAndOutputW) {
    for (int offset = 0; offset < 16; offset += 4) {
        alignas(16) unsigned char storage[96], before[96];
        memset(storage, 0x6d, sizeof(storage));
        VxVector4 *incident = ::new(storage + offset) VxVector4(1, 2, 3, nanf(""));
        VxVector4 *normal = ::new(storage + 32 + offset) VxVector4(0, 1, 0, HUGE_VALF);
        VxVector4 *result = ::new(storage + 64 + offset) VxVector4(0, 0, 0, 42);
        memcpy(before, storage, sizeof(storage));
        Function()(result, incident, normal);
        const float expected[] = {-1, 2, -3};
        ExpectVector(*result, expected);
        EXPECT_EQ(memcmp(before, storage, 64 + offset), 0);
        EXPECT_EQ(memcmp(before + 76 + offset, storage + 76 + offset, 20 - offset), 0);
        EXPECT_FLOAT_EQ(result->w, 42);
    }
}

#if defined(_WIN32)
TEST_P(VectorReflectReference, ReadonlyXyzInputsAndExactTwelveByteOutput) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    const size_t page = info.dwPageSize;
    unsigned char *memory = static_cast<unsigned char *>(VirtualAlloc(nullptr, 7 * page, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
    ASSERT_NE(memory, nullptr);
    DWORD old;
    ASSERT_TRUE(VirtualProtect(memory + page, page, PAGE_READWRITE, &old));
    ASSERT_TRUE(VirtualProtect(memory + 3 * page, page, PAGE_READWRITE, &old));
    ASSERT_TRUE(VirtualProtect(memory + 5 * page, page, PAGE_READWRITE, &old));
    VxVector *incident = ::new(memory + 2 * page - 12) VxVector(1, 2, 3);
    VxVector *normal = ::new(memory + 4 * page - 12) VxVector(0, 1, 0);
    VxVector *result = ::new(memory + 6 * page - 12) VxVector;
    ASSERT_TRUE(VirtualProtect(memory + page, page, PAGE_READONLY, &old));
    ASSERT_TRUE(VirtualProtect(memory + 3 * page, page, PAGE_READONLY, &old));
    const bool called = CallReflect(Function(), result, incident, normal);
    EXPECT_TRUE(called);
    if (called) {
        const float expected[] = {-1, 2, -3};
        ExpectVector(*result, expected);
    }
    VirtualFree(memory, 0, MEM_RELEASE);
}
#endif

#if defined(VX_SIMD_SSE)
INSTANTIATE_TEST_SUITE_P(FreePointerRegister, VectorReflectReference, testing::Values(0, 1, 2));

TEST(VectorReflectRegister, DotUsesOnlyXyzAndAllResultLanesFollowTheFormula) {
    float result[4];
    _mm_storeu_ps(result, VxSIMDReflect3(_mm_set_ps(7, 3, 2, 1), _mm_set_ps(11, 0, 1, 0)));
    EXPECT_FLOAT_EQ(result[0], -1);
    EXPECT_FLOAT_EQ(result[1], 2);
    EXPECT_FLOAT_EQ(result[2], -3);
    EXPECT_FLOAT_EQ(result[3], 37);
    _mm_storeu_ps(result, VxSIMDReflect3(_mm_set_ps(nanf(""), 3, 2, 1), _mm_set_ps(HUGE_VALF, 0, 1, 0)));
    EXPECT_FLOAT_EQ(result[0], -1);
    EXPECT_FLOAT_EQ(result[1], 2);
    EXPECT_FLOAT_EQ(result[2], -3);
}
#else
INSTANTIATE_TEST_SUITE_P(Free, VectorReflectReference, testing::Values(0));
#endif
#endif
