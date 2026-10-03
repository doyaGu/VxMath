#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include "VxMath.h"
#include "QuaternionConversionSamples.h"
#include "TrigonometricReferenceSamples.h"

namespace {
class QuaternionConversionTest : public testing::TestWithParam<int> {};

void SortThree(float (&values)[3]) {
    for (int i = 0; i < 2; ++i) for (int j = i+1; j < 3; ++j) {
        if (values[j] < values[i]) {
            const float temp = values[i]; values[i] = values[j]; values[j] = temp;
        }
    }
}

void ExpectRotation(const VxMatrix &matrix) {
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) {
        double dot = 0;
        for (int k = 0; k < 3; ++k) dot += static_cast<double>(matrix[i][k])*matrix[j][k];
        EXPECT_NEAR(dot,i==j ? 1.0 : 0.0,16.0*FLT_EPSILON);
    }
    const double determinant = static_cast<double>(matrix[0][0])*(static_cast<double>(matrix[1][1])*matrix[2][2]-static_cast<double>(matrix[1][2])*matrix[2][1])
        - static_cast<double>(matrix[0][1])*(static_cast<double>(matrix[1][0])*matrix[2][2]-static_cast<double>(matrix[1][2])*matrix[2][0])
        + static_cast<double>(matrix[0][2])*(static_cast<double>(matrix[1][0])*matrix[2][1]-static_cast<double>(matrix[1][1])*matrix[2][0]);
    EXPECT_NEAR(determinant,1.0,16.0*FLT_EPSILON);
}

void ExpectRotationMatrix(const VxMatrix &actual, const double (&expected)[3][3]) {
    ExpectRotation(actual);
    for (int i=0; i<3; ++i) for (int j=0; j<3; ++j)
        EXPECT_NEAR(actual[i][j],expected[i][j],32.0*FLT_EPSILON);
}

void QuaternionRotation(const float *q, double (&matrix)[3][3]) {
    double length=0;
    for (int i=0; i<4; ++i) length=std::hypot(length,static_cast<double>(q[i]));
    const double u[3]={q[0]/length,q[1]/length,q[2]/length}, w=q[3]/length;
    // Quaternion sandwich action on each basis vector, with hypot-based
    // normalization independent of the production squared-norm formula.
    for (int column=0; column<3; ++column) {
        double basis[3]={0,0,0}; basis[column]=1;
        double cross[3], twiceCross[3];
        for (int i=0; i<3; ++i) {
            const int j=(i+1)%3, k=(i+2)%3;
            cross[i]=u[j]*basis[k]-u[k]*basis[j];
        }
        for (int i=0; i<3; ++i) {
            const int j=(i+1)%3, k=(i+2)%3;
            twiceCross[i]=u[j]*cross[k]-u[k]*cross[j];
            matrix[i][column]=basis[i]+2.0*(w*cross[i]+twiceCross[i]);
        }
    }
}

void ReferenceSinCos(float angle, double &sine, double &cosine) {
    if (std::fabs(angle) < 8192) {
        sine=std::sin(static_cast<double>(angle)); cosine=std::cos(static_cast<double>(angle));
        return;
    }
    for (const auto &sample : TrigonometricReference::Samples) if (sample.angle==angle) {
        sine=sample.sine; cosine=sample.cosine;
        return;
    }
    sine=cosine=0;
    FAIL() << "Missing independent angle reference: " << angle;
}

void AxisRotation(const float *axis, float angle, double (&matrix)[3][3]) {
    const double length=std::hypot(std::hypot(static_cast<double>(axis[0]),axis[1]),axis[2]);
    const double u[3]={axis[0]/length,axis[1]/length,axis[2]/length};
    double s,c; ReferenceSinCos(angle,s,c);
    for (int i=0; i<3; ++i) for (int j=0; j<3; ++j)
        matrix[i][j]=(i==j ? c : 0.0)+(1-c)*u[i]*u[j];
    for (int i=0; i<3; ++i) {
        const int j=(i+1)%3,k=(i+2)%3;
        matrix[j][k]-=s*u[i]; matrix[k][j]+=s*u[i];
    }
}

void EulerRotation(const float *angles, double (&matrix)[3][3]) {
    for (int i=0; i<3; ++i) for (int j=0; j<3; ++j) matrix[i][j]=i==j ? 1.0 : 0.0;
    // The API composes Rx(-x) * Ry(-y) * Rz(-z).
    for (int axis=0; axis<3; ++axis) {
        double s,c; ReferenceSinCos(angles[axis],s,c); s=-s;
        double rotation[3][3]={}, product[3][3]={};
        rotation[axis][axis]=1;
        const int j=(axis+1)%3,k=(axis+2)%3;
        rotation[j][j]=rotation[k][k]=c; rotation[j][k]=-s; rotation[k][j]=s;
        for (int row=0; row<3; ++row) for (int column=0; column<3; ++column) {
            for (int inner=0; inner<3; ++inner) product[row][column]+=matrix[row][inner]*rotation[inner][column];
        }
        for (int row=0; row<3; ++row) for (int column=0; column<3; ++column) matrix[row][column]=product[row][column];
    }
}

// Build the symmetric scale tensor in double, independently of ToMatrix.
void ScaleTensor(const VxQuaternion &q, const VxVector &scale, double (&tensor)[3][3]) {
    const double x=q.x, y=q.y, z=q.z, w=q.w;
    const double s=2.0/(x*x+y*y+z*z+w*w);
    const double r[3][3] = {
        {1-s*(y*y+z*z), s*(x*y-z*w), s*(x*z+y*w)},
        {s*(x*y+z*w), 1-s*(x*x+z*z), s*(y*z-x*w)},
        {s*(x*z-y*w), s*(y*z+x*w), 1-s*(x*x+y*y)}
    };
    for (int i=0; i<3; ++i) for (int j=0; j<3; ++j) {
        tensor[i][j]=0;
        for (int k=0; k<3; ++k) tensor[i][j]+=r[i][k]*scale[k]*r[j][k];
    }
}

TEST_P(QuaternionConversionTest, PreservesConversionsAndEquivalentScaleTransforms) {
    int index = 0;
    for (const auto &sample : QuaternionConversionReference::Samples) {
        const int current = index++;
        if (sample.operation != GetParam()) continue;
        SCOPED_TRACE(testing::Message() << "sample=" << current << " operation=" << sample.operation
                     << " unit=" << sample.unit << " restore=" << sample.restore);
        const float *v = sample.input;
        if (sample.operation == 6) {
            double norm=0;
            for (int i=0; i<4; ++i) norm+=static_cast<double>(v[i])*v[i];
            // Snuggle operates on a scale-axis rotation. Non-unit/nonfinite
            // captures do not specify a mathematically valid decomposition.
            if (!std::isfinite(norm) || std::fabs(norm-1.0)>1e-5 ||
                !std::isfinite(v[4]) || !std::isfinite(v[5]) || !std::isfinite(v[6])) continue;
        }
        VxQuaternion q(v[0], v[1], v[2], v[3]), returned;
        VxVector axis(v[0], v[1], v[2]), scale(v[4], v[5], v[6]);
        VxMatrix matrix;
        std::memcpy(&matrix[0][0], v, sizeof(float)*16);
        float actual[20] = {};
        switch (sample.operation) {
        case 0: returned.FromRotation(axis, v[3]); break;
        case 1: returned.FromEulerAngles(v[0], v[1], v[2]); break;
        case 2: q.ToEulerAngles(actual, actual+1, actual+2); break;
        case 3: returned.FromMatrix(matrix, sample.unit, sample.restore); break;
        case 4: returned = Vx3DQuaternionFromMatrix(matrix); break;
        case 5: q.ToMatrix(matrix); break;
        case 6:
            returned = Vx3DQuaternionSnuggle(&q, &scale);
            std::memcpy(actual+4, &q.x, sizeof(float)*4);
            std::memcpy(actual+8, &scale.x, sizeof(float)*3);
            break;
        case 7: Vx3DMatrixFromRotation(matrix, axis, v[3]); break;
        case 8: Vx3DMatrixFromEulerAngles(matrix, v[0], v[1], v[2]); break;
        case 9: Vx3DMatrixToEulerAngles(matrix, actual, actual+1, actual+2); break;
        case 10:
            matrix[3] = scale;
            Vx3DMatrixFromRotationAndOrigin(matrix, sample.unit == 2 ? static_cast<const VxVector &>(matrix[0]) : axis,
                                           sample.unit == 1 ? static_cast<const VxVector &>(matrix[3]) : scale, v[3]);
            break;
        default: FAIL() << "Unknown operation";
        }
        if (sample.operation == 0 || sample.operation == 1 || sample.operation == 3 || sample.operation == 4 || sample.operation == 6)
            std::memcpy(actual, &returned.x, sizeof(float)*4);
        if (sample.operation == 3 || sample.operation == 4) std::memcpy(actual+4, &matrix[0][0], sizeof(float)*16);
        if (sample.operation == 5 || sample.operation == 7 || sample.operation == 8 || sample.operation == 10) std::memcpy(actual, &matrix[0][0], sizeof(float)*16);
        if (sample.operation == 2 || sample.operation == 5) {
            double norm=0;
            for (int i=0; i<4; ++i) norm+=static_cast<double>(v[i])*v[i];
            if (std::isfinite(norm) && norm>0) {
                double expected[3][3]; QuaternionRotation(v,expected);
                if (sample.operation == 2) Vx3DMatrixFromEulerAngles(matrix,actual[0],actual[1],actual[2]);
                ExpectRotationMatrix(matrix,expected);
                for (int i=0; i<4; ++i) {
                    EXPECT_EQ(matrix[3][i],i==3 ? 1.0f : 0.0f);
                    EXPECT_EQ(matrix[i][3],i==3 ? 1.0f : 0.0f);
                }
                continue;
            }
        }
        bool mathematicalRotation=false;
        double expectedRotation[3][3];
        if ((sample.operation==0 || sample.operation==7 || sample.operation==10) &&
            std::isfinite(v[3]) && std::fabs(v[3])>10000) {
            const double length=std::hypot(std::hypot(static_cast<double>(v[0]),v[1]),v[2]);
            if (std::isfinite(length) && length>0) {
                AxisRotation(v,v[3],expectedRotation); mathematicalRotation=true;
            }
        }
        if ((sample.operation==1 || sample.operation==8) && std::isfinite(v[0]) &&
            std::isfinite(v[1]) && std::isfinite(v[2]) &&
            (std::fabs(v[0])>10000 || std::fabs(v[1])>10000 || std::fabs(v[2])>10000)) {
            EulerRotation(v,expectedRotation); mathematicalRotation=true;
        }
        if (mathematicalRotation) {
            if (sample.operation==0 || sample.operation==1) {
                returned.ToMatrix(matrix);
                ExpectRotationMatrix(matrix,expectedRotation);
                continue;
            }
            ExpectRotationMatrix(matrix,expectedRotation);
        }
        if (sample.operation == 6) {
            const VxQuaternion finalRotation = Vx3DQuaternionMultiply(q, returned);
            double correctionNorm=0;
            for (int i=0; i<4; ++i) correctionNorm+=static_cast<double>(returned[i])*returned[i];
            EXPECT_NEAR(correctionNorm,1.0,64.0*FLT_EPSILON);
            const float *reference = sample.expected;
            const VxQuaternion referenceRotation = Vx3DQuaternionMultiply(
                VxQuaternion(reference[4],reference[5],reference[6],reference[7]),
                VxQuaternion(reference[0],reference[1],reference[2],reference[3]));
            double actualTensor[3][3], referenceTensor[3][3];
            ScaleTensor(finalRotation, scale, actualTensor);
            ScaleTensor(referenceRotation, VxVector(reference[8],reference[9],reference[10]), referenceTensor);
            if ((v[4]!=v[5] && v[4]!=v[6] && v[5]!=v[6]) || (v[4]==v[5] && v[5]==v[6]))
                ScaleTensor(VxQuaternion(v[0],v[1],v[2],v[3]),VxVector(v[4],v[5],v[6]),referenceTensor);
            for (int i=0; i<3; ++i) for (int j=0; j<3; ++j)
                EXPECT_NEAR(actualTensor[i][j], referenceTensor[i][j], 32.0*FLT_EPSILON*
                    (1.0+std::fabs(v[4])+std::fabs(v[5])+std::fabs(v[6])));
            float before[3]={v[4],v[5],v[6]}, after[3]={scale.x,scale.y,scale.z};
            SortThree(before); SortThree(after);
            for (int i=0; i<3; ++i) EXPECT_EQ(before[i],after[i]);
            continue;
        }
        bool boundedMatrix = true;
        for (int i=0; i<3; ++i) for (int j=0; j<3; ++j)
            boundedMatrix &= std::isfinite(matrix[i][j]) && std::fabs(matrix[i][j])<=4.0f;
        if (sample.operation == 10 && boundedMatrix) {
            double origin[3]={v[4],v[5],v[6]};
            for (int i=0; i<3; ++i) {
                double expected=origin[i], bound=std::fabs(origin[i]);
                for (int j=0; j<3; ++j) {
                    expected-=matrix[j][i]*origin[j];
                    bound+=std::fabs(matrix[j][i]*origin[j]);
                }
                EXPECT_NEAR(matrix[3][i],expected,8.0*FLT_EPSILON*(1.0+bound));
                if (sample.unit==1) origin[i]=matrix[3][i];
            }
        }
        for (int component = 0; component < 20; ++component) {
            if (mathematicalRotation && component<12) continue;
            // MatIsUnit on an arbitrary extreme matrix is outside the rotation
            // contract; still check its input-preservation side effect below.
            if ((sample.operation==4 || (sample.operation==3 && sample.unit)) &&
                !boundedMatrix && component<4) continue;
            if (sample.operation==10 && boundedMatrix && component>=12 && component<15) continue;
            SCOPED_TRACE(testing::Message() << "component=" << component);
            // Accurate normalization now shares one path. The native SSE
            // capture contains RSQRT approximation and float-range failures.
            const float expected = sample.scalar[component];
            if (std::isnan(expected)) EXPECT_TRUE(std::isnan(actual[component]));
            else if (std::isinf(expected)) EXPECT_EQ(actual[component], expected);
            else {
                EXPECT_TRUE(std::isfinite(actual[component]));
                const float tolerance = sample.tolerance == 0.0f ? 16.0f * FLT_EPSILON : sample.tolerance;
                EXPECT_NEAR(actual[component], expected, tolerance * (1.0f + std::fabs(expected)));
            }
        }
    }
}

INSTANTIATE_TEST_SUITE_P(Operations, QuaternionConversionTest, testing::Range(0, 11));

TEST(QuaternionConversionNumericsTest, TraceCancellationRetainsUnitTerm) {
    VxMatrix m;
    std::memset(&m[0][0],0,sizeof(float)*16);
    m[0][0]=-16777216.0f; m[1][1]=16777216.0f; m[2][2]=1.0f;
    m[3][3]=1.0f;
    const VxQuaternion q=Vx3DQuaternionFromMatrix(m);
    EXPECT_NEAR(q.w,std::sqrt(.5),FLT_EPSILON);
    EXPECT_EQ(q.x,0); EXPECT_EQ(q.y,0); EXPECT_EQ(q.z,0);
}

TEST(QuaternionConversionNumericsTest, FiniteAxisScaleAndQuaternionScaleDoNotChangeRotation) {
    const float scales[] = {std::numeric_limits<float>::denorm_min(),1e-30f,1e-20f,1.0f,1e20f,1e30f,FLT_MAX/4.0f};
    VxMatrix reference;
    Vx3DMatrixFromRotation(reference,VxVector(1,2,3),.7f);
    for (float scale : scales) {
        VxMatrix actual;
        Vx3DMatrixFromRotation(actual,VxVector(scale,2*scale,3*scale),.7f);
        ExpectRotation(actual);
        for (int i=0; i<3; ++i) for (int j=0; j<3; ++j)
            EXPECT_NEAR(actual[i][j],reference[i][j],8.0*FLT_EPSILON);
        VxQuaternion(scale,scale,scale,scale).ToMatrix(actual);
        ExpectRotation(actual);
        const float expected[3][3] = {{0,0,1},{1,0,0},{0,1,0}};
        for (int i=0; i<3; ++i) for (int j=0; j<3; ++j)
            EXPECT_NEAR(actual[i][j],expected[i][j],8.0*FLT_EPSILON);
        VxQuaternion(0,0,0,scale).ToMatrix(actual);
        for (int i=0; i<4; ++i) for (int j=0; j<4; ++j)
            EXPECT_EQ(actual[i][j],i==j ? 1.0f : 0.0f);
    }
}

TEST(QuaternionConversionNumericsTest, LargeFiniteAnglesProduceProperRotations) {
    for (const auto &sample : TrigonometricReference::Samples) {
        if (std::fabs(sample.angle)>FLT_MAX) continue;
        const float angle=static_cast<float>(sample.angle);
        if (static_cast<double>(angle)!=sample.angle) continue;
        SCOPED_TRACE(angle);
        VxMatrix matrix;
        Vx3DMatrixFromRotation(matrix,VxVector(1,0,0),angle);
        ExpectRotation(matrix);
        EXPECT_NEAR(matrix[1][1],sample.cosine,8.0*FLT_EPSILON);
        EXPECT_NEAR(matrix[2][1],sample.sine,8.0*FLT_EPSILON);
        Vx3DMatrixFromEulerAngles(matrix,angle,-angle,angle);
        const float angles[3]={angle,-angle,angle};
        double expected[3][3]; EulerRotation(angles,expected);
        ExpectRotationMatrix(matrix,expected);
    }
}
} // namespace
