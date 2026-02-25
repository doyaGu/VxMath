#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <random>

#include "VxMath.h"

namespace {

VxVector MultiplyVec3Scalar(const VxMatrix &m, const VxVector &v) {
    return VxVector(
        v.x * m[0][0] + v.y * m[1][0] + v.z * m[2][0] + m[3][0],
        v.x * m[0][1] + v.y * m[1][1] + v.z * m[2][1] + m[3][1],
        v.x * m[0][2] + v.y * m[1][2] + v.z * m[2][2] + m[3][2]
    );
}

VxVector RotateVec3Scalar(const VxMatrix &m, const VxVector &v) {
    return VxVector(
        v.x * m[0][0] + v.y * m[1][0] + v.z * m[2][0],
        v.x * m[0][1] + v.y * m[1][1] + v.z * m[2][1],
        v.x * m[0][2] + v.y * m[1][2] + v.z * m[2][2]
    );
}

VxQuaternion MultiplyQuaternionScalar(const VxQuaternion &left, const VxQuaternion &right) {
    return VxQuaternion(
        left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
        left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
        left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w,
        left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z
    );
}

VxQuaternion NormalizeQuaternionScalar(VxQuaternion q) {
    const float norm = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (norm == 0.0f) {
        return VxQuaternion(0.0f, 0.0f, 0.0f, 1.0f);
    }

    const float invNorm = 1.0f / norm;
    return VxQuaternion(q.x * invNorm, q.y * invNorm, q.z * invNorm, q.w * invNorm);
}

VxQuaternion SlerpQuaternionScalar(float t, const VxQuaternion &a, const VxQuaternion &b) {
    float cosOmega = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    float k0;
    float k1;

    if (cosOmega >= 0.0f) {
        const float oneMinusCos = 1.0f - cosOmega;
        if (oneMinusCos < 0.01f) {
            k0 = 1.0f - t;
            k1 = t;
        } else {
            const float omega = std::acos(cosOmega);
            const float invSinOmega = 1.0f / std::sin(omega);
            k0 = std::sin((1.0f - t) * omega) * invSinOmega;
            k1 = std::sin(t * omega) * invSinOmega;
        }
    } else {
        const float oneMinusCosNeg = 1.0f - (-cosOmega);
        if (oneMinusCosNeg < 0.01f) {
            k0 = 1.0f - t;
            k1 = -t;
        } else {
            const float omega = std::acos(-cosOmega);
            const float invSinOmega = 1.0f / std::sin(omega);
            k0 = std::sin((1.0f - t) * omega) * invSinOmega;
            k1 = -std::sin(t * omega) * invSinOmega;
        }
    }

    return VxQuaternion(
        k0 * a.x + k1 * b.x,
        k0 * a.y + k1 * b.y,
        k0 * a.z + k1 * b.z,
        k0 * a.w + k1 * b.w
    );
}

float RandomRange(std::mt19937 &rng, float lo, float hi) {
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(rng);
}

VxVector RandomVector3(std::mt19937 &rng, float lo, float hi) {
    return VxVector(RandomRange(rng, lo, hi), RandomRange(rng, lo, hi), RandomRange(rng, lo, hi));
}

VxMatrix MakeAffineMatrix(float tx, float ty, float tz, float rx, float ry, float rz, float sx, float sy, float sz) {
    VxMatrix m;
    Vx3DMatrixFromEulerAngles(m, rx, ry, rz);
    m[0][0] *= sx; m[0][1] *= sx; m[0][2] *= sx;
    m[1][0] *= sy; m[1][1] *= sy; m[1][2] *= sy;
    m[2][0] *= sz; m[2][1] *= sz; m[2][2] *= sz;
    m[3][0] = tx;
    m[3][1] = ty;
    m[3][2] = tz;
    m[3][3] = 1.0f;
    return m;
}

float ScalarRaySquareDistance(const VxRay &ray, const VxVector &point) {
    const VxVector v = point - ray.GetOrigin();
    const float aa = v.SquareMagnitude();
    const float dd = ray.GetDirection().SquareMagnitude();
    if (dd <= EPSILON) {
        return aa;
    }
    const float ps = DotProduct(v, ray.GetDirection());
    const float sq = aa - (ps * ps) / dd;
    return (sq > 0.0f) ? sq : 0.0f;
}

float ScalarRayDistance(const VxRay &ray, const VxVector &point) {
    return std::sqrt(ScalarRaySquareDistance(ray, point));
}

void ScalarRayInterpolate(VxVector &point, const VxRay &ray, float t) {
    point = ray.GetOrigin() + ray.GetDirection() * t;
}

void ScalarRayTransform(VxRay &dest, const VxRay &ray, const VxMatrix &mat) {
    Vx3DMultiplyMatrixVector(&dest.m_Origin, mat, &ray.m_Origin);
    Vx3DRotateVector(&dest.m_Direction, mat, &ray.m_Direction);
}

void ScalarPlaneCreateFromPoint(VxPlane &plane, const VxVector &normal, const VxVector &point) {
    VxVector n = normal;
    const float magSq = n.SquareMagnitude();
    if (magSq > EPSILON) {
        n *= (1.0f / std::sqrt(magSq));
    }
    plane.m_Normal = n;
    plane.m_D = -DotProduct(n, point);
}

void ScalarPlaneCreateFromTriangle(VxPlane &plane, const VxVector &a, const VxVector &b, const VxVector &c) {
    VxVector edge1 = b - a;
    VxVector edge2 = c - a;
    VxVector n = CrossProduct(edge1, edge2);
    const float magSq = n.SquareMagnitude();
    if (magSq > EPSILON) {
        n *= (1.0f / std::sqrt(magSq));
    } else {
        n = VxVector(0.0f, 0.0f, 1.0f);
    }
    ScalarPlaneCreateFromPoint(plane, n, a);
}

float ScalarPlaneClassifyPoint(const VxPlane &plane, const VxVector &point) {
    return DotProduct(plane.m_Normal, point) + plane.m_D;
}

float ScalarPlaneDistance(const VxPlane &plane, const VxVector &point) {
    return std::fabs(ScalarPlaneClassifyPoint(plane, point));
}

VxVector ScalarPlaneNearestPoint(const VxPlane &plane, const VxVector &point) {
    return point - plane.m_Normal * ScalarPlaneClassifyPoint(plane, point);
}

float ScalarPlaneXClassify(const VxPlane &plane, const VxVector boxAxis[4]) {
    const float radius =
        std::fabs(DotProduct(plane.m_Normal, boxAxis[0])) +
        std::fabs(DotProduct(plane.m_Normal, boxAxis[1])) +
        std::fabs(DotProduct(plane.m_Normal, boxAxis[2]));
    const float centerDist = DotProduct(plane.m_Normal, boxAxis[3]) + plane.m_D;
    if (centerDist > radius) {
        return centerDist - radius;
    }
    if (centerDist < -radius) {
        return centerDist + radius;
    }
    return 0.0f;
}

void ScalarFrustumComputeVertices(const VxFrustum &frustum, VxVector *vertices) {
    const VxVector nearDirVec = frustum.GetDir() * frustum.GetDMin();
    const VxVector rightVec = frustum.GetRight() * frustum.GetRBound();
    const VxVector upVec = frustum.GetUp() * frustum.GetUBound();

    const VxVector leftVec = nearDirVec - rightVec;
    const VxVector rightVec2 = nearDirVec + rightVec;

    vertices[0] = leftVec - upVec;
    vertices[1] = leftVec + upVec;
    vertices[2] = rightVec2 + upVec;
    vertices[3] = rightVec2 - upVec;

    for (int i = 0; i < 4; ++i) {
        const VxVector nearVec = vertices[i];
        vertices[i + 4] = frustum.GetOrigin() + nearVec * frustum.GetDRatio();
        vertices[i] = vertices[i] + frustum.GetOrigin();
    }
}

void ScalarFrustumTransform(VxFrustum &frustum, const VxMatrix &invWorldMat) {
    VxVector rightScaled = frustum.GetRight() * frustum.GetRBound();
    VxVector upScaled = frustum.GetUp() * frustum.GetUBound();
    VxVector dirScaled = frustum.GetDir() * frustum.GetDMin();

    VxVector newOrigin;
    Vx3DMultiplyMatrixVector(&newOrigin, invWorldMat, &frustum.GetOrigin());
    frustum.GetOrigin() = newOrigin;

    VxVector inVectors[3];
    inVectors[0] = rightScaled;
    inVectors[1] = upScaled;
    inVectors[2] = dirScaled;

    VxVector outVectors[3];
    Vx3DRotateVectorMany(outVectors, invWorldMat, inVectors, 3, sizeof(VxVector));

    const float newRBound = Magnitude(outVectors[0]);
    const float newUBound = Magnitude(outVectors[1]);
    const float newDMin = Magnitude(outVectors[2]);

    frustum.GetRBound() = newRBound;
    frustum.GetUBound() = newUBound;
    frustum.GetDMin() = newDMin;
    frustum.GetDMax() = newDMin * frustum.GetDRatio();

    if (newRBound > 0.0f) {
        outVectors[0] /= newRBound;
    } else {
        outVectors[0] = VxVector::axis0();
    }
    if (newUBound > 0.0f) {
        outVectors[1] /= newUBound;
    } else {
        outVectors[1] = VxVector::axis0();
    }
    if (newDMin > 0.0f) {
        outVectors[2] /= newDMin;
    } else {
        outVectors[2] = VxVector::axis0();
    }

    frustum.GetRight() = outVectors[0];
    frustum.GetUp() = outVectors[1];
    frustum.GetDir() = outVectors[2];
    frustum.Update();
}

XBOOL ScalarTransformBox2DReference(const VxMatrix &worldProjection, const VxBbox &box, const VxRect *screenSize, VxRect *extents, VXCLIP_FLAGS &orClipFlags, VXCLIP_FLAGS &andClipFlags) {
    if (!box.IsValid()) {
        orClipFlags = VXCLIP_ALL;
        andClipFlags = VXCLIP_ALL;
        return FALSE;
    }

    VxVector4 transformedVertices[8];
    Vx3DMultiplyMatrixVector4(&transformedVertices[0], worldProjection, &box.Min);

    const VxVector boxSize = box.Max - box.Min;
    const float dx = boxSize.x;
    const float dy = boxSize.y;
    const float dz = boxSize.z;

    const VxVector4 &matRow0 = worldProjection[0];
    const VxVector4 &matRow1 = worldProjection[1];
    const VxVector4 &matRow2 = worldProjection[2];
    const VxVector4 deltaX(matRow0[0] * dx, matRow0[1] * dx, matRow0[2] * dx, matRow0[3] * dx);
    const VxVector4 deltaY(matRow1[0] * dy, matRow1[1] * dy, matRow1[2] * dy, matRow1[3] * dy);

    int vertexCount;
    if (dz == 0.0f) {
        vertexCount = 4;
        transformedVertices[1] = transformedVertices[0] + deltaX;
        transformedVertices[2] = transformedVertices[0] + deltaY;
        transformedVertices[3] = transformedVertices[1] + deltaY;
    } else {
        vertexCount = 8;
        const VxVector4 deltaZ(matRow2[0] * dz, matRow2[1] * dz, matRow2[2] * dz, matRow2[3] * dz);
        transformedVertices[1] = transformedVertices[0] + deltaZ;
        transformedVertices[2] = transformedVertices[0] + deltaY;
        transformedVertices[3] = transformedVertices[1] + deltaY;
        transformedVertices[4] = transformedVertices[0] + deltaX;
        transformedVertices[5] = transformedVertices[4] + deltaZ;
        transformedVertices[6] = transformedVertices[4] + deltaY;
        transformedVertices[7] = transformedVertices[5] + deltaY;
    }

    XDWORD allVerticesFlags = 0;
    XDWORD allVerticesFlagsAnded = 0xFFFFFFFFu;
    for (int i = 0; i < vertexCount; ++i) {
        XDWORD vertexFlags = 0;
        const VxVector4 &vertex = transformedVertices[i];
        if (-vertex.w > vertex.x) vertexFlags |= VXCLIP_LEFT;
        if (vertex.x > vertex.w) vertexFlags |= VXCLIP_RIGHT;
        if (-vertex.w > vertex.y) vertexFlags |= VXCLIP_BOTTOM;
        if (vertex.y > vertex.w) vertexFlags |= VXCLIP_TOP;
        if (vertex.z < 0.0f) vertexFlags |= VXCLIP_FRONT;
        if (vertex.z > vertex.w) vertexFlags |= VXCLIP_BACK;
        allVerticesFlags |= vertexFlags;
        allVerticesFlagsAnded &= vertexFlags;
    }

    if (extents && screenSize && !(allVerticesFlagsAnded & VXCLIP_ALL)) {
        float minX = 1.0e6f;
        float minY = 1.0e6f;
        float maxX = -1.0e6f;
        float maxY = -1.0e6f;
        bool hasProjected = false;

        const float halfWidth = (screenSize->right - screenSize->left) * 0.5f;
        const float halfHeight = (screenSize->bottom - screenSize->top) * 0.5f;
        const float centerX = halfWidth + screenSize->left;
        const float centerY = halfHeight + screenSize->top;

        for (int i = 0; i < vertexCount; ++i) {
            VxVector4 &vertex = transformedVertices[i];
            if (vertex.w <= 0.0f) {
                continue;
            }
            const float invW = 1.0f / vertex.w;
            const float screenX = vertex.x * invW * halfWidth + centerX;
            const float screenY = centerY - vertex.y * invW * halfHeight;
            if (screenX < minX) minX = screenX;
            if (screenY < minY) minY = screenY;
            if (screenX > maxX) maxX = screenX;
            if (screenY > maxY) maxY = screenY;
            hasProjected = true;
        }

        if (hasProjected) {
            extents->left = minX;
            extents->top = minY;
            extents->right = maxX;
            extents->bottom = maxY;
        }
    }

    if ((allVerticesFlags & VXCLIP_FRONT) && extents && screenSize) {
        if (allVerticesFlags & VXCLIP_LEFT) extents->left = screenSize->left;
        if (allVerticesFlags & VXCLIP_RIGHT) extents->right = screenSize->right;
        if (allVerticesFlags & VXCLIP_TOP) extents->top = screenSize->top;
        if (allVerticesFlags & VXCLIP_BOTTOM) extents->bottom = screenSize->bottom;
    }

    orClipFlags = static_cast<VXCLIP_FLAGS>(allVerticesFlags);
    andClipFlags = static_cast<VXCLIP_FLAGS>(allVerticesFlagsAnded);
    return !(allVerticesFlagsAnded & VXCLIP_ALL);
}

void ScalarRectTransform(VxRect &rect, const VxRect &destScreen, const VxRect &srcScreen) {
    const float srcInvW = 1.0f / (srcScreen.right - srcScreen.left);
    const float srcInvH = 1.0f / (srcScreen.bottom - srcScreen.top);
    const VxRect r((rect.left - srcScreen.left) * srcInvW,
                   (rect.top - srcScreen.top) * srcInvH,
                   (rect.right - srcScreen.right) * srcInvW,
                   (rect.bottom - srcScreen.bottom) * srcInvH);
    const float dstW = destScreen.right - destScreen.left;
    const float dstH = destScreen.bottom - destScreen.top;
    rect.left = r.left * dstW + destScreen.left;
    rect.top = r.top * dstH + destScreen.top;
    rect.right = r.right * dstW + destScreen.right;
    rect.bottom = r.bottom * dstH + destScreen.bottom;
}

void ScalarRectTransformBySize(VxRect &rect, const Vx2DVector &destScreenSize, const Vx2DVector &srcScreenSize) {
    const float invW = 1.0f / srcScreenSize.x;
    const float invH = 1.0f / srcScreenSize.y;
    rect.left *= invW * destScreenSize.x;
    rect.top *= invH * destScreenSize.y;
    rect.right *= invW * destScreenSize.x;
    rect.bottom *= invH * destScreenSize.y;
}

void ScalarRectTransformToHomogeneous(VxRect &rect, const VxRect &screen) {
    const float invW = 1.0f / (screen.right - screen.left);
    const float invH = 1.0f / (screen.bottom - screen.top);
    const float width = rect.right - rect.left;
    const float height = rect.bottom - rect.top;
    rect.left = (rect.left - screen.left) * invW;
    rect.top = (rect.top - screen.top) * invH;
    rect.right = rect.left + width * invW;
    rect.bottom = rect.top + height * invH;
}

void ScalarRectTransformFromHomogeneous(VxRect &rect, const VxRect &screen) {
    const float sW = screen.right - screen.left;
    const float sH = screen.bottom - screen.top;
    const float width = rect.right - rect.left;
    const float height = rect.bottom - rect.top;
    rect.left = screen.left + rect.left * sW;
    rect.top = screen.top + rect.top * sH;
    rect.right = rect.left + width * sW;
    rect.bottom = rect.top + height * sH;
}

void ScalarBboxTransformTo(const VxBbox &self, VxVector *points, const VxMatrix &mat) {
    Vx3DMultiplyMatrixVector(&points[0], mat, &self.Min);
    const float sizeX = self.Max.x - self.Min.x;
    const float sizeY = self.Max.y - self.Min.y;
    const float sizeZ = self.Max.z - self.Min.z;
    const VxVector xVec(sizeX * mat[0][0], sizeX * mat[0][1], sizeX * mat[0][2]);
    const VxVector yVec(sizeY * mat[1][0], sizeY * mat[1][1], sizeY * mat[1][2]);
    const VxVector zVec(sizeZ * mat[2][0], sizeZ * mat[2][1], sizeZ * mat[2][2]);
    points[1] = points[0] + zVec;
    points[2] = points[0] + yVec;
    points[3] = points[2] + zVec;
    points[4] = points[0] + xVec;
    points[5] = points[4] + zVec;
    points[6] = points[4] + yVec;
    points[7] = points[6] + zVec;
}

void ScalarBboxTransformFrom(VxBbox &dest, const VxBbox &src, const VxMatrix &mat) {
    VxVector center((src.Min.x + src.Max.x) * 0.5f,
                    (src.Min.y + src.Max.y) * 0.5f,
                    (src.Min.z + src.Max.z) * 0.5f);
    Vx3DMultiplyMatrixVector(&dest.Min, mat, &center);

    const float sizeX = src.Max.x - src.Min.x;
    const float sizeY = src.Max.y - src.Min.y;
    const float sizeZ = src.Max.z - src.Min.z;
    const VxVector xVec(sizeX * mat[0][0], sizeX * mat[0][1], sizeX * mat[0][2]);
    const VxVector yVec(sizeY * mat[1][0], sizeY * mat[1][1], sizeY * mat[1][2]);
    const VxVector zVec(sizeZ * mat[2][0], sizeZ * mat[2][1], sizeZ * mat[2][2]);
    const float halfX = (std::fabs(xVec.x) + std::fabs(yVec.x) + std::fabs(zVec.x)) * 0.5f;
    const float halfY = (std::fabs(xVec.y) + std::fabs(yVec.y) + std::fabs(zVec.y)) * 0.5f;
    const float halfZ = (std::fabs(xVec.z) + std::fabs(yVec.z) + std::fabs(zVec.z)) * 0.5f;

    dest.Max.x = dest.Min.x + halfX;
    dest.Max.y = dest.Min.y + halfY;
    dest.Max.z = dest.Min.z + halfZ;
    dest.Min.x = dest.Min.x - halfX;
    dest.Min.y = dest.Min.y - halfY;
    dest.Min.z = dest.Min.z - halfZ;
}

void ScalarProjectBoxZExtents(const VxMatrix &worldProjection, const VxBbox &box, float &zhMin, float &zhMax) {
    zhMin = 1.0e10f;
    zhMax = -1.0e10f;

    if (!box.IsValid()) {
        return;
    }

    VxVector4 corners[8];
    Vx3DMultiplyMatrixVector4(&corners[0], worldProjection, &box.Min);

    const float dx = box.Max.x - box.Min.x;
    const float dy = box.Max.y - box.Min.y;
    const float dz = box.Max.z - box.Min.z;

    const VxVector4 &matRow0 = worldProjection[0];
    const VxVector4 &matRow1 = worldProjection[1];
    const VxVector4 &matRow2 = worldProjection[2];
    const VxVector4 deltaX(matRow0[0] * dx, matRow0[1] * dx, matRow0[2] * dx, matRow0[3] * dx);
    const VxVector4 deltaY(matRow1[0] * dy, matRow1[1] * dy, matRow1[2] * dy, matRow1[3] * dy);
    const VxVector4 deltaZ(matRow2[0] * dz, matRow2[1] * dz, matRow2[2] * dz, matRow2[3] * dz);

    int vertexCount = 8;
    if (std::fabs(dz) < EPSILON) {
        vertexCount = 4;
        corners[1] = corners[0] + deltaX;
        corners[2] = corners[0] + deltaY;
        corners[3] = corners[1] + deltaY;
    } else {
        corners[1] = corners[0] + deltaZ;
        corners[2] = corners[0] + deltaY;
        corners[3] = corners[1] + deltaY;
        corners[4] = corners[0] + deltaX;
        corners[5] = corners[4] + deltaZ;
        corners[6] = corners[4] + deltaY;
        corners[7] = corners[5] + deltaY;
    }

    for (int i = 0; i < vertexCount; ++i) {
        const float w = corners[i].w;
        if (w <= EPSILON) {
            continue;
        }
        const float projZ = corners[i].z / w;
        if (projZ < zhMin) zhMin = projZ;
        if (projZ > zhMax) zhMax = projZ;
    }
    if (zhMin > zhMax) {
        zhMin = 0.0f;
        zhMax = 1.0f;
    }
}

TEST(SIMDDispatchTest, SIMDFeatureHierarchyIsValid) {
    const VxSIMDFeatures features = VxGetSIMDFeatures();

    if (features.AVX2) {
        EXPECT_TRUE(features.SSE2);
    }

    if (features.SSSE3) {
        EXPECT_TRUE(features.SSE2);
    }

    if (features.SSE2) {
        EXPECT_TRUE(features.SSE);
    }
}

TEST(SIMDDispatchTest, InterpolateFloatArrayMatchesScalarWithTail) {
    constexpr int kCount = 7;
    float a[kCount] = {0.0f, -1.0f, 2.0f, 3.5f, 1000.0f, -1.0e-4f, 42.0f};
    float b[kCount] = {1.0f, 3.0f, -2.0f, 7.5f, -1000.0f, 2.0e-4f, -42.0f};
    float result[kCount] = {};
    float expected[kCount] = {};
    const float t = 0.37f;

    for (int i = 0; i < kCount; ++i) {
        expected[i] = a[i] + (b[i] - a[i]) * t;
    }

    InterpolateFloatArray(result, a, b, t, kCount);

    for (int i = 0; i < kCount; ++i) {
        EXPECT_NEAR(result[i], expected[i], 1.0e-6f);
    }
}

TEST(SIMDDispatchTest, InterpolateVectorArrayMatchesScalarWithStride) {
    struct PackedVector {
        VxVector v;
        float padding;
    };

    constexpr int kCount = 5;
    PackedVector inA[kCount];
    PackedVector inB[kCount];
    PackedVector out[kCount];
    PackedVector expected[kCount];

    for (int i = 0; i < kCount; ++i) {
        inA[i].v = VxVector(static_cast<float>(i), static_cast<float>(2 * i), static_cast<float>(-i));
        inB[i].v = VxVector(static_cast<float>(i + 10), static_cast<float>(-i), static_cast<float>(i * 3));
        inA[i].padding = inB[i].padding = out[i].padding = expected[i].padding = -999.0f;
    }

    const float t = 0.625f;
    for (int i = 0; i < kCount; ++i) {
        expected[i].v.x = inA[i].v.x + (inB[i].v.x - inA[i].v.x) * t;
        expected[i].v.y = inA[i].v.y + (inB[i].v.y - inA[i].v.y) * t;
        expected[i].v.z = inA[i].v.z + (inB[i].v.z - inA[i].v.z) * t;
    }

    InterpolateVectorArray(
        out,
        inA,
        inB,
        t,
        kCount,
        static_cast<XDWORD>(sizeof(PackedVector)),
        static_cast<XDWORD>(sizeof(PackedVector))
    );

    for (int i = 0; i < kCount; ++i) {
        EXPECT_NEAR(out[i].v.x, expected[i].v.x, 1.0e-6f);
        EXPECT_NEAR(out[i].v.y, expected[i].v.y, 1.0e-6f);
        EXPECT_NEAR(out[i].v.z, expected[i].v.z, 1.0e-6f);
        EXPECT_FLOAT_EQ(out[i].padding, -999.0f);
    }
}

TEST(SIMDDispatchTest, MultiplyMatrixVectorManyMatchesScalarWithStrideAndTail) {
    struct PackedVector {
        VxVector v;
        float pad;
    };

    constexpr int kCount = 11;
    PackedVector input[kCount];
    PackedVector output[kCount];
    PackedVector expected[kCount];

    VxMatrix mat;
    mat.SetIdentity();
    mat[0][0] = 1.25f; mat[0][1] = -0.5f; mat[0][2] = 0.75f;
    mat[1][0] = 0.25f; mat[1][1] = 2.0f;  mat[1][2] = -1.0f;
    mat[2][0] = -0.8f; mat[2][1] = 0.6f;  mat[2][2] = 1.1f;
    mat[3][0] = 10.0f; mat[3][1] = -5.0f; mat[3][2] = 2.5f;

    for (int i = 0; i < kCount; ++i) {
        input[i].v = VxVector(static_cast<float>(i) * 0.5f, static_cast<float>(i - 3), static_cast<float>(2 * i + 1));
        input[i].pad = output[i].pad = expected[i].pad = -777.0f;
        expected[i].v = MultiplyVec3Scalar(mat, input[i].v);
    }

    Vx3DMultiplyMatrixVectorMany(
        &output[0].v,
        mat,
        &input[0].v,
        kCount,
        static_cast<int>(sizeof(PackedVector))
    );

    for (int i = 0; i < kCount; ++i) {
        EXPECT_NEAR(output[i].v.x, expected[i].v.x, 1.0e-6f);
        EXPECT_NEAR(output[i].v.y, expected[i].v.y, 1.0e-6f);
        EXPECT_NEAR(output[i].v.z, expected[i].v.z, 1.0e-6f);
        EXPECT_FLOAT_EQ(output[i].pad, -777.0f);
    }
}

TEST(SIMDDispatchTest, MultiplyMatrixVectorSingleMatchesScalarReference) {
    VxMatrix mat;
    mat.SetIdentity();
    mat[0][0] = 1.25f; mat[0][1] = -0.5f; mat[0][2] = 0.75f;
    mat[1][0] = 0.25f; mat[1][1] = 2.0f;  mat[1][2] = -1.0f;
    mat[2][0] = -0.8f; mat[2][1] = 0.6f;  mat[2][2] = 1.1f;
    mat[3][0] = 10.0f; mat[3][1] = -5.0f; mat[3][2] = 2.5f;

    const VxVector input(1.5f, -2.0f, 3.25f);
    const VxVector expected = MultiplyVec3Scalar(mat, input);

    VxVector output;
    Vx3DMultiplyMatrixVector(&output, mat, &input);
    EXPECT_NEAR(output.x, expected.x, 1.0e-6f);
    EXPECT_NEAR(output.y, expected.y, 1.0e-6f);
    EXPECT_NEAR(output.z, expected.z, 1.0e-6f);

    VxVector inPlace = input;
    Vx3DMultiplyMatrixVector(&inPlace, mat, &inPlace);
    EXPECT_NEAR(inPlace.x, expected.x, 1.0e-6f);
    EXPECT_NEAR(inPlace.y, expected.y, 1.0e-6f);
    EXPECT_NEAR(inPlace.z, expected.z, 1.0e-6f);
}

TEST(SIMDDispatchTest, RotateVectorManyInPlaceMatchesScalar) {
    constexpr int kCount = 13;
    VxVector input[kCount];
    VxVector expected[kCount];

    VxMatrix mat;
    mat.SetIdentity();
    mat[0][0] = 0.0f;  mat[0][1] = 1.0f;  mat[0][2] = 0.0f;
    mat[1][0] = -1.0f; mat[1][1] = 0.0f;  mat[1][2] = 0.0f;
    mat[2][0] = 0.0f;  mat[2][1] = 0.0f;  mat[2][2] = 1.0f;

    for (int i = 0; i < kCount; ++i) {
        input[i] = VxVector(static_cast<float>(i) * 0.25f, static_cast<float>(2 - i), static_cast<float>(i % 5 - 2));
        expected[i] = RotateVec3Scalar(mat, input[i]);
    }

    Vx3DRotateVectorMany(input, mat, input, kCount, static_cast<int>(sizeof(VxVector)));

    for (int i = 0; i < kCount; ++i) {
        EXPECT_NEAR(input[i].x, expected[i].x, 1.0e-6f);
        EXPECT_NEAR(input[i].y, expected[i].y, 1.0e-6f);
        EXPECT_NEAR(input[i].z, expected[i].z, 1.0e-6f);
    }
}

TEST(SIMDDispatchTest, RotateVectorSingleMatchesScalarReference) {
    VxMatrix mat;
    mat.SetIdentity();
    mat[0][0] = 0.0f;  mat[0][1] = 1.0f;  mat[0][2] = 0.0f;
    mat[1][0] = -1.0f; mat[1][1] = 0.0f;  mat[1][2] = 0.0f;
    mat[2][0] = 0.0f;  mat[2][1] = 0.0f;  mat[2][2] = 1.0f;

    const VxVector input(3.5f, -4.0f, 2.0f);
    const VxVector expected = RotateVec3Scalar(mat, input);

    VxVector output;
    Vx3DRotateVector(&output, mat, &input);
    EXPECT_NEAR(output.x, expected.x, 1.0e-6f);
    EXPECT_NEAR(output.y, expected.y, 1.0e-6f);
    EXPECT_NEAR(output.z, expected.z, 1.0e-6f);

    VxVector inPlace = input;
    Vx3DRotateVector(&inPlace, mat, &inPlace);
    EXPECT_NEAR(inPlace.x, expected.x, 1.0e-6f);
    EXPECT_NEAR(inPlace.y, expected.y, 1.0e-6f);
    EXPECT_NEAR(inPlace.z, expected.z, 1.0e-6f);
}

TEST(SIMDDispatchTest, QuaternionMultiplyMatchesScalarReference) {
    const VxQuaternion lhs[] = {
        VxQuaternion(0.0f, 0.0f, 0.0f, 1.0f),
        VxQuaternion(0.70710677f, 0.0f, 0.0f, 0.70710677f),
        VxQuaternion(0.0f, 0.70710677f, 0.0f, 0.70710677f),
        VxQuaternion(0.1f, -0.2f, 0.3f, 0.9f),
    };
    const VxQuaternion rhs[] = {
        VxQuaternion(0.0f, 0.70710677f, 0.0f, 0.70710677f),
        VxQuaternion(0.0f, 0.0f, 0.70710677f, 0.70710677f),
        VxQuaternion(-0.3f, 0.2f, 0.1f, 0.92f),
        VxQuaternion(0.4f, -0.1f, 0.5f, 0.75f),
    };

    for (size_t i = 0; i < sizeof(lhs) / sizeof(lhs[0]); ++i) {
        const VxQuaternion expected = MultiplyQuaternionScalar(lhs[i], rhs[i]);
        const VxQuaternion actual = Vx3DQuaternionMultiply(lhs[i], rhs[i]);

        EXPECT_NEAR(actual.x, expected.x, 1.0e-6f);
        EXPECT_NEAR(actual.y, expected.y, 1.0e-6f);
        EXPECT_NEAR(actual.z, expected.z, 1.0e-6f);
        EXPECT_NEAR(actual.w, expected.w, 1.0e-6f);
    }
}

TEST(SIMDDispatchTest, QuaternionNormalizeMatchesScalarReference) {
    const VxQuaternion inputs[] = {
        VxQuaternion(0.0f, 0.0f, 0.0f, 0.0f),
        VxQuaternion(0.1f, -0.2f, 0.3f, 0.9f),
        VxQuaternion(-2.0f, 4.0f, -8.0f, 16.0f),
    };

    for (const VxQuaternion &q : inputs) {
        VxQuaternion actual = q;
        actual.Normalize();
        const VxQuaternion expected = NormalizeQuaternionScalar(q);

        EXPECT_NEAR(actual.x, expected.x, 1.0e-6f);
        EXPECT_NEAR(actual.y, expected.y, 1.0e-6f);
        EXPECT_NEAR(actual.z, expected.z, 1.0e-6f);
        EXPECT_NEAR(actual.w, expected.w, 1.0e-6f);
    }
}

TEST(SIMDDispatchTest, QuaternionSlerpMatchesScalarReference) {
    const VxQuaternion a(0.1f, -0.2f, 0.3f, 0.9f);
    const VxQuaternion b(-0.3f, 0.5f, -0.1f, 0.8f);
    const float ts[] = {0.0f, 0.2f, 0.5f, 0.8f, 1.0f};

    for (float t : ts) {
        const VxQuaternion actual = Slerp(t, a, b);
        const VxQuaternion expected = SlerpQuaternionScalar(t, a, b);

        EXPECT_NEAR(actual.x, expected.x, 1.0e-5f);
        EXPECT_NEAR(actual.y, expected.y, 1.0e-5f);
        EXPECT_NEAR(actual.z, expected.z, 1.0e-5f);
        EXPECT_NEAR(actual.w, expected.w, 1.0e-5f);
    }
}

TEST(SIMDDispatchTest, TransformBox2DInvalidBoxSetsClipFlags) {
    VxMatrix worldProjection;
    worldProjection.SetIdentity();

    VxBbox invalidBox; // default ctor is invalid (Min > Max)
    VxRect screen(0.0f, 0.0f, 800.0f, 600.0f);
    VxRect extents;
    VXCLIP_FLAGS orFlags = static_cast<VXCLIP_FLAGS>(0);
    VXCLIP_FLAGS andFlags = static_cast<VXCLIP_FLAGS>(0);

    const XBOOL visible = VxTransformBox2D(worldProjection, invalidBox, &screen, &extents, orFlags, andFlags);

    EXPECT_FALSE(visible);
    EXPECT_EQ(orFlags, VXCLIP_ALL);
    EXPECT_EQ(andFlags, VXCLIP_ALL);
}

TEST(SIMDDispatchTest, TransformBox2DFlatBoxProjectsToScreenBounds) {
    VxMatrix worldProjection;
    worldProjection.SetIdentity();

    VxBbox box(VxVector(-1.0f, -1.0f, 0.5f), VxVector(1.0f, 1.0f, 0.5f));
    VxRect screen(0.0f, 0.0f, 800.0f, 600.0f);
    VxRect extents;
    VXCLIP_FLAGS orFlags = static_cast<VXCLIP_FLAGS>(0);
    VXCLIP_FLAGS andFlags = static_cast<VXCLIP_FLAGS>(0);

    const XBOOL visible = VxTransformBox2D(worldProjection, box, &screen, &extents, orFlags, andFlags);

    EXPECT_TRUE(visible);
    EXPECT_NEAR(extents.left, 0.0f, 1.0e-4f);
    EXPECT_NEAR(extents.top, 0.0f, 1.0e-4f);
    EXPECT_NEAR(extents.right, 800.0f, 1.0e-4f);
    EXPECT_NEAR(extents.bottom, 600.0f, 1.0e-4f);
}

TEST(SIMDDispatchTest, ProjectBoxZExtentsInvalidBoxKeepsLegacyDefaults) {
    VxMatrix worldProjection;
    worldProjection.SetIdentity();

    VxBbox invalidBox;
    float zhMin = 0.0f;
    float zhMax = 0.0f;

    VxProjectBoxZExtents(worldProjection, invalidBox, zhMin, zhMax);

    EXPECT_FLOAT_EQ(zhMin, 1.0e10f);
    EXPECT_FLOAT_EQ(zhMax, -1.0e10f);
}

TEST(SIMDDispatchTest, FillStructureGenericSizeAndStrideMatchesPattern) {
    struct Pattern20 {
        uint32_t data[5];
    };

    Pattern20 pattern = {{0x10203040u, 0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u}};
    alignas(16) unsigned char buffer[96] = {};

    ASSERT_TRUE(VxFillStructure(3, buffer, 32, sizeof(Pattern20), &pattern));

    for (int i = 0; i < 3; ++i) {
        const void *dst = buffer + i * 32;
        EXPECT_EQ(std::memcmp(dst, &pattern, sizeof(Pattern20)), 0);
        EXPECT_EQ(buffer[i * 32 + 20], 0u);
    }
}

TEST(SIMDDispatchTest, CopyStructureGenericSizeWithDifferentStrideMatchesSource) {
    struct SrcElement {
        uint32_t data[7];
        uint32_t pad;
    };

    SrcElement src[3] = {
        {{1u, 2u, 3u, 4u, 5u, 6u, 7u}, 0xAAAAAAAAu},
        {{8u, 9u, 10u, 11u, 12u, 13u, 14u}, 0xBBBBBBBBu},
        {{15u, 16u, 17u, 18u, 19u, 20u, 21u}, 0xCCCCCCCCu},
    };
    alignas(16) unsigned char dst[120] = {};

    ASSERT_TRUE(VxCopyStructure(3, dst, 40, 28, src, sizeof(SrcElement)));

    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(std::memcmp(dst + i * 40, src[i].data, 28), 0);
        EXPECT_EQ(dst[i * 40 + 28], 0u);
    }
}

TEST(SIMDDispatchTest, IndexedCopyGenericSizeFollowsIndices) {
    struct SrcElement {
        uint32_t data[5];
        uint32_t pad;
    };
    struct DstElement {
        uint32_t data[5];
    };

    SrcElement srcData[4] = {
        {{11u, 12u, 13u, 14u, 15u}, 0xAAAAAAA1u},
        {{21u, 22u, 23u, 24u, 25u}, 0xAAAAAAA2u},
        {{31u, 32u, 33u, 34u, 35u}, 0xAAAAAAA3u},
        {{41u, 42u, 43u, 44u, 45u}, 0xAAAAAAA4u},
    };
    DstElement dstData[4] = {};
    int indices[4] = {2, 0, 3, 1};

    VxStridedData src(srcData, sizeof(SrcElement));
    VxStridedData dst(dstData, sizeof(DstElement));

    ASSERT_TRUE(VxIndexedCopy(dst, src, sizeof(DstElement), indices, 4));

    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(std::memcmp(dstData[i].data, srcData[indices[i]].data, sizeof(DstElement)), 0);
    }
}

TEST(SIMDDispatchTest, PtInRectHandlesNegativeAndBoundaryValues) {
    CKRECT rect = {-50, -20, 80, 60};
    CKPOINT inside = {0, 0};
    CKPOINT onEdge = {-50, 60};
    CKPOINT outside = {-51, 0};

    EXPECT_TRUE(VxPtInRect(&rect, &inside));
    EXPECT_TRUE(VxPtInRect(&rect, &onEdge));
    EXPECT_FALSE(VxPtInRect(&rect, &outside));
}

TEST(SIMDDispatchTest, ComputeBestFitBBoxContainsInputPoints) {
    struct PointWithPad {
        float x, y, z, pad;
    };

    PointWithPad points[] = {
        {-3.0f, -1.0f, 2.0f, 0.0f},
        {4.0f, -2.0f, 1.0f, 0.0f},
        {1.0f, 5.0f, -4.0f, 0.0f},
        {-2.0f, 3.0f, 6.0f, 0.0f},
        {0.5f, -4.0f, -1.0f, 0.0f},
    };

    VxMatrix bbox;
    ASSERT_TRUE(VxComputeBestFitBBox(
        reinterpret_cast<const XBYTE *>(points),
        static_cast<XDWORD>(sizeof(PointWithPad)),
        static_cast<int>(sizeof(points) / sizeof(points[0])),
        bbox,
        0.05f
    ));

    const VxVector center(bbox[3][0], bbox[3][1], bbox[3][2]);
    for (const PointWithPad &p : points) {
        const VxVector d(p.x - center.x, p.y - center.y, p.z - center.z);
        for (int axis = 0; axis < 3; ++axis) {
            const VxVector scaledAxis(bbox[axis][0], bbox[axis][1], bbox[axis][2]);
            const float denom = DotProduct(scaledAxis, scaledAxis);
            ASSERT_GT(denom, 0.0f);
            const float local = DotProduct(d, scaledAxis) / denom;
            EXPECT_LE(std::fabs(local), 1.01f);
        }
    }
}

TEST(SIMDDispatchTest, InterpolateVectorArrayContiguousMatchesScalarReference) {
    constexpr int kCount = 97;
    VxVector a[kCount];
    VxVector b[kCount];
    VxVector out[kCount];
    VxVector expected[kCount];

    std::mt19937 rng(1337u);
    for (int i = 0; i < kCount; ++i) {
        a[i] = RandomVector3(rng, -50.0f, 50.0f);
        b[i] = RandomVector3(rng, -50.0f, 50.0f);
    }

    const float t = 0.3125f;
    for (int i = 0; i < kCount; ++i) {
        expected[i].x = a[i].x + (b[i].x - a[i].x) * t;
        expected[i].y = a[i].y + (b[i].y - a[i].y) * t;
        expected[i].z = a[i].z + (b[i].z - a[i].z) * t;
    }

    InterpolateVectorArray(out, a, b, t, kCount, sizeof(VxVector), sizeof(VxVector));
    for (int i = 0; i < kCount; ++i) {
        EXPECT_NEAR(out[i].x, expected[i].x, 1.0e-6f);
        EXPECT_NEAR(out[i].y, expected[i].y, 1.0e-6f);
        EXPECT_NEAR(out[i].z, expected[i].z, 1.0e-6f);
    }
}

#if defined(VX_SIMD_SSE)
TEST(SIMDDispatchTest, RaySquareDistanceSIMDMatchesScalarReference) {
    std::mt19937 rng(2026u);

    for (int i = 0; i < 256; ++i) {
        const VxVector origin = RandomVector3(rng, -20.0f, 20.0f);
        const VxVector direction = RandomVector3(rng, -10.0f, 10.0f);
        const VxVector point = RandomVector3(rng, -30.0f, 30.0f);
        int dummy = 0;
        const VxRay ray(origin, direction, &dummy);

        const float scalar = ScalarRaySquareDistance(ray, point);
        const float simd = VxSIMDRaySquareDistance(&ray, &point);
        EXPECT_NEAR(simd, scalar, 1.0e-5f);
    }

    int dummy = 0;
    const VxRay zeroDirRay(VxVector(1.0f, 2.0f, 3.0f), VxVector(0.0f, 0.0f, 0.0f), &dummy);
    const VxVector p(6.0f, -1.0f, 8.0f);
    EXPECT_NEAR(VxSIMDRaySquareDistance(&zeroDirRay, &p), ScalarRaySquareDistance(zeroDirRay, p), 1.0e-6f);
}

TEST(SIMDDispatchTest, RaySIMDTransformInterpolateDistanceMatchScalarReference) {
    std::mt19937 rng(555u);

    for (int i = 0; i < 192; ++i) {
        const VxVector origin = RandomVector3(rng, -20.0f, 20.0f);
        const VxVector direction = RandomVector3(rng, -15.0f, 15.0f);
        int dummy = 0;
        const VxRay ray(origin, direction, &dummy);
        const VxMatrix m = MakeAffineMatrix(
            RandomRange(rng, -3.0f, 3.0f),
            RandomRange(rng, -3.0f, 3.0f),
            RandomRange(rng, -3.0f, 3.0f),
            RandomRange(rng, -0.4f, 0.4f),
            RandomRange(rng, -0.4f, 0.4f),
            RandomRange(rng, -0.4f, 0.4f),
            RandomRange(rng, 0.3f, 1.8f),
            RandomRange(rng, 0.3f, 1.8f),
            RandomRange(rng, 0.3f, 1.8f));

        VxRay scalarTransformed;
        VxRay simdTransformed;
        ScalarRayTransform(scalarTransformed, ray, m);
        VxSIMDRayTransform(&simdTransformed, &ray, &m);
        EXPECT_NEAR(simdTransformed.m_Origin.x, scalarTransformed.m_Origin.x, 1.0e-5f);
        EXPECT_NEAR(simdTransformed.m_Origin.y, scalarTransformed.m_Origin.y, 1.0e-5f);
        EXPECT_NEAR(simdTransformed.m_Origin.z, scalarTransformed.m_Origin.z, 1.0e-5f);
        EXPECT_NEAR(simdTransformed.m_Direction.x, scalarTransformed.m_Direction.x, 1.0e-5f);
        EXPECT_NEAR(simdTransformed.m_Direction.y, scalarTransformed.m_Direction.y, 1.0e-5f);
        EXPECT_NEAR(simdTransformed.m_Direction.z, scalarTransformed.m_Direction.z, 1.0e-5f);

        const float t = RandomRange(rng, -2.0f, 2.0f);
        VxVector scalarPoint;
        VxVector simdPoint;
        ScalarRayInterpolate(scalarPoint, ray, t);
        VxSIMDRayInterpolate(&simdPoint, &ray, t);
        EXPECT_NEAR(simdPoint.x, scalarPoint.x, 1.0e-6f);
        EXPECT_NEAR(simdPoint.y, scalarPoint.y, 1.0e-6f);
        EXPECT_NEAR(simdPoint.z, scalarPoint.z, 1.0e-6f);

        const VxVector testPoint = RandomVector3(rng, -30.0f, 30.0f);
        const float scalarDist = ScalarRayDistance(ray, testPoint);
        const float simdDist = std::sqrt(VxSIMDRaySquareDistance(&ray, &testPoint));
        EXPECT_NEAR(simdDist, scalarDist, 1.0e-5f);
    }
}

TEST(SIMDDispatchTest, PlaneSIMDMatchesScalarReference) {
    std::mt19937 rng(1201u);

    for (int i = 0; i < 128; ++i) {
        const VxVector normal = RandomVector3(rng, -5.0f, 5.0f);
        const VxVector pointOnPlane = RandomVector3(rng, -10.0f, 10.0f);
        VxPlane scalarPlane;
        VxPlane simdPlane;

        ScalarPlaneCreateFromPoint(scalarPlane, normal, pointOnPlane);
        VxSIMDPlaneCreateFromPoint(&simdPlane, &normal, &pointOnPlane);

        EXPECT_NEAR(simdPlane.m_Normal.x, scalarPlane.m_Normal.x, 1.0e-5f);
        EXPECT_NEAR(simdPlane.m_Normal.y, scalarPlane.m_Normal.y, 1.0e-5f);
        EXPECT_NEAR(simdPlane.m_Normal.z, scalarPlane.m_Normal.z, 1.0e-5f);
        EXPECT_NEAR(simdPlane.m_D, scalarPlane.m_D, 1.0e-5f);

        const VxVector testPoint = RandomVector3(rng, -25.0f, 25.0f);
        EXPECT_NEAR(VxSIMDPlaneClassifyPoint(&simdPlane, &testPoint), ScalarPlaneClassifyPoint(scalarPlane, testPoint), 1.0e-5f);
        EXPECT_NEAR(VxSIMDPlaneDistance(&simdPlane, &testPoint), ScalarPlaneDistance(scalarPlane, testPoint), 1.0e-5f);

        VxVector simdNearest;
        VxSIMDPlaneNearestPoint(&simdNearest, &simdPlane, &testPoint);
        const VxVector scalarNearest = ScalarPlaneNearestPoint(scalarPlane, testPoint);
        EXPECT_NEAR(simdNearest.x, scalarNearest.x, 1.0e-5f);
        EXPECT_NEAR(simdNearest.y, scalarNearest.y, 1.0e-5f);
        EXPECT_NEAR(simdNearest.z, scalarNearest.z, 1.0e-5f);
    }

    VxPlane scalarTri;
    VxPlane simdTri;
    const VxVector a(0.0f, 0.0f, 1.0f);
    const VxVector b(1.0f, 0.0f, 1.0f);
    const VxVector c(0.0f, 1.0f, 1.0f);
    ScalarPlaneCreateFromTriangle(scalarTri, a, b, c);
    VxSIMDPlaneCreateFromTriangle(&simdTri, &a, &b, &c);
    EXPECT_NEAR(simdTri.m_Normal.x, scalarTri.m_Normal.x, 1.0e-5f);
    EXPECT_NEAR(simdTri.m_Normal.y, scalarTri.m_Normal.y, 1.0e-5f);
    EXPECT_NEAR(simdTri.m_Normal.z, scalarTri.m_Normal.z, 1.0e-5f);
    EXPECT_NEAR(simdTri.m_D, scalarTri.m_D, 1.0e-5f);
}

TEST(SIMDDispatchTest, PlaneXClassifySIMDMatchesScalarReference) {
    std::mt19937 rng(321u);

    for (int i = 0; i < 192; ++i) {
        VxPlane plane;
        const VxVector normal = RandomVector3(rng, -4.0f, 4.0f);
        const VxVector pointOnPlane = RandomVector3(rng, -8.0f, 8.0f);
        VxSIMDPlaneCreateFromPoint(&plane, &normal, &pointOnPlane);

        VxVector boxAxis[4];
        boxAxis[0] = RandomVector3(rng, -3.0f, 3.0f);
        boxAxis[1] = RandomVector3(rng, -3.0f, 3.0f);
        boxAxis[2] = RandomVector3(rng, -3.0f, 3.0f);
        boxAxis[3] = RandomVector3(rng, -3.0f, 3.0f);

        const float scalar = ScalarPlaneXClassify(plane, boxAxis);
        const float simd = VxSIMDPlaneXClassify(&plane, boxAxis);
        EXPECT_NEAR(simd, scalar, 1.0e-5f);
    }
}

TEST(SIMDDispatchTest, RectSIMDTransformPathsMatchScalarReference) {
    std::mt19937 rng(777u);
    const VxRect srcScreen(-320.0f, -240.0f, 320.0f, 240.0f);
    const VxRect dstScreen(0.0f, 0.0f, 1280.0f, 720.0f);

    for (int i = 0; i < 128; ++i) {
        VxRect simdRect(RandomRange(rng, -300.0f, 200.0f),
                        RandomRange(rng, -200.0f, 100.0f),
                        RandomRange(rng, 50.0f, 350.0f),
                        RandomRange(rng, 80.0f, 280.0f));
        VxRect scalarRect = simdRect;
        VxSIMDRectTransform(&simdRect, &dstScreen, &srcScreen);
        ScalarRectTransform(scalarRect, dstScreen, srcScreen);
        EXPECT_NEAR(simdRect.left, scalarRect.left, 1.0e-5f);
        EXPECT_NEAR(simdRect.top, scalarRect.top, 1.0e-5f);
        EXPECT_NEAR(simdRect.right, scalarRect.right, 1.0e-5f);
        EXPECT_NEAR(simdRect.bottom, scalarRect.bottom, 1.0e-5f);

        VxRect simdH = simdRect;
        VxRect scalarH = scalarRect;
        VxSIMDRectTransformToHomogeneous(&simdH, &dstScreen);
        ScalarRectTransformToHomogeneous(scalarH, dstScreen);
        EXPECT_NEAR(simdH.left, scalarH.left, 1.0e-5f);
        EXPECT_NEAR(simdH.top, scalarH.top, 1.0e-5f);
        EXPECT_NEAR(simdH.right, scalarH.right, 1.0e-5f);
        EXPECT_NEAR(simdH.bottom, scalarH.bottom, 1.0e-5f);

        VxSIMDRectTransformFromHomogeneous(&simdH, &dstScreen);
        ScalarRectTransformFromHomogeneous(scalarH, dstScreen);
        EXPECT_NEAR(simdH.left, scalarH.left, 1.0e-4f);
        EXPECT_NEAR(simdH.top, scalarH.top, 1.0e-4f);
        EXPECT_NEAR(simdH.right, scalarH.right, 1.0e-4f);
        EXPECT_NEAR(simdH.bottom, scalarH.bottom, 1.0e-4f);
    }
}

TEST(SIMDDispatchTest, BboxSIMDTransformMatchesScalarReference) {
    std::mt19937 rng(9001u);

    for (int i = 0; i < 96; ++i) {
        const VxVector minV(RandomRange(rng, -10.0f, -1.0f), RandomRange(rng, -10.0f, -1.0f), RandomRange(rng, -10.0f, -1.0f));
        const VxVector maxV(RandomRange(rng, 1.0f, 10.0f), RandomRange(rng, 1.0f, 10.0f), RandomRange(rng, 1.0f, 10.0f));
        const VxBbox box(minV, maxV);
        const VxMatrix m = MakeAffineMatrix(
            RandomRange(rng, -5.0f, 5.0f),
            RandomRange(rng, -5.0f, 5.0f),
            RandomRange(rng, -5.0f, 5.0f),
            RandomRange(rng, -0.5f, 0.5f),
            RandomRange(rng, -0.5f, 0.5f),
            RandomRange(rng, -0.5f, 0.5f),
            RandomRange(rng, 0.2f, 2.0f),
            RandomRange(rng, 0.2f, 2.0f),
            RandomRange(rng, 0.2f, 2.0f));

        VxVector simdPts[8];
        VxVector scalarPts[8];
        VxSIMDBboxTransformTo(&box, simdPts, &m);
        ScalarBboxTransformTo(box, scalarPts, m);
        for (int k = 0; k < 8; ++k) {
            EXPECT_NEAR(simdPts[k].x, scalarPts[k].x, 1.0e-5f);
            EXPECT_NEAR(simdPts[k].y, scalarPts[k].y, 1.0e-5f);
            EXPECT_NEAR(simdPts[k].z, scalarPts[k].z, 1.0e-5f);
        }

        VxBbox simdOut;
        VxBbox scalarOut;
        VxSIMDBboxTransformFrom(&simdOut, &box, &m);
        ScalarBboxTransformFrom(scalarOut, box, m);
        EXPECT_NEAR(simdOut.Min.x, scalarOut.Min.x, 1.0e-5f);
        EXPECT_NEAR(simdOut.Min.y, scalarOut.Min.y, 1.0e-5f);
        EXPECT_NEAR(simdOut.Min.z, scalarOut.Min.z, 1.0e-5f);
        EXPECT_NEAR(simdOut.Max.x, scalarOut.Max.x, 1.0e-5f);
        EXPECT_NEAR(simdOut.Max.y, scalarOut.Max.y, 1.0e-5f);
        EXPECT_NEAR(simdOut.Max.z, scalarOut.Max.z, 1.0e-5f);
    }
}

TEST(SIMDDispatchTest, FrustumSIMDComputeVerticesAndTransformMatchScalarReference) {
    std::mt19937 rng(808u);

    for (int i = 0; i < 96; ++i) {
        const VxVector origin = RandomVector3(rng, -6.0f, 6.0f);
        const float dMin = RandomRange(rng, 0.2f, 2.5f);
        const float dMax = dMin + RandomRange(rng, 1.0f, 20.0f);
        const float fov = RandomRange(rng, 0.4f, 1.2f);
        const float aspect = RandomRange(rng, 0.7f, 2.0f);

        VxFrustum scalarFrustum(
            origin,
            VxVector::axisX(),
            VxVector::axisY(),
            VxVector::axisZ(),
            dMin,
            dMax,
            fov,
            aspect);
        VxFrustum simdFrustum = scalarFrustum;

        VxVector scalarVertices[8];
        VxVector simdVertices[8];
        ScalarFrustumComputeVertices(scalarFrustum, scalarVertices);
        VxSIMDFrustumComputeVertices(&simdFrustum, simdVertices);
        for (int k = 0; k < 8; ++k) {
            EXPECT_NEAR(simdVertices[k].x, scalarVertices[k].x, 1.0e-5f);
            EXPECT_NEAR(simdVertices[k].y, scalarVertices[k].y, 1.0e-5f);
            EXPECT_NEAR(simdVertices[k].z, scalarVertices[k].z, 1.0e-5f);
        }

        const VxMatrix m = MakeAffineMatrix(
            RandomRange(rng, -3.0f, 3.0f),
            RandomRange(rng, -3.0f, 3.0f),
            RandomRange(rng, -3.0f, 3.0f),
            RandomRange(rng, -0.4f, 0.4f),
            RandomRange(rng, -0.4f, 0.4f),
            RandomRange(rng, -0.4f, 0.4f),
            RandomRange(rng, 0.5f, 2.0f),
            RandomRange(rng, 0.5f, 2.0f),
            RandomRange(rng, 0.5f, 2.0f));

        ScalarFrustumTransform(scalarFrustum, m);
        VxSIMDFrustumTransform(&simdFrustum, &m);

        EXPECT_NEAR(simdFrustum.GetOrigin().x, scalarFrustum.GetOrigin().x, 1.0e-4f);
        EXPECT_NEAR(simdFrustum.GetOrigin().y, scalarFrustum.GetOrigin().y, 1.0e-4f);
        EXPECT_NEAR(simdFrustum.GetOrigin().z, scalarFrustum.GetOrigin().z, 1.0e-4f);
        EXPECT_NEAR(simdFrustum.GetRight().x, scalarFrustum.GetRight().x, 1.0e-4f);
        EXPECT_NEAR(simdFrustum.GetRight().y, scalarFrustum.GetRight().y, 1.0e-4f);
        EXPECT_NEAR(simdFrustum.GetRight().z, scalarFrustum.GetRight().z, 1.0e-4f);
        EXPECT_NEAR(simdFrustum.GetUp().x, scalarFrustum.GetUp().x, 1.0e-4f);
        EXPECT_NEAR(simdFrustum.GetUp().y, scalarFrustum.GetUp().y, 1.0e-4f);
        EXPECT_NEAR(simdFrustum.GetUp().z, scalarFrustum.GetUp().z, 1.0e-4f);
        EXPECT_NEAR(simdFrustum.GetDir().x, scalarFrustum.GetDir().x, 1.0e-4f);
        EXPECT_NEAR(simdFrustum.GetDir().y, scalarFrustum.GetDir().y, 1.0e-4f);
        EXPECT_NEAR(simdFrustum.GetDir().z, scalarFrustum.GetDir().z, 1.0e-4f);
        EXPECT_NEAR(simdFrustum.GetRBound(), scalarFrustum.GetRBound(), 1.0e-4f);
        EXPECT_NEAR(simdFrustum.GetUBound(), scalarFrustum.GetUBound(), 1.0e-4f);
        EXPECT_NEAR(simdFrustum.GetDMin(), scalarFrustum.GetDMin(), 1.0e-4f);
        EXPECT_NEAR(simdFrustum.GetDMax(), scalarFrustum.GetDMax(), 1.0e-4f);
    }
}

TEST(SIMDDispatchTest, TransformBox2DSIMDMatchesScalarReference) {
    std::mt19937 rng(42u);
    const VxRect screen(0.0f, 0.0f, 1920.0f, 1080.0f);

    for (int i = 0; i < 160; ++i) {
        const VxVector minV(
            RandomRange(rng, -2.0f, 0.0f),
            RandomRange(rng, -2.0f, 0.0f),
            RandomRange(rng, -2.0f, 0.0f));
        VxVector maxV(
            RandomRange(rng, 0.0f, 2.0f),
            RandomRange(rng, 0.0f, 2.0f),
            RandomRange(rng, 0.0f, 2.0f));
        if ((i % 7) == 0) {
            maxV.z = minV.z;
        }
        const VxBbox box(minV, maxV);

        VxMatrix worldProjection = MakeAffineMatrix(
            RandomRange(rng, -2.0f, 2.0f),
            RandomRange(rng, -2.0f, 2.0f),
            RandomRange(rng, 1.0f, 8.0f),
            RandomRange(rng, -0.3f, 0.3f),
            RandomRange(rng, -0.3f, 0.3f),
            RandomRange(rng, -0.3f, 0.3f),
            RandomRange(rng, 0.4f, 2.0f),
            RandomRange(rng, 0.4f, 2.0f),
            RandomRange(rng, 0.4f, 2.0f));
        if ((i % 11) == 0) {
            worldProjection[2][3] = -1.0f;
            worldProjection[3][3] = 0.0f;
        }

        VxRect scalarExtents(0.0f, 0.0f, 0.0f, 0.0f);
        VxRect simdExtents(0.0f, 0.0f, 0.0f, 0.0f);
        VXCLIP_FLAGS scalarOr = static_cast<VXCLIP_FLAGS>(0);
        VXCLIP_FLAGS scalarAnd = static_cast<VXCLIP_FLAGS>(0);
        VXCLIP_FLAGS simdOr = static_cast<VXCLIP_FLAGS>(0);
        VXCLIP_FLAGS simdAnd = static_cast<VXCLIP_FLAGS>(0);

        const XBOOL scalarVisible = ScalarTransformBox2DReference(worldProjection, box, &screen, &scalarExtents, scalarOr, scalarAnd);
        const XBOOL simdVisible = VxSIMDTransformBox2D(&worldProjection, &box, &simdExtents, &screen, &simdOr, &simdAnd);

        EXPECT_EQ(simdVisible, scalarVisible);
        EXPECT_EQ(simdOr, scalarOr);
        EXPECT_EQ(simdAnd, scalarAnd);
        EXPECT_NEAR(simdExtents.left, scalarExtents.left, 1.0e-3f);
        EXPECT_NEAR(simdExtents.top, scalarExtents.top, 1.0e-3f);
        EXPECT_NEAR(simdExtents.right, scalarExtents.right, 1.0e-3f);
        EXPECT_NEAR(simdExtents.bottom, scalarExtents.bottom, 1.0e-3f);
    }
}

TEST(SIMDDispatchTest, ProjectBoxZExtentsSIMDMatchesScalarReference) {
    std::mt19937 rng(314159u);

    for (int i = 0; i < 160; ++i) {
        const VxVector minV(RandomRange(rng, -5.0f, -0.1f), RandomRange(rng, -5.0f, -0.1f), RandomRange(rng, -5.0f, 0.1f));
        const VxVector maxV(RandomRange(rng, 0.1f, 5.0f), RandomRange(rng, 0.1f, 5.0f), RandomRange(rng, 0.1f, 8.0f));
        const VxBbox box(minV, maxV);
        const VxMatrix m = MakeAffineMatrix(
            RandomRange(rng, -2.0f, 2.0f),
            RandomRange(rng, -2.0f, 2.0f),
            RandomRange(rng, 0.5f, 6.0f),
            RandomRange(rng, -0.3f, 0.3f),
            RandomRange(rng, -0.3f, 0.3f),
            RandomRange(rng, -0.3f, 0.3f),
            RandomRange(rng, 0.5f, 2.0f),
            RandomRange(rng, 0.5f, 2.0f),
            RandomRange(rng, 0.5f, 2.0f));

        float simdMin = 0.0f;
        float simdMax = 0.0f;
        VxSIMDProjectBoxZExtents(&m, &box, &simdMin, &simdMax);

        float scalarMin = 0.0f;
        float scalarMax = 0.0f;
        ScalarProjectBoxZExtents(m, box, scalarMin, scalarMax);

        EXPECT_NEAR(simdMin, scalarMin, 1.0e-5f);
        EXPECT_NEAR(simdMax, scalarMax, 1.0e-5f);
    }
}
#endif

} // namespace
