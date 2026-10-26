#include <math.h>
#include <float.h>
#include <string.h>
#ifndef VX_QUATERNION_FUNCTIONS_PROBE
#include "VxMath.h"
#include <gtest/gtest.h>
#endif

struct QuaternionFunctionChecks {
    int checks = 0, errors = 0;
    void Check(bool valid) { ++checks; errors += !valid; }
    void Value(const VxQuaternion &q, double x, double y, double z, double w, double tolerance = .000004) {
        Check(fabs(q.x-x) <= tolerance && fabs(q.y-y) <= tolerance &&
              fabs(q.z-z) <= tolerance && fabs(q.w-w) <= tolerance);
    }
};

static VxQuaternion ZRotation(double halfAngle) {
    return VxQuaternion(0,0,static_cast<float>(sin(halfAngle)),static_cast<float>(cos(halfAngle)));
}

static QuaternionFunctionChecks QuaternionSlerpArcs() {
    QuaternionFunctionChecks checks;
    const VxQuaternion start, end(1,0,0,0), negativeEnd(-1,0,0,0);
    const float phases[] = {-.5f,0,.25f,.5f,.75f,1,1.5f};
    const double halfPi = 1.57079632679489661923;
    for (float t : phases) {
        const VxQuaternion result = Slerp(t,start,end);
        checks.Value(result,sin(t*halfPi),0,0,cos(t*halfPi));
        // An exact zero dot product does not negate the second endpoint.
        checks.Value(Slerp(t,start,negativeEnd),-sin(t*halfPi),0,0,cos(t*halfPi));
    }
    const VxQuaternion a = ZRotation(.2), b = ZRotation(.8);
    for (float t : phases) {
        const double angle = .2+.6*t;
        const VxQuaternion opposite(-b.x,-b.y,-b.z,-b.w);
        checks.Value(Slerp(t,a,b),0,0,sin(angle),cos(angle));
        checks.Value(Slerp(t,a,opposite),0,0,sin(angle),cos(angle));
    }
    checks.Value(a,0,0,sin(.2),cos(.2));
    checks.Value(b,0,0,sin(.8),cos(.8));
    return checks;
}

static QuaternionFunctionChecks QuaternionSlerpLinearBranch() {
    QuaternionFunctionChecks checks;
    const VxQuaternion a;
    const VxQuaternion b = ZRotation(.05);
    const float phases[] = {0,.25f,.5f,1};
    for (float t : phases) {
        checks.Value(Slerp(t,a,b),0,0,t*b.z,1.0-t+t*b.w,.0000002);
    }
    const VxQuaternion midpoint = Slerp(.5f,a,b);
    checks.Check(midpoint.z*midpoint.z+midpoint.w*midpoint.w < 1);
    const VxQuaternion identical(.5f,-.5f,.5f,.5f), antipode(-.5f,.5f,-.5f,-.5f);
    const float endpointPhases[] = {-2,0,.5f,1,2};
    for (float t : endpointPhases) {
        checks.Value(Slerp(t,identical,identical),.5,-.5,.5,.5,0);
        checks.Value(Slerp(t,identical,antipode),.5,-.5,.5,.5,0);
    }
    // Non-unit inputs are not implicitly normalized in the close branch.
    checks.Value(Slerp(.5f,VxQuaternion(1,0,0,1),VxQuaternion(1,0,0,2)),1,0,0,1.5);
    return checks;
}

static QuaternionFunctionChecks QuaternionSquadCurve() {
    QuaternionFunctionChecks checks;
    const VxQuaternion start, out = ZRotation(1.1), in = ZRotation(1.4), end = ZRotation(.8);
    const float phases[] = {0,.125f,.25f,.5f,.75f,.875f,1};
    for (float t : phases) {
        // Coaxial controls reduce the spherical Bezier blend to this cubic angle.
        // All three slerps stay outside the close-angle approximation branch.
        const double angle = .8*t+2.0*t*(1.0-t)*(1.1-.5*t);
        checks.Value(Squad(t,start,out,in,end),0,0,sin(angle),cos(angle));
    }
    return checks;
}

static QuaternionFunctionChecks QuaternionLogarithms() {
    QuaternionFunctionChecks checks;
    const double angle = atan2(3.0,4.0);
    const VxQuaternion a(1,2,2,4), scaled(3,6,6,12);
    checks.Value(Ln(a),angle/3,2*angle/3,2*angle/3,0);
    checks.Value(Ln(scaled),angle/3,2*angle/3,2*angle/3,0);
    checks.Value(Ln(VxQuaternion(0,0,0,-1)),0,0,0,0,0);
    checks.Value(Ln(VxQuaternion(0,0,0,0)),0,0,0,0,0);
    // i^-1*j = -k, so LnDif(i,j) must point along negative Z.
    checks.Value(LnDif(VxQuaternion(1,0,0,0),VxQuaternion(0,1,0,0)),0,0,-1.57079632679489661923,0);
    checks.Value(LnDif(a,a),0,0,0,0);
    checks.Value(LnDif(ZRotation(.2),ZRotation(.8)),0,0,.6,0);
    checks.Value(a,1,2,2,4,0);
    return checks;
}

static QuaternionFunctionChecks QuaternionExponentials() {
    QuaternionFunctionChecks checks;
    const double halfPi = 1.57079632679489661923;
    checks.Value(Exp(VxQuaternion(0,0,0,99)),0,0,0,1,0);
    checks.Value(Exp(VxQuaternion(0,0,0,nanf(""))),0,0,0,1,0);
    const float phase = static_cast<float>(halfPi);
    checks.Value(Exp(VxQuaternion(phase,0,0,7)),sin(phase),0,0,cos(phase));
    checks.Value(Exp(VxQuaternion(-phase,0,0,-7)),-sin(phase),0,0,cos(phase));
    checks.Value(Exp(VxQuaternion(1e-25f,0,0,1)),1e-25f,0,0,1,0);
    const VxQuaternion unit(.6f,0,0,.8f);
    checks.Value(Exp(Ln(unit)),.6,0,0,.8);
    return checks;
}

static void QuaternionScaleTensor(const VxQuaternion &q, const VxVector &scale, double tensor[3][3]) {
    const double length = sqrt(double(q.x)*q.x+double(q.y)*q.y+double(q.z)*q.z+double(q.w)*q.w);
    const double u[] = {q.x/length,q.y/length,q.z/length}, w = q.w/length;
    double basis[3][3];
    for (int column = 0; column < 3; ++column) {
        double vector[3] = {0,0,0}; vector[column] = 1;
        double cross[3];
        for (int i = 0; i < 3; ++i) cross[i] = u[(i+1)%3]*vector[(i+2)%3]-u[(i+2)%3]*vector[(i+1)%3];
        for (int i = 0; i < 3; ++i)
            basis[i][column] = vector[i]+2*(w*cross[i]+u[(i+1)%3]*cross[(i+2)%3]-u[(i+2)%3]*cross[(i+1)%3]);
    }
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) {
        tensor[i][j] = 0;
        for (int k = 0; k < 3; ++k) tensor[i][j] += basis[i][k]*scale[k]*basis[j][k];
    }
}

static QuaternionFunctionChecks QuaternionSnuggleTensor() {
    QuaternionFunctionChecks checks;
    const float rootHalf = .7071067811865475f;
    const VxQuaternion inputs[] = {VxQuaternion(),VxQuaternion(1,0,0,0),VxQuaternion(0,1,0,0),VxQuaternion(0,0,1,0),
        VxQuaternion(.6f,0,0,.8f),VxQuaternion(0,.6f,0,.8f),VxQuaternion(0,0,.6f,.8f),
        VxQuaternion(.5f,.5f,.5f,.5f),VxQuaternion(.5f,-.5f,.5f,-.5f),VxQuaternion(rootHalf,0,0,rootHalf),
        VxQuaternion(.6f,.8f,0,0),VxQuaternion(0,0,.1f,sqrtf(.99f))};
    const VxVector scales[] = {VxVector(1,2,3),VxVector(3,1,2),VxVector(-2,0,4),VxVector(2,2,5),VxVector(2,5,2),
        VxVector(5,2,2),VxVector(2,2,2),VxVector(2,0,0),VxVector(0,3,0),VxVector(0,0,4),VxVector(-1,-1,-1)};
    for (const VxQuaternion &input : inputs) for (const VxVector &originalScale : scales) {
        VxQuaternion rewritten(input);
        VxVector scale(originalScale);
        const VxQuaternion correction = Vx3DQuaternionSnuggle(&rewritten,&scale);
        // Snuggle mutates its workspace. The rotation to compose is the original input.
        const VxQuaternion final = Vx3DQuaternionMultiply(input,correction);
        double before[3][3], after[3][3];
        QuaternionScaleTensor(input,originalScale,before);
        QuaternionScaleTensor(final,scale,after);
        for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j)
            checks.Check(fabs(before[i][j]-after[i][j]) < .00002);
        float sortedBefore[] = {originalScale.x,originalScale.y,originalScale.z}, sortedAfter[] = {scale.x,scale.y,scale.z};
        for (int i = 0; i < 3; ++i) for (int j = i+1; j < 3; ++j) {
            if (sortedBefore[j] < sortedBefore[i]) { float t=sortedBefore[i]; sortedBefore[i]=sortedBefore[j]; sortedBefore[j]=t; }
            if (sortedAfter[j] < sortedAfter[i]) { float t=sortedAfter[i]; sortedAfter[i]=sortedAfter[j]; sortedAfter[j]=t; }
        }
        for (int i = 0; i < 3; ++i) checks.Check(sortedBefore[i] == sortedAfter[i]);
        double norm = 0;
        for (int i = 0; i < 4; ++i) norm += double(correction[i])*correction[i];
        checks.Check(fabs(norm-1) < .000004);
        checks.Check(fabs(final.w)+.000004 >= fabs(input.w));
        if ((originalScale.x != originalScale.y && originalScale.y != originalScale.z && originalScale.x != originalScale.z)
                || (originalScale.x == originalScale.y && originalScale.y == originalScale.z))
            checks.Check(memcmp(&rewritten,&input,sizeof(input)) == 0);
    }
    return checks;
}

static QuaternionFunctionChecks QuaternionSnuggleWrites() {
    QuaternionFunctionChecks checks;
    const float rootHalf = .7071067811865475f;
    const VxVector scales[] = {VxVector(5,2,2),VxVector(2,5,2),VxVector(2,2,5)};
    const float expected[][4] = {{0,-rootHalf,0,rootHalf},{-rootHalf,0,0,rootHalf},{0,0,0,1}};
    for (int i = 0; i < 3; ++i) {
        struct QuaternionStorage { unsigned int before; VxQuaternion q; unsigned int after; } rotation;
        struct ScaleStorage { unsigned int before; VxVector value; unsigned int after; } scaling;
        rotation.before = rotation.after = scaling.before = scaling.after = 0xA5A5A5A5;
        scaling.value = scales[i];
        Vx3DQuaternionSnuggle(&rotation.q,&scaling.value);
        checks.Value(rotation.q,expected[i][0],expected[i][1],expected[i][2],expected[i][3]);
        checks.Check(rotation.before == 0xA5A5A5A5 && rotation.after == 0xA5A5A5A5);
        checks.Check(scaling.before == 0xA5A5A5A5 && scaling.after == 0xA5A5A5A5);
    }
    return checks;
}

#ifndef VX_QUATERNION_FUNCTIONS_PROBE
TEST(QuaternionFunctionsReferenceTest, SlerpShortestArcHemisphereAndOrdinaryExtrapolation) { EXPECT_EQ(0,QuaternionSlerpArcs().errors); }
TEST(QuaternionFunctionsReferenceTest, SlerpCloseBranchIsLinearWithoutRenormalization) { EXPECT_EQ(0,QuaternionSlerpLinearBranch().errors); }
TEST(QuaternionFunctionsReferenceTest, SquadFollowsAnIndependentCoaxialCubic) { EXPECT_EQ(0,QuaternionSquadCurve().errors); }
TEST(QuaternionFunctionsReferenceTest, LogIsRotationOnlyAndLogDifferenceHasTheOriginalOrder) { EXPECT_EQ(0,QuaternionLogarithms().errors); }
TEST(QuaternionFunctionsReferenceTest, ExpIgnoresScalarPartAndHandlesZeroAndTinyVectors) { EXPECT_EQ(0,QuaternionExponentials().errors); }
TEST(QuaternionFunctionsReferenceTest, SnugglePreservesScaleTensorAndChoosesASmallerRotation) { EXPECT_EQ(0,QuaternionSnuggleTensor().errors); }
TEST(QuaternionFunctionsReferenceTest, SnuggleWorkspaceMutationAndExactWriteFootprints) { EXPECT_EQ(0,QuaternionSnuggleWrites().errors); }

TEST(QuaternionFunctionsReferenceTest, SnuggleKeepsExistingNullInputExtension) {
    VxQuaternion q(.5f,.5f,.5f,.5f);
    VxVector scale(1,2,3);
    EXPECT_EQ(1,Vx3DQuaternionSnuggle(nullptr,&scale).w);
    EXPECT_EQ(1,Vx3DQuaternionSnuggle(&q,nullptr).w);
    EXPECT_EQ(1,Vx3DQuaternionSnuggle(nullptr,nullptr).w);
    EXPECT_EQ(1,scale.x); EXPECT_EQ(2,scale.y); EXPECT_EQ(3,scale.z);
    EXPECT_EQ(.5f,q.x); EXPECT_EQ(.5f,q.y); EXPECT_EQ(.5f,q.z); EXPECT_EQ(.5f,q.w);
}

#if defined(VX_SIMD_SSE)
TEST(QuaternionFunctionsReferenceTest, SimdSlerpAllowsEitherInputAsOutput) {
    const VxQuaternion a, b(1,0,0,0);
    for (int alias = 0; alias < 2; ++alias) {
        VxQuaternion left(a),right(b);
        VxQuaternion *out = alias ? &right : &left;
        VxSIMDSlerpQuaternion(out,.5f,&left,&right);
        EXPECT_NEAR(sqrt(.5),out->x,.0000002);
        EXPECT_EQ(0,out->y); EXPECT_EQ(0,out->z);
        EXPECT_NEAR(sqrt(.5),out->w,.0000002);
    }
}
#endif
#endif
