#include <stddef.h>
#include <math.h>
#include <string.h>
#include <new>
#ifndef VX_QUATERNION_CLASS_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif

struct QuaternionClassChecks {
    int checks = 0, errors = 0;
    void Check(bool valid) { ++checks; errors += !valid; }
    void Value(const VxQuaternion &q, float x, float y, float z, float w, float tolerance = 0) {
        Check(fabs(q.x-x) <= tolerance && fabs(q.y-y) <= tolerance &&
              fabs(q.z-z) <= tolerance && fabs(q.w-w) <= tolerance);
    }
};

static QuaternionClassChecks QuaternionClassLayout() {
    QuaternionClassChecks checks;
    checks.Check(sizeof(VxQuaternion) == 16 && alignof(VxQuaternion) == 4);
    checks.Check(offsetof(VxQuaternion,x) == 0 && offsetof(VxQuaternion,y) == 4 &&
                 offsetof(VxQuaternion,z) == 8 && offsetof(VxQuaternion,w) == 12 && offsetof(VxQuaternion,angle) == 12);
    checks.Check(__is_standard_layout(VxQuaternion) && __is_trivially_copyable(VxQuaternion));
    VxQuaternion identity, value(1,2,3,4);
    checks.Value(identity,0,0,0,1);
    const VxQuaternion &constant = value;
    for (int i = 0; i < 4; ++i) {
        checks.Check(&value[i] == &constant[i]);
        checks.Check(reinterpret_cast<const char *>(&constant[i]) - reinterpret_cast<const char *>(&value) == i*4);
        checks.Check(constant[i] == i+1);
        value[i] = static_cast<float>(i+5);
    }
    checks.Value(value,5,6,7,8);
    value.angle = 9;
    checks.Check(value.w == 9);
#if defined(_MSC_VER)
    checks.Check(&value.axis.x == &value.x && &value.axis.y == &value.y && &value.axis.z == &value.z);
    checks.Check(&value.v[0] == &value.x && &value.v[3] == &value.w);
#endif
    VxQuaternion copy(value), moved(static_cast<VxQuaternion &&>(copy));
    copy = value;
    moved = static_cast<VxQuaternion &&>(copy);
    checks.Value(moved,5,6,7,9);
    VxQuaternion array[2];
    checks.Check(reinterpret_cast<char *>(array+1) - reinterpret_cast<char *>(array) == 16);
    return checks;
}

static QuaternionClassChecks QuaternionClassComponents() {
    QuaternionClassChecks checks;
    const VxQuaternion a(1,2,3,4), b(5,6,7,8);
    checks.Value(a+b,6,8,10,12);
    checks.Value(a-b,-4,-4,-4,-4);
    checks.Value(+a,1,2,3,4);
    checks.Value(-a,-1,-2,-3,-4);
    checks.Value(a*.5f,.5f,1,1.5f,2);
    checks.Value(.5f*a,.5f,1,1.5f,2);
    VxQuaternion scaled(a);
    checks.Check(&(scaled *= .5f) == &scaled);
    checks.Value(scaled,.5f,1,1.5f,2);
    checks.Value(a,1,2,3,4);
    checks.Check(Magnitude(a) == 30 && DotProduct(a,b) == 70);
    checks.Value(Vx3DQuaternionConjugate(a),-1,-2,-3,4);
    VxQuaternion zero(0,0,0,0), signedZero(-0.0f,0,-0.0f,0);
    checks.Check(zero == signedZero && !(zero != signedZero));
    checks.Check(a != b && !(a == b));
    VxQuaternion unordered(a);
    unordered.w = nanf("");
    checks.Check(!(unordered == unordered) && unordered != unordered);
    return checks;
}

static QuaternionClassChecks QuaternionClassProducts() {
    QuaternionClassChecks checks;
    const VxQuaternion basis[] = {VxQuaternion(1,0,0,0), VxQuaternion(0,1,0,0),
                                  VxQuaternion(0,0,1,0), VxQuaternion()};
    const int index[4][4] = {{3,2,1,0},{2,3,0,1},{1,0,3,2},{0,1,2,3}};
    const int sign[4][4] = {{-1,1,-1,1},{-1,-1,1,1},{1,-1,-1,1},{1,1,1,1}};
    // Independent Hamilton basis table: i*j=k, j*i=-k, i*i=j*j=k*k=-1.
    for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) {
        const VxQuaternion expected = sign[i][j] * basis[index[i][j]];
        const VxQuaternion free = Vx3DQuaternionMultiply(basis[i],basis[j]);
        const VxQuaternion product = basis[i] * basis[j];
        VxQuaternion member(basis[i]);
        member.Multiply(basis[j]);
        checks.Value(free,expected.x,expected.y,expected.z,expected.w);
        checks.Value(product,expected.x,expected.y,expected.z,expected.w);
        checks.Value(member,expected.x,expected.y,expected.z,expected.w);
    }
    const VxQuaternion a(1,2,3,4), b(5,6,7,8);
    checks.Value(a*b,24,48,48,-6);
    VxQuaternion self(a);
    self.Multiply(self);
    checks.Value(self,8,16,24,2);
    // The legacy quotient is conjugate(b)*a, without dividing by |b| squared.
    checks.Value(a/b,-16,0,-8,70);
    checks.Value(Vx3DQuaternionDivide(a,b),-16,0,-8,70);
    checks.Value(a/a,0,0,0,30);
    checks.Value(a,1,2,3,4);
    checks.Value(b,5,6,7,8);
    return checks;
}

static QuaternionClassChecks QuaternionClassNormalize() {
    QuaternionClassChecks checks;
    VxQuaternion zero(0,0,0,0);
    zero.Normalize();
    checks.Value(zero,0,0,0,1);
    VxQuaternion value(3,0,0,4);
    value.Normalize();
    checks.Value(value,.6f,0,0,.8f,.000002f);
    VxQuaternion negative(-3,0,0,-4);
    negative.Normalize();
    checks.Value(negative,-.6f,0,0,-.8f,.000002f);
    return checks;
}

static QuaternionClassChecks QuaternionClassAxisEuler() {
    QuaternionClassChecks checks;
    const float angle = .6f;
    const float sine = static_cast<float>(sin(.5*angle)), cosine = static_cast<float>(cos(.5*angle));
    for (int axis = 0; axis < 3; ++axis) {
        VxVector direction(0,0,0);
        direction[axis] = 3;
        VxQuaternion constructed(direction,angle), rotation;
        rotation.FromRotation(direction,angle);
        float expected[4] = {0,0,0,cosine};
        expected[axis] = sine;
        checks.Value(constructed,expected[0],expected[1],expected[2],expected[3],.000002f);
        checks.Value(rotation,expected[0],expected[1],expected[2],expected[3],.000002f);
        float euler[3] = {0,0,0};
        euler[axis] = angle;
        rotation.FromEulerAngles(euler[0],euler[1],euler[2]);
        expected[axis] = -sine;
        checks.Value(rotation,expected[0],expected[1],expected[2],expected[3],.000002f);
        float returned[3] = {9,9,9};
        rotation.ToEulerAngles(returned,returned+1,returned+2);
        for (int i = 0; i < 3; ++i) checks.Check(fabs(returned[i]-euler[i]) < .000002f);
    }
    return checks;
}

static QuaternionClassChecks QuaternionClassMatrixBranches() {
    QuaternionClassChecks checks;
    const float diagonal[][3] = {{1,1,1},{1,-1,-1},{-1,1,-1},{-1,-1,1}};
    const float expected[][4] = {{0,0,0,1},{1,0,0,0},{0,1,0,0},{0,0,1,0}};
    for (int branch = 0; branch < 4; ++branch) {
        VxMatrix matrix;
        matrix.SetIdentity();
        for (int i = 0; i < 3; ++i) matrix[i][i] = diagonal[branch][i];
        const VxMatrix before = matrix;
        VxQuaternion q;
        q.FromMatrix(matrix);
        checks.Value(q,expected[branch][0],expected[branch][1],expected[branch][2],expected[branch][3]);
        checks.Value(Vx3DQuaternionFromMatrix(matrix),q.x,q.y,q.z,q.w);
        checks.Check(memcmp(&before,&matrix,sizeof(matrix)) == 0);
    }
    // q=(1,1,1,1) rotates the three basis vectors cyclically; any nonzero scale is equivalent.
    const float expectedRotation[][4] = {{0,0,1,0},{1,0,0,0},{0,1,0,0},{0,0,0,1}};
    const float scales[] = {1,3,-2};
    for (float scale : scales) {
        VxMatrix matrix;
        memset(&matrix,0xA5,sizeof(matrix));
        VxQuaternion(scale,scale,scale,scale).ToMatrix(matrix);
        for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j)
            checks.Check(fabs(matrix[i][j]-expectedRotation[i][j]) < .000002f);
    }
    return checks;
}

static QuaternionClassChecks QuaternionClassMatrixRestore() {
    QuaternionClassChecks checks;
    VxMatrix source;
    for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) source[i][j] = static_cast<float>(i*4+j+10);
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) source[i][j] = i==j ? float(i+2) : 0;
    source[2][2] = 0; // A rank-two frame: the third row must be rebuilt from the first two.
    for (int restore = 0; restore < 2; ++restore) {
        VxMatrix matrix = source;
        VxQuaternion q;
        q.FromMatrix(matrix,FALSE,restore);
        checks.Value(q,0,0,0,1,.000002f);
        for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j)
            checks.Check(fabs(matrix[i][j] - (!restore && i<3 && j<3 ? float(i==j) : source[i][j])) < .000002f);
    }
    return checks;
}

static QuaternionClassChecks QuaternionClassSpecialValues() {
    QuaternionClassChecks checks;
    VxQuaternion unordered(nanf(""),1,2,3);
    unordered.Normalize();
    for (int i = 0; i < 4; ++i) checks.Check(unordered[i] != unordered[i]);
    VxQuaternion infinite(HUGE_VALF,1,2,3);
    infinite.Normalize();
    checks.Check(infinite.x != infinite.x && infinite.y == 0 && infinite.z == 0 && infinite.w == 0);
    VxMatrix matrix;
    VxQuaternion(0,0,0,0).ToMatrix(matrix);
    for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j)
        checks.Check(i<3 && j<3 ? matrix[i][j] != matrix[i][j] : matrix[i][j] == float(i==j));
    VxQuaternion zeroAxis;
    zeroAxis.FromRotation(VxVector(0,0,0),.6f);
    for (int i = 0; i < 4; ++i) checks.Check(zeroAxis[i] != zeroAxis[i]);
    VxQuaternion rotation;
    rotation.FromEulerAngles(.1f,.2f,.3f);
    float shared = 9;
    rotation.ToEulerAngles(&shared,&shared,&shared);
    checks.Check(fabs(shared-.3f) < .000002f);
#ifndef VX_QUATERNION_CLASS_PROBE
    // The portable wrapper permits omitted outputs; the original DLL dereferences them.
    rotation.ToEulerAngles(nullptr,nullptr,nullptr);
#endif
    return checks;
}

static QuaternionClassChecks QuaternionClassFootprint() {
    QuaternionClassChecks checks;
    struct Storage { unsigned int before; VxQuaternion q; unsigned int after; } storage;
    memset(&storage,0xA5,sizeof(storage));
    VxQuaternion *q = new (&storage.q) VxQuaternion(1,2,3,4);
    q->Multiply(*q);
    checks.Value(*q,8,16,24,2);
    q->Normalize();
    q->FromRotation(VxVector(0,0,1),.6f);
    q->~VxQuaternion();
    checks.Check(storage.before == 0xA5A5A5A5 && storage.after == 0xA5A5A5A5);
    return checks;
}

#ifndef VX_QUATERNION_CLASS_PROBE
TEST(QuaternionClassReferenceTest, LayoutFieldsIndexingAndImplicitLifecycle) { EXPECT_EQ(0,QuaternionClassLayout().errors); }
TEST(QuaternionClassReferenceTest, ComponentArithmeticAndNumericEquality) { EXPECT_EQ(0,QuaternionClassComponents().errors); }
TEST(QuaternionClassReferenceTest, HamiltonBasisProductsAndRelativeQuotient) { EXPECT_EQ(0,QuaternionClassProducts().errors); }
TEST(QuaternionClassReferenceTest, NormalizationAndZeroIdentity) { EXPECT_EQ(0,QuaternionClassNormalize().errors); }
TEST(QuaternionClassReferenceTest, AxisAndEulerRotationHaveKnownSignedDirections) { EXPECT_EQ(0,QuaternionClassAxisEuler().errors); }
TEST(QuaternionClassReferenceTest, MatrixTraceBranchesAndCompleteOutput) { EXPECT_EQ(0,QuaternionClassMatrixBranches().errors); }
TEST(QuaternionClassReferenceTest, MatrixNormalizationHonorsRestoreAndOnlyChangesNineEntries) { EXPECT_EQ(0,QuaternionClassMatrixRestore().errors); }
TEST(QuaternionClassReferenceTest, ZeroNonfiniteAndOptionalOutputSemantics) { EXPECT_EQ(0,QuaternionClassSpecialValues().errors); }
TEST(QuaternionClassReferenceTest, InPlaceOperationsStayWithinSixteenBytes) { EXPECT_EQ(0,QuaternionClassFootprint().errors); }

#if defined(VX_SIMD_SSE)
TEST(QuaternionClassReferenceTest, RawSimdDotBroadcastAndQuaternionLaneOrder) {
    const __m128 a = _mm_setr_ps(1,2,3,4), b = _mm_setr_ps(5,6,7,8);
    float values[4];
    _mm_storeu_ps(values,VxSIMDDotProduct4(a,b));
    for (int i = 0; i < 4; ++i) EXPECT_EQ(70,values[i]);
    _mm_storeu_ps(values,VxSIMDQuaternionConjugate(a));
    EXPECT_EQ(-1,values[0]); EXPECT_EQ(-2,values[1]); EXPECT_EQ(-3,values[2]); EXPECT_EQ(4,values[3]);
    _mm_storeu_ps(values,VxSIMDQuaternionMultiply(a,b));
    EXPECT_EQ(24,values[0]); EXPECT_EQ(48,values[1]); EXPECT_EQ(48,values[2]); EXPECT_EQ(-6,values[3]);
}

TEST(QuaternionClassReferenceTest, SimdAdaptersAllowEitherInputAsOutput) {
    const VxQuaternion a(1,2,3,4), b(5,6,7,8);
    for (int alias = 0; alias < 2; ++alias) {
        VxQuaternion left(a),right(b);
        VxQuaternion *out = alias ? &right : &left;
        VxSIMDMultiplyQuaternion(out,&left,&right);
        EXPECT_EQ(24,out->x); EXPECT_EQ(48,out->y); EXPECT_EQ(48,out->z); EXPECT_EQ(-6,out->w);
        left=a; right=b;
        VxSIMDDivideQuaternion(out,&left,&right);
        EXPECT_EQ(-16,out->x); EXPECT_EQ(0,out->y); EXPECT_EQ(-8,out->z); EXPECT_EQ(70,out->w);
    }
    VxQuaternion value(a);
    VxSIMDConjugateQuaternion(&value,&value);
    EXPECT_EQ(-1,value.x); EXPECT_EQ(-2,value.y); EXPECT_EQ(-3,value.z); EXPECT_EQ(4,value.w);
    VxSIMDScaleQuaternion(&value,&value,2);
    EXPECT_EQ(-2,value.x); EXPECT_EQ(-4,value.y); EXPECT_EQ(-6,value.z); EXPECT_EQ(8,value.w);
    EXPECT_EQ(120,VxSIMDMagnitudeQuaternion(&value));
    EXPECT_EQ(4,VxSIMDDotQuaternion(&value,&a));
}
#endif
#endif
