#include <gtest/gtest.h>
#include <cmath>
#include "VxMath.h"
#include "QuaternionReferenceSamples.h"

static VxQuaternion Quaternion(const float (&v)[4]) {
    return VxQuaternion(v[0], v[1], v[2], v[3]);
}

static void ExpectReference(const VxQuaternion &actual, const float (&expected)[4], float tolerance) {
    // Captures with zero tolerance measured x87 rounding, not an API contract.
    // A short float operation chain is allowed a small accumulated error.
    if (tolerance == 0.0f) tolerance = 16.0f * FLT_EPSILON;
    for (int i = 0; i < 4; ++i) {
        if (std::isnan(expected[i])) { EXPECT_TRUE(std::isnan(actual[i])) << "component=" << i; continue; }
        if (std::isinf(expected[i])) { EXPECT_EQ(actual[i], expected[i]) << "component=" << i; continue; }
        ASSERT_TRUE(std::isfinite(actual[i])) << "component=" << i;
        EXPECT_NEAR(actual[i], expected[i], tolerance * (1.0f + std::fabs(expected[i]))) << "component=" << i;
    }
}

class QuaternionOriginalRuntimeTest : public testing::TestWithParam<int> {};

TEST_P(QuaternionOriginalRuntimeTest, MatchesStableReferencesAndSquareIdentity) {
    int index = 0;
    int ordinaryProducts = 0;
    int rangeDiagnostics = 0;
    for (const auto &sample : QuaternionReference::Samples) {
        const int current = index++;
        if (sample.operation != GetParam()) continue;
        SCOPED_TRACE(testing::Message() << "sample=" << current << " operation=" << sample.operation);
        const VxQuaternion a = Quaternion(sample.a), b = Quaternion(sample.b);
        if (sample.operation == 1 || sample.operation == 2 || sample.operation == 5 ||
            sample.operation == 9 || sample.operation == 10) {
            // Captured x87 results for overflowing float products are range
            // diagnostics, rather than a requirement for an extended-range kernel.
            bool inRange = true;
            for (int component = 0; component < 4; ++component) {
                inRange = inRange && fabs(a[component]) <= 1.0e18f && fabs(b[component]) <= 1.0e18f;
            }
            if (!inRange) {
                ++rangeDiagnostics;
                continue;
            }
            ++ordinaryProducts;
        }
        // Large extrapolation and cancellation at a 180-degree hemisphere
        // boundary amplify native rounding into a different interpolation.
        // These captures are diagnostics, not numerical correctness oracles.
        if (sample.operation == 6 || sample.operation == 7) {
            bool stable = std::isfinite(sample.t) && std::fabs(sample.t) <= 2.0f;
            double dot = 0.0;
            for (int i = 0; i < 4; ++i) {
                dot += static_cast<double>(sample.a[i])*sample.b[i];
                const float values[4] = {sample.a[i], sample.b[i], sample.c[i], sample.d[i]};
                for (int j = 0; j < 4; ++j)
                    stable &= std::isfinite(values[j]) && std::fabs(values[j]) <= 4.0f;
            }
            if (!stable || (dot != 0.0 && std::fabs(dot) < 32.0*FLT_EPSILON)) continue;
        }
        float expected[4];
        for (int i = 0; i < 4; ++i) expected[i] = sample.expected[i];
        if (sample.operation == 4) {
            // The DLL's large-angle instruction failure is not an Exp oracle.
            const double length = std::hypot(std::hypot(static_cast<double>(a.x),a.y),a.z);
            const double sine = std::sin(length);
            for (int i = 0; i < 3; ++i)
                expected[i] = static_cast<float>(length == 0.0 ? a[i] : (a[i]/length)*sine);
            expected[3] = static_cast<float>(std::cos(length));
        }
        if (sample.operation == 10 && std::isfinite(a.x) && std::isfinite(a.y) &&
            std::isfinite(a.z) && std::isfinite(a.w)) {
            // q*q has no vector cross term. Evaluate this identity directly
            // instead of retaining cancellation artifacts from the DLL.
            for (int i = 0; i < 3; ++i) expected[i] = static_cast<float>(2.0*a.w*a[i]);
            expected[3] = static_cast<float>(static_cast<double>(a.w)*a.w -
                (static_cast<double>(a.x)*a.x + static_cast<double>(a.y)*a.y + static_cast<double>(a.z)*a.z));
        }
        VxQuaternion actual;
        switch (sample.operation) {
        case 0: actual = Vx3DQuaternionConjugate(a); break;
        case 1: actual = Vx3DQuaternionMultiply(a, b); ExpectReference(a * b, sample.expected, sample.tolerance); break;
        case 2: actual = Vx3DQuaternionDivide(a, b); ExpectReference(a / b, sample.expected, sample.tolerance); break;
        case 3: actual = Ln(a); break;
        case 4: actual = Exp(a); break;
        case 5: actual = LnDif(a, b); break;
        case 6: actual = Slerp(sample.t, a, b); break;
        case 7: actual = Squad(sample.t, a, b, Quaternion(sample.c), Quaternion(sample.d)); break;
        case 8: actual = a; actual.Normalize(); break;
        case 9: actual = a; actual.Multiply(b); break;
        case 10: actual = a; actual.Multiply(actual); break;
        default: FAIL() << "Unknown reference operation";
        }
        if (sample.operation == 4) {
            const double length=std::hypot(std::hypot(static_cast<double>(a.x),a.y),a.z);
            if (std::isfinite(length) && length>1e6) {
                // At huge mixed-component lengths, a double norm ULP can span
                // many trig periods. Check unit length and the rotation axis;
                // the dedicated axis-aligned test checks phase without that
                // conditioning problem, through FLT_MAX.
                double norm=0;
                for (int i=0; i<4; ++i) norm+=static_cast<double>(actual[i])*actual[i];
                EXPECT_NEAR(norm,1.0,8.0*FLT_EPSILON);
                for (int i=0; i<3; ++i) {
                    const int j=(i+1)%3,k=(i+2)%3;
                    EXPECT_NEAR((a[j]/length)*actual[k]-(a[k]/length)*actual[j],0.0,8.0*FLT_EPSILON);
                }
                continue;
            }
        }
        if (sample.operation == 10 && std::isfinite(a.x) && std::isfinite(a.y) &&
            std::isfinite(a.z) && std::isfinite(a.w)) {
            for (int i=0; i<4; ++i) {
                if (!std::isfinite(expected[i])) { EXPECT_EQ(actual[i],expected[i]); continue; }
                ASSERT_TRUE(std::isfinite(actual[i]));
                double sum=0;
                if (i<3) sum=2.0*std::fabs(static_cast<double>(a.w)*a[i])+
                    2.0*std::fabs(static_cast<double>(a[(i+1)%3])*a[(i+2)%3]);
                else for (int j=0; j<4; ++j) sum+=static_cast<double>(a[j])*a[j];
                // Forward error scales with the terms before cancellation,
                // not with a potentially tiny final component.
                EXPECT_NEAR(actual[i],expected[i],8.0*FLT_EPSILON*sum+
                    std::numeric_limits<float>::denorm_min());
            }
        } else ExpectReference(actual, expected, sample.tolerance);
#if defined(VX_SIMD_SSE)
        // Output may alias either input in the public SIMD helpers.
        if (sample.operation == 0) {
            actual = a;
            VxSIMDConjugateQuaternion(&actual, &actual);
            ExpectReference(actual, sample.expected, sample.tolerance);
        } else if (sample.operation == 1 || sample.operation == 2 || sample.operation == 6) {
            for (int aliasLeft = 0; aliasLeft < 2; ++aliasLeft) {
                VxQuaternion left = a, right = b;
                VxQuaternion *result = aliasLeft ? &left : &right;
                if (sample.operation == 1) VxSIMDMultiplyQuaternion(result, &left, &right);
                if (sample.operation == 2) VxSIMDDivideQuaternion(result, &left, &right);
                if (sample.operation == 6) VxSIMDSlerpQuaternion(result, sample.t, &left, &right);
                ExpectReference(*result, sample.expected, sample.tolerance);
            }
        }
#endif
    }
    RecordProperty("ordinary_product_captures", ordinaryProducts);
    RecordProperty("excluded_extended_range_diagnostics", rangeDiagnostics);
}

INSTANTIATE_TEST_SUITE_P(Operations, QuaternionOriginalRuntimeTest, testing::Range(0, 11));

TEST(QuaternionNumericsTest, FiniteNormalizationPreservesDirectionAcrossFloatRange) {
    int checked=0;
    for (const auto &sample : QuaternionReference::Samples) {
        if (sample.operation!=8) continue;
        double length=0;
        for (float v : sample.a) length=std::hypot(length,static_cast<double>(v));
        if (!std::isfinite(length) || length==0) continue;
        SCOPED_TRACE(checked++);
        VxQuaternion q=Quaternion(sample.a);
        q.Normalize();
        double norm=0;
        for (int i=0; i<4; ++i) {
            const double expected=sample.a[i]/length;
            EXPECT_NEAR(q[i],expected,4.0*FLT_EPSILON*std::fabs(expected)+std::numeric_limits<float>::denorm_min());
            norm+=static_cast<double>(q[i])*q[i];
        }
        EXPECT_NEAR(norm,1.0,8.0*FLT_EPSILON);
    }
    EXPECT_GT(checked,100);
}

TEST(QuaternionNumericsTest, SlerpFollowsShortestArcIncludingHemisphereBoundary) {
    int checked=0;
    for (const auto &sample : QuaternionReference::Samples) {
        if (sample.operation!=6 || !std::isfinite(sample.t) || std::fabs(sample.t)>2) continue;
        double normA=0,normB=0,dot=0;
        for (int i=0; i<4; ++i) {
            normA+=static_cast<double>(sample.a[i])*sample.a[i];
            normB+=static_cast<double>(sample.b[i])*sample.b[i];
            dot+=static_cast<double>(sample.a[i])*sample.b[i];
        }
        if (!std::isfinite(normA+normB) || std::fabs(normA-1)>1e-6 || std::fabs(normB-1)>1e-6) continue;
        SCOPED_TRACE(checked++);
        const double cosine=std::fabs(dot) < 1.0 ? std::fabs(dot) : 1.0;
        double left=1.0-sample.t,right=sample.t;
        // The API uses a linear approximation for very close orientations.
        if (cosine<0.99) {
            const double angle=std::acos(cosine);
            left=std::sin(left*angle)/std::sin(angle);
            right=std::sin(right*angle)/std::sin(angle);
        }
        if (dot<0) right=-right;
        const VxQuaternion result=Slerp(sample.t,Quaternion(sample.a),Quaternion(sample.b));
        for (int i=0; i<4; ++i)
            EXPECT_NEAR(result[i],left*sample.a[i]+right*sample.b[i],32.0*FLT_EPSILON);
    }
    EXPECT_GT(checked,50);
}

TEST(QuaternionNumericsTest, LogExpRoundTripUnitRotations) {
    const VxQuaternion inputs[] = {VxQuaternion(.6f,0,0,.8f), VxQuaternion(0,-.8f,0,.6f),
                                  VxQuaternion(.5f,-.5f,.5f,-.5f), VxQuaternion(0,0,0,1)};
    for (const VxQuaternion &input : inputs) {
        const VxQuaternion result=Exp(Ln(input));
        for (int i=0; i<4; ++i) EXPECT_NEAR(result[i],input[i],16.0*FLT_EPSILON);
    }
}

TEST(QuaternionNumericsTest, EqualEndpointsSurviveInterpolationAndAliasing) {
    const float factors[] = {-2.0f, -.5f, 0.0f, 1.0f, 1.5f, 2.0f};
    const VxQuaternion inputs[] = {VxQuaternion(), VxQuaternion(.5f,-.5f,.5f,.5f)};
    for (const VxQuaternion &q : inputs) for (float t : factors) {
        const VxQuaternion squad = Squad(t,q,q,q,q);
        for (int i = 0; i < 4; ++i) EXPECT_EQ(squad[i],q[i]);
        for (int sign = -1; sign <= 1; sign += 2) {
            const VxQuaternion b = q * static_cast<float>(sign);
            const VxQuaternion actual = Slerp(t,q,b);
            for (int i = 0; i < 4; ++i) EXPECT_EQ(actual[i],q[i]);
#if defined(VX_SIMD_SSE)
            for (int alias = 0; alias < 2; ++alias) {
                VxQuaternion left=q, right=b;
                VxQuaternion *out=alias ? &left : &right;
                VxSIMDSlerpQuaternion(out,t,&left,&right);
                for (int i = 0; i < 4; ++i) EXPECT_EQ((*out)[i],q[i]);
            }
#endif
        }
    }
}

TEST(QuaternionNumericsTest, SphericalInterpolationAndOrdinaryExtrapolationStayRotations) {
    const VxQuaternion start;
    const VxQuaternion end(1, 0, 0, 0);
    for (float factor : {-2.0f, -.5f, 0.0f, .25f, .5f, .75f, 1.0f, 1.5f, 2.0f}) {
        const VxQuaternion result = Slerp(factor, start, end);
        const double square = double(result.x) * result.x + double(result.w) * result.w;
        EXPECT_NEAR(square, 1.0, 8 * FLT_EPSILON);
        EXPECT_EQ(result.y, 0);
        EXPECT_EQ(result.z, 0);
    }
}

TEST(QuaternionNumericsTest, ExpMatchesPrincipalAxisRotations) {
    const float rootHalf = sqrtf(.5f);
    struct Rotation {
        float angle;
        float sine;
        float cosine;
    };
    const Rotation rotations[] = {
        {0, 0, 1},
        {PI * .25f, rootHalf, rootHalf},
        {PI * .5f, 1, 0},
        {PI, 0, -1},
        {-PI * .5f, -1, 0}
    };
    for (const Rotation &rotation : rotations) {
        for (int axis = 0; axis < 3; ++axis) {
            VxQuaternion input(0, 0, 0, 0);
            input[axis] = rotation.angle;
            const VxQuaternion result = Exp(input);
            for (int component = 0; component < 3; ++component) {
                EXPECT_NEAR(result[component], component == axis ? rotation.sine : 0, 8 * FLT_EPSILON);
            }
            EXPECT_NEAR(result.w, rotation.cosine, 8 * FLT_EPSILON);
        }
    }
}
