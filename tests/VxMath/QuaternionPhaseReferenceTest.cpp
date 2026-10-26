#include <math.h>
#include <float.h>
#include "VxMath.h"
#include <gtest/gtest.h>
#include "TrigonometricReferenceSamples.h"

static bool IsLargeFloatPhase(double angle) {
    return fabs(angle) > 1000000 && fabs(angle) <= FLT_MAX && double(static_cast<float>(angle)) == angle;
}

static void ExpectPrincipalRotation(const VxMatrix &matrix, int axis, double sine, double cosine) {
    double expected[4][4] = {};
    expected[axis][axis] = expected[3][3] = 1;
    const int j = (axis+1)%3, k = (axis+2)%3;
    expected[j][j] = expected[k][k] = cosine;
    expected[j][k] = -sine;
    expected[k][j] = sine;
    for (int row = 0; row < 4; ++row) for (int column = 0; column < 4; ++column)
        EXPECT_NEAR(expected[row][column],matrix[row][column],.000002);
}

TEST(QuaternionPhaseReferenceTest, MatrixAxisRotationRetainsLargeAnglePhase) {
    int phases = 0;
    for (const auto &sample : TrigonometricReference::Samples) {
        if (!IsLargeFloatPhase(sample.angle)) continue;
        for (int axis = 0; axis < 3; ++axis) {
            SCOPED_TRACE(sample.angle);
            SCOPED_TRACE(axis);
            VxVector direction(0,0,0); direction[axis] = 1;
            VxMatrix matrix;
            Vx3DMatrixFromRotation(matrix,direction,static_cast<float>(sample.angle));
            ExpectPrincipalRotation(matrix,axis,sample.sine,sample.cosine);
        }
        ++phases;
    }
    EXPECT_EQ(398,phases);
    RecordProperty("independent_large_float_phases",phases);
}

TEST(QuaternionPhaseReferenceTest, AxisConstructorAndFromRotationMatchIndependentMatrices) {
    for (const auto &sample : TrigonometricReference::Samples) {
        if (!IsLargeFloatPhase(sample.angle)) continue;
        for (int axis = 0; axis < 3; ++axis) {
            SCOPED_TRACE(sample.angle);
            SCOPED_TRACE(axis);
            VxVector direction(0,0,0); direction[axis] = 1;
            VxQuaternion constructed(direction,static_cast<float>(sample.angle)), assigned;
            assigned.FromRotation(direction,static_cast<float>(sample.angle));
            VxMatrix matrix;
            constructed.ToMatrix(matrix);
            ExpectPrincipalRotation(matrix,axis,sample.sine,sample.cosine);
            assigned.ToMatrix(matrix);
            ExpectPrincipalRotation(matrix,axis,sample.sine,sample.cosine);
        }
    }
}

TEST(QuaternionPhaseReferenceTest, EulerMatrixAndQuaternionKeepTheirNegativeAngleConvention) {
    for (const auto &sample : TrigonometricReference::Samples) {
        if (!IsLargeFloatPhase(sample.angle)) continue;
        for (int axis = 0; axis < 3; ++axis) {
            SCOPED_TRACE(sample.angle);
            SCOPED_TRACE(axis);
            float angles[3] = {}; angles[axis] = static_cast<float>(sample.angle);
            VxMatrix matrix;
            Vx3DMatrixFromEulerAngles(matrix,angles[0],angles[1],angles[2]);
            ExpectPrincipalRotation(matrix,axis,-sample.sine,sample.cosine);
            VxQuaternion q; q.FromEulerAngles(angles[0],angles[1],angles[2]);
            q.ToMatrix(matrix);
            ExpectPrincipalRotation(matrix,axis,-sample.sine,sample.cosine);
        }
    }
}

TEST(QuaternionPhaseReferenceTest, PivotRotationKeepsItsOriginAtLargeAngles) {
    const VxVector origin(2,-3,5);
    for (const auto &sample : TrigonometricReference::Samples) {
        if (!IsLargeFloatPhase(sample.angle)) continue;
        VxMatrix matrix;
        Vx3DMatrixFromRotationAndOrigin(matrix,VxVector(0,0,1),origin,static_cast<float>(sample.angle));
        EXPECT_NEAR(sample.cosine,matrix[0][0],.000002);
        EXPECT_NEAR(-sample.sine,matrix[0][1],.000002);
        EXPECT_NEAR(sample.sine,matrix[1][0],.000002);
        for (int column = 0; column < 3; ++column) {
            const double transformed = double(origin.x)*matrix[0][column] + double(origin.y)*matrix[1][column]
                + double(origin.z)*matrix[2][column] + matrix[3][column];
            EXPECT_NEAR(origin[column],transformed,.000002);
        }
    }
}

TEST(QuaternionPhaseReferenceTest, SinCosPreservesSignedZeroAndPropagatesNonfiniteArguments) {
    double sine,cosine;
    VxQuaternionSinCos(-0.0,sine,cosine);
    EXPECT_EQ(0,sine); EXPECT_LT(1/sine,0); EXPECT_EQ(1,cosine);
    VxQuaternionSinCos(HUGE_VAL,sine,cosine);
    EXPECT_TRUE(sine != sine && cosine != cosine);
    VxQuaternionSinCos(nan(""),sine,cosine);
    EXPECT_TRUE(sine != sine && cosine != cosine);
}

TEST(QuaternionPhaseReferenceTest, ExpRetainsPhaseForExactMixedComponentLengths) {
    // 460-digit Decimal reference for lengths 5*2^exponent. The 3-4-5 norm is exact.
    const int exponents[] = {0,30,70,120};
    const double sine[] = {-.95892427466313845,.18405058710354458,-.95481566851716926,.93362043763201041};
    const double cosine[] = {.28366218546322625,-.98291677236012231,.29719865267882889,.35826369957869486};
    for (int i = 0; i < 4; ++i) {
        const float scale = ldexpf(1,exponents[i]);
        const VxQuaternion result = Exp(VxQuaternion(3*scale,4*scale,0,nanf("")));
        EXPECT_NEAR(.6*sine[i],result.x,.0000002);
        EXPECT_NEAR(.8*sine[i],result.y,.0000002);
        EXPECT_EQ(0,result.z);
        EXPECT_NEAR(cosine[i],result.w,.0000002);
    }
}

#if defined(_MSC_VER) && defined(_M_IX86)
TEST(QuaternionPhaseReferenceTest, CorrectPhaseDoesNotChangeTheCallersX87Precision) {
    unsigned int saved = 0;
    ASSERT_EQ(0,_controlfp_s(&saved,0,0));
    const unsigned int precision[] = {_PC_24,_PC_53,_PC_64};
    for (unsigned int mode : precision) {
        unsigned int control = 0;
        EXPECT_EQ(0,_controlfp_s(&control,mode,_MCW_PC));
        double sine,cosine;
        VxQuaternionSinCos(double(1e20f),sine,cosine);
        EXPECT_NEAR(.65657667785459028,sine,.000000000000002);
        EXPECT_NEAR(.75425928307010548,cosine,.000000000000002);
        EXPECT_EQ(0,_controlfp_s(&control,0,0));
        EXPECT_EQ(mode,control & _MCW_PC);
    }
    unsigned int ignored = 0;
    EXPECT_EQ(0,_controlfp_s(&ignored,saved,_MCW_PC));
}
#endif
