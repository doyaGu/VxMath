#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include "VxMath.h"
#include "MatrixDecompositionSamples.h"
#define VX_POLAR_PROBE 1
#include "MatrixPolarReferenceTest.cpp"

class MatrixDecompositionTest : public testing::TestWithParam<int> {};

TEST_P(MatrixDecompositionTest, MatchesOriginalComponents) {
    int index = 0;
    for (const auto &sample : MatrixDecompositionReference::Samples) {
        const int current = index++;
        if (sample.operation != GetParam()) continue;
        SCOPED_TRACE(testing::Message() << "sample=" << current << " op=" << sample.operation << " mask=" << sample.mask);
        VxMatrix inputA, inputB, a, b;
        std::memcpy(&inputA[0][0], sample.a, 64);
        std::memcpy(&inputB[0][0], sample.b, 64);
        VxQuaternion q(31, 32, 33, 34), u(41, 42, 43, 44);
        VxVector pos(51, 52, 53), scale(61, 62, 63);
        float actual[36] = {};
        switch (sample.operation) {
        case 0:
            actual[0] = Vx3DMatrixPolarDecomposition(inputA, a, b);
            std::memcpy(actual+1, &a[0][0], 64); std::memcpy(actual+17, &b[0][0], 64); break;
        case 1:
            scale = Vx3DMatrixSpectralDecomposition(inputA, a);
            std::memcpy(actual, &scale.x, 12); std::memcpy(actual+3, &a[0][0], 64); break;
        case 2:
            Vx3DDecomposeMatrix(inputA, q, pos, scale);
            std::memcpy(actual, &q.x, 16); std::memcpy(actual+4, &pos.x, 12); std::memcpy(actual+7, &scale.x, 12); break;
        case 3: actual[0] = Vx3DDecomposeMatrixTotal(inputA, q, pos, scale, u); break;
        case 4:
            actual[0] = Vx3DDecomposeMatrixTotalPtr(inputA, sample.mask & 1 ? &q : nullptr, sample.mask & 2 ? &pos : nullptr,
                                                  sample.mask & 4 ? &scale : nullptr, sample.mask & 8 ? &u : nullptr); break;
        case 5: Vx3DInterpolateMatrix(sample.step, a, inputA, inputB); break;
        case 6: Vx3DInterpolateMatrixNoScale(sample.step, a, inputA, inputB); break;
        }
        if (sample.operation == 3 || sample.operation == 4) {
            std::memcpy(actual+1, &q.x, 16); std::memcpy(actual+5, &pos.x, 12);
            std::memcpy(actual+8, &scale.x, 12); std::memcpy(actual+11, &u.x, 16);
        }
        if (sample.operation == 5 || sample.operation == 6) std::memcpy(actual, &a[0][0], 64);
        const bool singular = VxMatrixInverseDeterminant(inputA) == 0.0;
        if (sample.operation == 0 && singular) {
            MPMatrix original, orthogonal, stretch;
            MPFrom(inputA, original); MPFrom(a, orthogonal); MPFrom(b, stretch);
            EXPECT_EQ(actual[0], 0);
            EXPECT_TRUE(MPFactors(original, orthogonal, stretch));
            EXPECT_TRUE(MPAffine(a)); EXPECT_TRUE(MPAffine(b));
            continue;
        }
        const bool singularTotal = singular && (sample.operation == 3 || sample.operation == 4);
        if (singularTotal) {
            // A null-space basis is not unique. Check the actual returned factors
            // geometrically, including optional outputs and their untouched sentinels.
            VxQuaternion fullQ, fullU; VxVector fullPos, fullScale;
            if (sample.operation == 4) Vx3DDecomposeMatrixTotal(inputA, fullQ, fullPos, fullScale, fullU);
            if (sample.operation == 3 || (sample.mask & 1)) fullQ = q;
            if (sample.operation == 3 || (sample.mask & 2)) fullPos = pos;
            if (sample.operation == 3 || (sample.mask & 4)) fullScale = scale;
            if (sample.operation == 3 || (sample.mask & 8)) fullU = u;
            VxMatrix rotation, stretchRotation;
            fullQ.ToMatrix(rotation); fullU.ToMatrix(stretchRotation);
            MPMatrix original, orthogonal, stretch = {};
            MPFrom(inputA, original); MPFrom(rotation, orthogonal);
            for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) {
                orthogonal[i][j] *= actual[0];
                for (int k = 0; k < 3; ++k) stretch[i][j] += double(stretchRotation[i][k])*fullScale[k]*stretchRotation[j][k];
            }
            EXPECT_TRUE(MPFactors(original, orthogonal, stretch));
            EXPECT_FLOAT_EQ(fullPos.x, inputA[3][0]); EXPECT_FLOAT_EQ(fullPos.y, inputA[3][1]); EXPECT_FLOAT_EQ(fullPos.z, inputA[3][2]);
        }
        // Intentional difference: with exactly two equal scales the original
        // composes the stretch rotation with Snuggle's rewritten input. Where
        // URot differs from the capture it must rebuild the polar stretch.
        bool correctedURot = false;
        if (sample.operation == 3 || (sample.operation == 4 && (sample.mask & 8))) {
            for (int i = 11; i < 15; ++i)
                correctedURot |= std::fabs(actual[i] - sample.scalar[i]) > 2e-5f*(1.0f+std::fabs(sample.scalar[i]));
        }
        if (correctedURot && !singularTotal) {
            VxQuaternion fullQ, fullU; VxVector fullPos, k;
            Vx3DDecomposeMatrixTotal(inputA, fullQ, fullPos, k, fullU);
            EXPECT_TRUE(k.x == k.y || k.x == k.z || k.y == k.z) << "URot differs without two equal scales";
            VxMatrix polarQ, stretch, rotation;
            Vx3DMatrixPolarDecomposition(inputA, polarQ, stretch);
            fullU.ToMatrix(rotation);
            for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) {
                float rebuilt = 0.0f;
                for (int m = 0; m < 3; ++m) rebuilt += rotation[i][m] * k[m] * rotation[j][m];
                EXPECT_NEAR(rebuilt, stretch[i][j], 2e-5f*(1.0f+std::fabs(stretch[i][j])));
            }
        }
        for (int i = 0; i < 36; ++i) {
            if (singularTotal && ((i >= 1 && i < 5 && (sample.operation == 3 || (sample.mask & 1))) ||
                                 (i >= 8 && i < 11 && (sample.operation == 3 || (sample.mask & 4))) ||
                                 (i >= 11 && i < 15 && (sample.operation == 3 || (sample.mask & 8))))) continue;
            if (correctedURot && i >= 11 && i < 15) continue;
            // Exact endpoints preserve the selected matrix, including shear;
            // do not reproduce NaNs from decomposing the unused endpoint.
            const bool endpoint = (sample.operation==5 || sample.operation==6) &&
                (sample.step==0 || sample.step==1) && i<16;
            const float expected = endpoint ? (sample.step==0 ? sample.a[i] : sample.b[i]) : sample.scalar[i];
            SCOPED_TRACE(testing::Message() << "component=" << i);
            if (std::isnan(expected)) EXPECT_TRUE(std::isnan(actual[i]));
            else if (std::isinf(expected)) EXPECT_EQ(actual[i], expected);
            else EXPECT_NEAR(actual[i], expected, 2e-5f*(1.0f+std::fabs(expected)));
        }
    }
}
INSTANTIATE_TEST_SUITE_P(Operations, MatrixDecompositionTest, testing::Range(0, 7));
