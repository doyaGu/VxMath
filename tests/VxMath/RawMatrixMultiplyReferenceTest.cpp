#ifndef VX_RAW_MATRIX_MULTIPLY_PROBE
#include "VxMatrix.h"
#include <gtest/gtest.h>
#include <type_traits>
#endif
#define VX_MATRIX_FULL_PROBE 1
#include "MatrixFullMultiplyReferenceTest.cpp"

static M4Result R4IndependentAlignments(M4Fn fn) {
    M4Result result;
    VxMatrix a, b, expected;
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) {
        a[r][c] = float(2 * r - c + 1);
        b[r][c] = float(r + 3 * c - 2);
    }
    M4Expected(expected, a, b);
    for (int sa = 0; sa < 16; sa += 4) for (int sb = 0; sb < 16; sb += 4) for (int sr = 0; sr < 16; sr += 4) {
        alignas(16) unsigned char bytes[320], before[320];
        std::memset(bytes, 0xa5, sizeof(bytes));
        VxMatrix *pa = new(bytes + 16 + sa) VxMatrix(a);
        VxMatrix *pb = new(bytes + 112 + sb) VxMatrix(b);
        VxMatrix *pr = new(bytes + 208 + sr) VxMatrix;
        std::memcpy(before, bytes, sizeof(bytes));
        fn(*pr, *pa, *pb);
        bool ok = M4Equal(*pr, expected);
        for (int i = 0; i < 320; ++i)
            if (i < 208 + sr || i >= 272 + sr) ok = ok && bytes[i] == before[i];
        ++result.samples;
        result.errors += !ok;
        pa->~VxMatrix(); pb->~VxMatrix(); pr->~VxMatrix();
    }
    return result;
}

#if defined(VX_SIMD_SSE)
static void R4Raw(VxMatrix &out, const VxMatrix &a, const VxMatrix &b) {
    VxSIMDMatrixMultiply(out[0], a[0], b[0]);
}
#ifndef VX_RAW_MATRIX_MULTIPLY_PROBE
static void R4Expect(M4Result result, int samples) {
    EXPECT_EQ(result.samples, samples);
    EXPECT_EQ(result.errors, 0);
}
TEST(RawMatrixMultiplyReference, SignatureRetainsNoexceptRawFloatArrays) {
    static_assert(std::is_same<decltype(&VxSIMDMatrixMultiply), void (*)(float *, const float *, const float *) noexcept>::value, "raw multiply signature");
}
TEST(RawMatrixMultiplyReference, NonCommutingFullHomogeneousCompositionAndAliases) { R4Expect(M4Literal(R4Raw), 3); }
TEST(RawMatrixMultiplyReference, IndependentIntegerBasisAndAllWholeObjectAliases) { R4Expect(M4Integers(R4Raw), 1280); }
TEST(RawMatrixMultiplyReference, EveryInputFieldIncludingAllProjectiveWeights) { R4Expect(M4BasisWeights(R4Raw), 96); }
TEST(RawMatrixMultiplyReference, ProjectionPreservesPointsAndDirectionsWithoutPerspectiveDivide) { R4Expect(M4Projective(R4Raw), 12); }
TEST(RawMatrixMultiplyReference, CancellationRetainsSmallTermsAcrossEveryLane) { R4Expect(M4Numerics(R4Raw, 0), 192); }
TEST(RawMatrixMultiplyReference, FiniteResultSurvivesIntermediateProductOverflow) { R4Expect(M4Numerics(R4Raw, 1), 192); }
TEST(RawMatrixMultiplyReference, ProductTailsRecoverSubnormalResult) { R4Expect(M4Numerics(R4Raw, 2), 192); }
TEST(RawMatrixMultiplyReference, NonfiniteArithmeticRetainsInfinitySignsAndNaN) { R4Expect(M4Nonfinite(R4Raw), 36); }
TEST(RawMatrixMultiplyReference, IndependentInputAndOutputFourByteAlignmentsPreserveCanaries) { R4Expect(R4IndependentAlignments(R4Raw), 64); }
TEST(RawMatrixMultiplyReference, Exact64ByteReadOnlyInputsAndGuardedOutput) {
#if defined(_WIN32)
    R4Expect(M4Footprint(R4Raw), 12);
#else
    R4Expect(M4Footprint(R4Raw), 4);
#endif
}
#endif
#endif
