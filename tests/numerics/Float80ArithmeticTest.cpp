#include <gtest/gtest.h>
#include "VxFloat80.h"
#include "VxMatrix.h"
#include "VxFPUControlWord.h"
#include "../../src/VxOBBOverlap.h"
#define VX_OBB_OBB_NATIVE_PROBE
#include "../VxMath/ObbIntersectionReferenceTest.cpp"
#undef VX_OBB_OBB_NATIVE_PROBE
#include <float.h>
#include <string.h>

struct RestoreControl {
    XWORD word = VxGetFPUControlWord();
    ~RestoreControl() {
        VxSetFPUControlWord(word);
    }
};

TEST(Float80Arithmetic, PrecisionAndBinary32StoresRemainSeparate) {
    const VxFloatPrecision precisions[] = {VXFLOAT_PRECISION_24, VXFLOAT_PRECISION_53, VXFLOAT_PRECISION_64};
    for (VxFloatPrecision precision : precisions) {
        VxFloatContext arithmetic;
        VxFloatInitContext(arithmetic, precision);
        const VxFloat80 wide =
            VxFloat80Add(arithmetic, VxFloat80Load(arithmetic, 16777216.0f), VxFloat80Load(arithmetic, 1.0f));
        EXPECT_EQ(16777216.0f, VxFloat80StoreFloat(arithmetic, wide));
        EXPECT_EQ(precision == VXFLOAT_PRECISION_24 ? 16777216.0 : 16777217.0, VxFloat80StoreDouble(arithmetic, wide));
    }
}

TEST(Float80Arithmetic, RoundingAndExceptionStatusAreExplicit) {
    VxFloatContext down;
    VxFloatInitContext(down, VXFLOAT_PRECISION_24, VXFLOAT_ROUND_DOWN);
    VxFloatContext up;
    VxFloatInitContext(up, VXFLOAT_PRECISION_24, VXFLOAT_ROUND_UP);
    EXPECT_EQ(16777216.0f, VxFloat80StoreFloat(
                               down, VxFloat80Add(down, VxFloat80Load(down, 16777216.0f), VxFloat80Load(down, 1.0f))));
    EXPECT_EQ(16777218.0f,
              VxFloat80StoreFloat(up, VxFloat80Add(up, VxFloat80Load(up, 16777216.0f), VxFloat80Load(up, 1.0f))));
    const VxFloat80 infinity = VxFloat80Divide(down, VxFloat80Load(down, 1.0f), VxFloat80Load(down, 0.0f));
    EXPECT_EQ(VXFLOAT_GREATER, VxFloat80Compare(down, infinity, VxFloat80Load(down, DBL_MAX)));
    EXPECT_NE(0u, VxFloatGetExceptionFlags(down) & VXFLOAT_EXCEPTION_DIVISION_BY_ZERO);
    VxFloatClearExceptionFlags(down);
    EXPECT_EQ(0u, VxFloatGetExceptionFlags(down));
}

TEST(Float80Arithmetic, ExtendedEncodingPreservesTheExponentRange) {
    VxFloatContext arithmetic;
    VxFloatInitContext(arithmetic);
    const VxFloat80 huge =
        VxFloat80Multiply(arithmetic, VxFloat80Load(arithmetic, DBL_MAX), VxFloat80Load(arithmetic, 2.0));
    unsigned char bytes[10];
    VxFloat80ToBytes(huge, bytes);
    const VxFloat80 decoded = VxFloat80FromBytes(bytes);
    EXPECT_EQ(huge.Significand, decoded.Significand);
    EXPECT_EQ(huge.SignExponent, decoded.SignExponent);
    EXPECT_EQ(DBL_MAX,
              VxFloat80StoreDouble(arithmetic, VxFloat80Divide(arithmetic, decoded, VxFloat80Load(arithmetic, 2.0))));
}

TEST(Float80Arithmetic, OriginalVectorStoresDetermineScaleAndHingeAxes) {
    RestoreControl restore;
    const XWORD precisions[] = {0x0200, 0x0300};
    for (XWORD precision : precisions) {
        VxSetFPUControlWord(static_cast<XWORD>(0x007f | precision));
        const VxVector rotated(.9848077297f, .1736481637f, 0);
        EXPECT_EQ(.9999999403953552f, Magnitude(rotated));
        const float hinges[3][4] = {{-.963630438f, .267238319f, -.963630438f, .267238319f},
                                    {.968147635f, .250380158f, .968147576f, .250380129f},
                                    {-.965925813f, .258819014f, -.965925813f, .258819014f}};
        for (int hinge = 0; hinge < 3; ++hinge) {
            const VxVector normalized = Normalize(VxVector(hinges[hinge][0], 0, hinges[hinge][1]));
            EXPECT_EQ(hinges[hinge][2], normalized.x);
            EXPECT_EQ(0, normalized.y);
            EXPECT_EQ(hinges[hinge][3], normalized.z);
        }
        const VxVector axis(-12, 9, 0);
        EXPECT_EQ(-24.000001907348633f, (Normalize(axis) * 30).x);
    }
}

TEST(Float80Arithmetic, OriginalParentInverseWorksAtEveryPrecision) {
    RestoreControl restore;
    const XWORD precisions[] = {0x0000, 0x0200, 0x0300};
    const float expected[3][3][3] = {
        {{1, 0, 3.55271368e-15f}, {0, 1, -4.37123889e-14f}, {-3.55271368e-15f, 4.37113894e-14f, 1}},
        {{1, 1.13686838e-13f, 0}, {1.13686838e-13f, 1, -4.37123922e-14f}, {-3.55271368e-15f, 4.3711386e-14f, 1}},
        {{1, 1.13686838e-13f, 0}, {1.13686838e-13f, 1, -4.37123922e-14f}, {-3.55271368e-15f, 4.3711386e-14f, 1}}};
    for (int p = 0; p < 3; ++p) {
        VxSetFPUControlWord(static_cast<XWORD>(0x007f | precisions[p]));
        VxMatrix pose = VxMatrix::Identity(), inverse = VxMatrix::Identity(), local;
        pose[0].Set(-4.37110828e-08f, 9.99992949e-07f, 0.999992967f);
        pose[1].Set(9.99992949e-07f, 0.999992967f, 9.99992949e-07f);
        pose[2].Set(-0.999992967f, 9.99992949e-07f, -4.37110828e-08f);
        Vx3DInverseMatrix(inverse, pose);
        Vx3DMultiplyMatrix(local, inverse, pose);
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                EXPECT_EQ(expected[p][r][c], local[r][c]) << p << ',' << r << ',' << c;
    }
}

TEST(Float80Arithmetic, InverseCofactorNegationPreservesSignedZerosAtEveryRounding) {
    RestoreControl restore;
    const XWORD roundingModes[] = {0x0000, 0x0400, 0x0800, 0x0c00};
    for (XWORD rounding : roundingModes) {
        VxSetFPUControlWord(static_cast<XWORD>(0x037f | rounding));
        VxMatrix input = VxMatrix::Identity(), output;
        Vx3DInverseMatrix(output, input);
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                if (row == column) continue;
                uint32_t bits;
                memcpy(&bits, &output[row][column], sizeof(bits));
                const bool negative = ((row + column) & 1) != (rounding == 0x0400);
                EXPECT_EQ(negative ? 0x80000000u : 0u, bits) << rounding << ',' << row << ',' << column;
            }
        }
    }
}

TEST(Float80Arithmetic, SavedFanBoxKeepsTheOriginalExitBoundary) {
    const float data[2][15] = {{355.334045f, 30.1296177f, -140.790283f, -0.134276867f, 0.791785657f, -0.595856667f,
                                0.937362313f, -0.0935582221f, -0.335557401f, -0.321436852f, -0.603591204f,
                                -0.729627311f, 2.00469851f, 1.99971092f, 1.99858856f},
                               {357.281708f, 27.11129f, -145.131653f, 1.0f, 1.00000284e-06f, 1.0000058e-06f,
                                1.00420334e-06f, 0.999991179f, -0.00420069788f, -9.98966925e-07f, 0.00105021556f,
                                0.999999464f, 1.00000572f, 8.00004578f, 1.00000429f}};
    VxOBB boxes[2];
    memcpy(boxes, data, sizeof(data));
    RestoreControl restore;
    VxSetFPUControlWord(0x037f);
    EXPECT_FALSE(VxOBBOverlap(boxes[0], boxes[1]));
}

TEST(Float80Arithmetic, OriginalBoxQueriesMatchAtEveryPrecision) {
    RestoreControl restore;
    const XWORD words[] = {0x007f, 0x027f, 0x037f};
    for (int mode = 0; mode < 3; ++mode) {
        VxSetFPUControlWord(words[mode]);
        const AOResults grid = OOGrid(VxOBBOverlap);
        EXPECT_EQ(51450, grid.samples);
        EXPECT_EQ(mode == 0 ? 49 : 112, grid.errors);
        EXPECT_EQ(mode == 0 ? 735904768u : 3784622691u, grid.digest);
        const AOResults wide = OOWide(VxOBBOverlap);
        EXPECT_EQ(6, wide.errors);
        EXPECT_EQ(2279203325u, wide.digest);
        EXPECT_EQ(0, OOScales(VxOBBOverlap).errors);
        EXPECT_EQ(0, OOContacts(VxOBBOverlap).errors);
        EXPECT_EQ(0, OOReparam(VxOBBOverlap).errors);
        EXPECT_EQ(0, OOCross(VxOBBOverlap).errors);
    }
}

TEST(Float80Arithmetic, VectorNormalizationPreservesOriginalRangeBoundaries) {
    RestoreControl restore;
    VxSetFPUControlWord(0x037f);
    const VxVector zero = Normalize(VxVector(0, 0, 0));
    EXPECT_NE(zero.x, zero.x);
    EXPECT_NE(zero.y, zero.y);
    EXPECT_NE(zero.z, zero.z);
    const VxVector huge = Normalize(VxVector(0, 1e20f, 0));
    EXPECT_EQ(0, huge.x);
    EXPECT_EQ(0, huge.y);
    EXPECT_EQ(0, huge.z);
    const VxVector tiny = Normalize(VxVector(-1e-30f, 0, 0));
    EXPECT_LT(tiny.x, -FLT_MAX);
    EXPECT_NE(tiny.y, tiny.y);
    EXPECT_NE(tiny.z, tiny.z);
}

TEST(Float80Arithmetic, TranscendentalOperationsShareTheSameState) {
    VxFloatContext context;
    VxFloatInitContext(context);
    const VxFloat80 zero = VxFloat80Load(context, 0.0), one = VxFloat80Load(context, 1.0);
    bool range = true;
    EXPECT_EQ(0, VxFloat80StoreDouble(context, VxFloat80Sine(context, zero, &range)));
    EXPECT_FALSE(range);
    EXPECT_EQ(1, VxFloat80StoreDouble(context, VxFloat80Cosine(context, zero)));
    EXPECT_EQ(0, VxFloat80StoreDouble(context, VxFloat80Arctangent(context, zero, one)));
    EXPECT_EQ(1, VxFloat80StoreDouble(context, VxFloat80Power2Minus1(context, one)));
    EXPECT_EQ(0, VxFloat80StoreDouble(context, VxFloat80Log2Multiply(context, one, one)));
    EXPECT_EQ(1, VxFloat80StoreDouble(context, VxFloat80Exponential(context, zero)));
    EXPECT_EQ(8, VxFloat80StoreDouble(context, VxFloat80Scale(context, one, VxFloat80Load(context, 3))));
    EXPECT_EQ(2, VxFloat80StoreDouble(context, VxFloat80RoundInteger(context, VxFloat80Load(context, 2.5))));
    EXPECT_EQ(.6931471805599453, VxFloat80StoreDouble(context, VxFloat80NaturalLogarithmOfTwo(context)));
    EXPECT_EQ(1.4426950408889634, VxFloat80StoreDouble(context, VxFloat80Logarithm2OfE(context)));
}
