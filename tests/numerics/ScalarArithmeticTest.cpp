#include <gtest/gtest.h>

#include "VxFPUControlWord.h"
#include "VxScalar.h"
#include "VxVector.h"

#include <string.h>

class ScalarArithmeticTest : public ::testing::Test {
  protected:
    XWORD SavedWord = 0;

    void SetUp() override {
        SavedWord = VxGetFPUControlWord();
        VxSetFPUControlWord(static_cast<XWORD>((SavedWord & ~0x0f00) | 0x0300 | 0x003f));
    }

    void TearDown() override { VxSetFPUControlWord(SavedWord); }
};

TEST_F(ScalarArithmeticTest, EnvironmentInitializationIsIdempotentAndPreservesUnrelatedControlBits) {
    VxSetFPUControlWord(static_cast<XWORD>(VxGetFPUControlWord() | 0x0f00));
    const XWORD selected = VxGetFPUControlWord();
    const XWORD expected = static_cast<XWORD>((selected & 0xf0ff) | 0x003f);
    VxInitializeMathEnvironment();
    EXPECT_EQ(expected, VxGetFPUControlWord());
    VxInitializeMathEnvironment();
    EXPECT_EQ(expected, VxGetFPUControlWord());
}

TEST_F(ScalarArithmeticTest, ScalarOperationsComposeWithoutKnowingTheirRepresentation) {
    const XWORD precisions[] = {0, 0x0200, 0x0300};
    for (XWORD precision : precisions) {
        VxSetFPUControlWord(static_cast<XWORD>((VxGetFPUControlWord() & ~0x0300) | precision));
        VxScalarContext arithmetic;
        VxScalarInitContext(arithmetic);
        const VxScalar one = VxScalarLoad(arithmetic, 1);
        const VxScalar two = VxScalarAdd(arithmetic, one, one);
        const VxScalar nine = VxScalarLoad(arithmetic, 9);
        const VxScalar root = VxScalarSquareRoot(arithmetic, nine);
        EXPECT_EQ(3, VxScalarStore(arithmetic, root));
        EXPECT_EQ(8, VxScalarStore(arithmetic, VxScalarSubtract(arithmetic, nine, one)));
        EXPECT_EQ(4.5f, VxScalarStore(arithmetic, VxScalarDivide(arithmetic, nine, two)));
        EXPECT_EQ(6, VxScalarStore(arithmetic, VxScalarMultiply(arithmetic, root, two)));
        const VxScalar negative = VxScalarNegate(arithmetic, root);
        EXPECT_EQ(-3, VxScalarStore(arithmetic, negative));
        EXPECT_EQ(3, VxScalarStore(arithmetic, VxScalarAbsolute(arithmetic, negative)));
        EXPECT_EQ(VXSCALAR_LESS, VxScalarCompare(arithmetic, one, two));
        EXPECT_EQ(VXSCALAR_GREATER, VxScalarCompare(arithmetic, two, one));
        EXPECT_EQ(VXSCALAR_EQUAL, VxScalarCompare(arithmetic, one, one));
    }
}

TEST_F(ScalarArithmeticTest, ScalarValuesPreserveSignedZeroAndUnorderedComparisons) {
    VxScalarContext arithmetic;
    VxScalarInitContext(arithmetic);
    const VxScalar zero = VxScalarLoad(arithmetic, 0);
    const float negative = VxScalarStore(arithmetic, VxScalarNegate(arithmetic, zero));
    uint32_t bits;
    memcpy(&bits, &negative, sizeof(bits));
    EXPECT_EQ(0x80000000u, bits);
    const VxScalar unordered = VxScalarDivide(arithmetic, zero, zero);
    EXPECT_EQ(VXSCALAR_UNORDERED, VxScalarCompare(arithmetic, unordered, zero));
}

TEST_F(ScalarArithmeticTest, ScalarLoadsPreserveIntegerAndDoubleValuesUntilStorage) {
    const XWORD precisions[] = {0, 0x0200, 0x0300};
    for (XWORD precision : precisions) {
        VxSetFPUControlWord(static_cast<XWORD>((VxGetFPUControlWord() & ~0x0300) | precision));
        VxScalarContext arithmetic;
        VxScalarInitContext(arithmetic);
        const VxScalar integer = VxScalarLoad(arithmetic, int32_t(16777217));
        EXPECT_EQ(16777217.0, VxScalarStoreDouble(arithmetic, integer));
        EXPECT_EQ(16777216.0f, VxScalarStore(arithmetic, integer));
        const double precise = 1.0000000000000002;
        const VxScalar value = VxScalarLoad(arithmetic, precise);
        EXPECT_EQ(precise, VxScalarStoreDouble(arithmetic, value));
        EXPECT_EQ(1.0f, VxScalarStore(arithmetic, value));
    }
}

#if VXMATH_ENABLE_X87_EMULATION
TEST_F(ScalarArithmeticTest, ScalarSequenceKeepsItsInitialPrecisionWhenTheThreadEnvironmentChanges) {
    VxSetFPUControlWord(0x007f);
    VxScalarContext arithmetic;
    VxScalarInitContext(arithmetic);
    VxSetFPUControlWord(0x037f);
    const VxScalar base = VxScalarLoad(arithmetic, 16777216);
    const VxScalar sum = VxScalarAdd(arithmetic, base, VxScalarLoad(arithmetic, 1));
    EXPECT_EQ(0, VxScalarStore(arithmetic, VxScalarSubtract(arithmetic, sum, base)));

    VxScalarContext next;
    VxScalarInitContext(next);
    const VxScalar wide = VxScalarAdd(next, base, VxScalarLoad(next, 1));
    EXPECT_EQ(1, VxScalarStore(next, VxScalarSubtract(next, wide, base)));
}
#endif

TEST_F(ScalarArithmeticTest, ScalarStoreRoundsIntermediateResultsToFloat) {
    const XWORD precisions[] = {0x0200, 0x0300};
    for (XWORD precision : precisions) {
        VxSetFPUControlWord(static_cast<XWORD>((VxGetFPUControlWord() & ~0x0300) | precision));
        VxScalarContext arithmetic;
        VxScalarInitContext(arithmetic);
        const VxScalar coefficient =
            VxScalarMultiply(arithmetic, VxScalarLoad(arithmetic, 4.16666651f), VxScalarLoad(arithmetic, 0.001f));
        const float result =
            VxScalarStore(arithmetic, VxScalarMultiply(arithmetic, coefficient, VxScalarLoad(arithmetic, 7)));
        const float storedCoefficient = VxScalarStore(arithmetic, coefficient);
        const float rounded =
            VxScalarStore(arithmetic, VxScalarMultiply(arithmetic, VxScalarLoad(arithmetic, storedCoefficient),
                                                       VxScalarLoad(arithmetic, 7)));
#if VXMATH_ENABLE_X87_EMULATION
        EXPECT_EQ(0.02916666679084301f, result);
        EXPECT_NE(result, rounded);
#else
        volatile float stored = 4.16666651f * 0.001f;
        (void)result;
        EXPECT_EQ(stored * 7, rounded);
#endif
    }
}

TEST_F(ScalarArithmeticTest, NormalizationAppliesTheRequestedSignedLength) {
    const VxVector direction(6, 8, 0);
    const VxVector result = Normalize(direction, -5);
    EXPECT_EQ(-3, result.x);
    EXPECT_EQ(-4, result.y);
    EXPECT_EQ(0, result.z);
    EXPECT_EQ(6, direction.x);
    EXPECT_EQ(8, direction.y);
    const VxVector rounded = Normalize(VxVector(-12, 9, 0), 30);
#if VXMATH_ENABLE_X87_EMULATION
    EXPECT_EQ(-24.000001907348633f, rounded.x);
#else
    EXPECT_EQ(-24, rounded.x);
#endif
    EXPECT_EQ(18, rounded.y);
}

TEST_F(ScalarArithmeticTest, NormalizationPreservesZeroOverflowAndUnderflowResults) {
    const VxVector zero = Normalize(VxVector(0, 0, 0), 4);
    const VxVector huge = Normalize(VxVector(0, 1e20f, 0), 2);
    const VxVector tiny = Normalize(VxVector(-1e-30f, 0, 0), 3);
#if VXMATH_ENABLE_X87_EMULATION
    EXPECT_NE(zero.x, zero.x);
    EXPECT_NE(zero.y, zero.y);
    EXPECT_NE(zero.z, zero.z);
    EXPECT_EQ(0, huge.y);
    EXPECT_LT(tiny.x, -1e38f);
    EXPECT_NE(tiny.y, tiny.y);
    EXPECT_NE(tiny.z, tiny.z);
#else
    EXPECT_EQ(0, zero.x);
    EXPECT_EQ(0, zero.y);
    EXPECT_EQ(0, zero.z);
    EXPECT_EQ(2, huge.y);
    EXPECT_EQ(-3, tiny.x);
    EXPECT_EQ(0, tiny.y);
    EXPECT_EQ(0, tiny.z);
#endif
}
