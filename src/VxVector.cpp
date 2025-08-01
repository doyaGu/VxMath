#include "VxVector.h"
#include "VxMatrix.h"

// Global utility functions
int radToAngle(float val) {
    return (int) (val * (180.0f / PI));
}

float Tsin(int angle) {
    return sinf((float) angle);
}

float Tcos(int angle) {
    return cosf((float) angle);
}

// Static vector constants
const VxVector VxVector::m_AxisX(1.0f, 0.0f, 0.0f);
const VxVector VxVector::m_AxisY(0.0f, 1.0f, 0.0f);
const VxVector VxVector::m_AxisZ(0.0f, 0.0f, 1.0f);
const VxVector VxVector::m_Axis0(0.0f, 0.0f, 0.0f);
const VxVector VxVector::m_Axis1(1.0f, 1.0f, 1.0f);

// Assignment from compressed vector
VxVector &VxVector::operator=(const VxCompressedVector &v) {
    // Convert from polar angles to cartesian coordinates
    x = sinf(((float) v.ya / 32767.0f) * PI) * sinf(((float) v.xa / 32767.0f) * PI);
    y = -sinf(((float) v.xa / 32767.0f) * PI);
    z = cosf(((float) v.ya / 32767.0f) * PI) * cosf(((float) v.xa / 32767.0f) * PI);
    return *this;
}

// VxVector normalization
void VxVector::Normalize() {
    const float magSq = SquareMagnitude();
    if (magSq > EPSILON) {
        const float invMag = 1.0f / sqrtf(magSq);
        x *= invMag;
        y *= invMag;
        z *= invMag;
    }
}

// Rotate a vector by a matrix
void VxVector::Rotate(const VxMatrix &M) {
    const float nx = M[0][0] * x + M[1][0] * y + M[2][0] * z;
    const float ny = M[0][1] * x + M[1][1] * y + M[2][1] * z;
    const float nz = M[0][2] * x + M[1][2] * y + M[2][2] * z;

    x = nx;
    y = ny;
    z = nz;
}

// Static accessor methods
const VxVector &VxVector::axisX() {
    return m_AxisX;
}

const VxVector &VxVector::axisY() {
    return m_AxisY;
}

const VxVector &VxVector::axisZ() {
    return m_AxisZ;
}

const VxVector &VxVector::axis0() {
    return m_Axis0;
}

const VxVector &VxVector::axis1() {
    return m_Axis1;
}

// Rotation functions
const VxVector Rotate(const VxMatrix &mat, const VxVector &pt) {
    return VxVector(
        mat[0][0] * pt.x + mat[1][0] * pt.y + mat[2][0] * pt.z,
        mat[0][1] * pt.x + mat[1][1] * pt.y + mat[2][1] * pt.z,
        mat[0][2] * pt.x + mat[1][2] * pt.y + mat[2][2] * pt.z
    );
}

const VxVector Rotate(const VxVector &v1, const VxVector &v2, float angle) {
    // Create normalized rotation axis
    VxVector n = Normalize(v2);

    // Rodrigues' rotation formula
    const float sinAngle = sinf(angle);
    const float cosAngle = cosf(angle);
    const float oneMinusCos = 1.0f - cosAngle;

    // Implementation using the expanded form for better numerical stability
    VxVector v;
    v.x = v1.x * ((1.0f - n.x * n.x) * cosAngle + n.x * n.x) +
        v1.y * (sinAngle * n.z + oneMinusCos * n.x * n.y) +
        v1.z * (oneMinusCos * n.x * n.z - sinAngle * n.y);

    v.y = v1.y * ((1.0f - n.y * n.y) * cosAngle + n.y * n.y) +
        v1.z * (sinAngle * n.x + oneMinusCos * n.y * n.z) +
        v1.x * (oneMinusCos * n.x * n.y - sinAngle * n.z);

    v.z = v1.z * ((1.0f - n.z * n.z) * cosAngle + n.z * n.z) +
        v1.x * (sinAngle * n.y + oneMinusCos * n.z * n.x) +
        v1.y * (oneMinusCos * n.y * n.z - sinAngle * n.x);

    return v;
}

const VxVector Rotate(const VxVector *v1, const VxVector *v2, float angle) {
    return Rotate(*v1, *v2, angle);
}

// Enhanced versions with Vx prefix
const VxVector VxRotate(const VxMatrix &mat, const VxVector &pt) {
    return Rotate(mat, pt);
}

const VxVector VxRotate(const VxVector &v1, const VxVector &v2, float angle) {
    return Rotate(v1, v2, angle);
}

const VxVector VxRotate(const VxVector *v1, const VxVector *v2, float angle) {
    return Rotate(v1, v2, angle);
}
