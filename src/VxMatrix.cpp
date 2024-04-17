#include "VxMatrix.h"

VxMatrix TheIdentityMatrix;

void Vx3DMatrixIdentity(VxMatrix &Mat)
{
    Mat = TheIdentityMatrix;
}

void Vx3DMultiplyMatrixVector(VxVector *ResultVector, const VxMatrix &Mat, const VxVector *Vector)
{

}

void Vx3DMultiplyMatrixVectorMany(VxVector *ResultVectors, const VxMatrix &Mat, const VxVector *Vectors, int count, int stride)
{

}

void Vx3DMultiplyMatrixVector4(VxVector4 *ResultVector, const VxMatrix &Mat, const VxVector4 *Vector)
{

}

void Vx3DMultiplyMatrixVector4(VxVector4 *ResultVector, const VxMatrix &Mat, const VxVector *Vector)
{

}

void Vx3DRotateVector(VxVector *ResultVector, const VxMatrix &Mat, const VxVector *Vector)
{

}

void Vx3DRotateVectorMany(VxVector *ResultVector, const VxMatrix &Mat, const VxVector *Vector, int count, int stride)
{

}

void Vx3DMultiplyMatrix(VxMatrix &ResultMat, const VxMatrix &MatA, const VxMatrix &MatB)
{

}

void Vx3DMultiplyMatrix4(VxMatrix &ResultMat, const VxMatrix &MatA, const VxMatrix &MatB)
{

}

void Vx3DInverseMatrix(VxMatrix &InverseMat, const VxMatrix &Mat)
{

}

float Vx3DMatrixDeterminant(const VxMatrix &Mat)
{
    return (Mat[1][1] * Mat[2][2] - Mat[2][1] * Mat[1][2]) * Mat[0][0]
           - (Mat[1][0] * Mat[2][2] - Mat[2][0] * Mat[1][2]) * Mat[0][1]
           + (Mat[2][1] * Mat[1][0] - Mat[2][0] * Mat[1][1]) * Mat[0][2];
}

void Vx3DMatrixFromRotation(VxMatrix &ResultMat, const VxVector &Vector, float Angle)
{

}

void Vx3DMatrixFromRotationAndOrigin(VxMatrix &ResultMat, const VxVector &Vector, const VxVector &Origin, float Angle)
{

}

void Vx3DMatrixFromEulerAngles(VxMatrix &Mat, float eax, float eay, float eaz)
{

}

void Vx3DMatrixToEulerAngles(const VxMatrix &Mat, float *eax, float *eay, float *eaz)
{

}

void Vx3DInterpolateMatrix(float step, VxMatrix &Res, const VxMatrix &A, const VxMatrix &B)
{

}

void Vx3DInterpolateMatrixNoScale(float step, VxMatrix &Res, const VxMatrix &A, const VxMatrix &B)
{

}

void Vx3DMultiplyMatrixVectorStrided(VxStridedData *Dest, VxStridedData *Src, const VxMatrix &Mat, int count)
{

}

void Vx3DMultiplyMatrixVector4Strided(VxStridedData *Dest, VxStridedData *Src, const VxMatrix &Mat, int count)
{

}

void Vx3DRotateVectorStrided(VxStridedData *Dest, VxStridedData *Src, const VxMatrix &Mat, int count)
{

}

void Vx3DTransposeMatrix(VxMatrix &Result, const VxMatrix &A)
{
    Result[0][0] = A[0][0];
    Result[0][1] = A[1][0];
    Result[0][2] = A[2][0];
    Result[0][3] = A[3][0];
    Result[1][0] = A[0][1];
    Result[1][1] = A[1][1];
    Result[1][2] = A[2][1];
    Result[1][3] = A[3][1];
    Result[2][0] = A[0][2];
    Result[2][1] = A[1][2];
    Result[2][2] = A[2][2];
    Result[2][3] = A[3][2];
    Result[3][0] = A[0][3];
    Result[3][1] = A[1][3];
    Result[3][2] = A[2][3];
    Result[3][3] = A[3][3];
}