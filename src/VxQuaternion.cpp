#include "VxQuaternion.h"

#include "VxMatrix.h"

VxQuaternion Vx3DQuaternionSnuggle(VxQuaternion *Quat, VxVector *Scale) {
    return VxQuaternion();
}

VxQuaternion Vx3DQuaternionFromMatrix(const VxMatrix &Mat) {
    VxQuaternion quat;
    quat.FromMatrix(Mat, TRUE, TRUE);
    return quat;
}

VxQuaternion Vx3DQuaternionConjugate(const VxQuaternion &Quat) {
    return VxQuaternion(-Quat.x, -Quat.y, -Quat.z, Quat.angle);
}

VxQuaternion Vx3DQuaternionMultiply(const VxQuaternion &QuatL, const VxQuaternion &QuatR) {
    return VxQuaternion(
        QuatL.x * QuatR.w +
        QuatR.x * QuatL.w +
        QuatR.z * QuatL.y -
        QuatL.z * QuatR.y,
        QuatL.y * QuatR.w +
        QuatR.y * QuatL.w +
        QuatL.z * QuatR.x -
        QuatR.z * QuatL.x,
        QuatL.z * QuatR.w +
        QuatR.z * QuatL.w +
        QuatR.y * QuatL.x -
        QuatL.y * QuatR.x,
        QuatR.w * QuatL.w -
        QuatL.x * QuatR.x -
        QuatR.y * QuatL.y -
        QuatR.z * QuatL.z
    );
}

VxQuaternion Vx3DQuaternionDivide(const VxQuaternion &P, const VxQuaternion &Q) {
    return VxQuaternion(
        Q.w * P.x - Q.x * P.w - P.z * Q.y + P.y * Q.z,
        P.y * Q.w - P.w * Q.y - P.x * Q.z + P.z * Q.x,
        P.z * Q.w - P.w * Q.z - P.y * Q.x + P.x * Q.y,
        P.z * Q.z + P.y * Q.y + P.x * Q.x + Q.w * P.w
    );
}

VxQuaternion Slerp(float Theta, const VxQuaternion &Quat1, const VxQuaternion &Quat2) {
//    glm::quat q = glm::slerp(*reinterpret_cast<const glm::quat *>(&Quat1), *reinterpret_cast<const glm::quat *>(&Quat2), Theta);
//    return *reinterpret_cast<VxQuaternion *>(&q);
    return VxQuaternion();
}

VxQuaternion Squad(float Theta, const VxQuaternion &Quat1, const VxQuaternion &Quat1Out, const VxQuaternion &Quat2In,
                   const VxQuaternion &Quat2) {
    float theta = (1.0f - Theta) * Theta + (1.0f - Theta) * Theta;
    VxQuaternion quat1 = Slerp(Theta, Quat1, Quat2);
    VxQuaternion quat2 = Slerp(Theta, Quat1Out, Quat2In);
    return Slerp(theta, quat1, quat2);
}

VxQuaternion LnDif(const VxQuaternion &P, const VxQuaternion &Q) {
    return Ln(Vx3DQuaternionDivide(Q, P));
}

VxQuaternion Ln(const VxQuaternion &Quat) {
    return VxQuaternion();
}

VxQuaternion Exp(const VxQuaternion &Quat) {
    return VxQuaternion();
}

void VxQuaternion::FromMatrix(const VxMatrix &Mat, XBOOL MatIsUnit, XBOOL RestoreMat) {
}

void VxQuaternion::ToMatrix(VxMatrix &Mat) const {
}

void VxQuaternion::Multiply(const VxQuaternion &Quat) {
    y = Quat.y * w + y * Quat.w + z * Quat.x - Quat.z * x;
    z = Quat.z * w + z * Quat.w + Quat.y * x - y * Quat.x;
    x = w * Quat.x + x * Quat.w + y * Quat.z - z * Quat.y;
    w = w * Quat.w - Quat.x * x - y * Quat.y - z * Quat.z;
}

void VxQuaternion::FromRotation(const VxVector &Vector, float Angle) {
    VxMatrix mat;
    Vx3DMatrixFromRotation(mat, Vector, Angle);
    FromMatrix(mat, 1, 1);
}

void VxQuaternion::FromEulerAngles(float eax, float eay, float eaz) {
    VxMatrix mat;
    Vx3DMatrixFromEulerAngles(mat, eax, eay, eaz);
    FromMatrix(mat, TRUE, TRUE);
}

void VxQuaternion::ToEulerAngles(float *eax, float *eay, float *eaz) const {
    VxMatrix mat;
    ToMatrix(mat);
    Vx3DMatrixToEulerAngles(mat, eax, eay, eaz);
}

void VxQuaternion::Normalize() {
    float n = sqrtf(x * x + y * y + z * z + w * w);
    if (n == 0.0f) {
        x = 0.0f;
        y = 0.0f;
        z = 0.0f;
        w = 1.0f;
    } else {
        x = 1.0f / n * x;
        y = 1.0f / n * y;
        z = 1.0f / n * z;
        w = 1.0f / n * w;
    }
}