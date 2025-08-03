#include "VxMatrix.h"

static bool gMatrixInitialized = false;
VxMatrix TheIdentityMatrix;

static void InitializeIdentityMatrix() {
    if (!gMatrixInitialized) {
        TheIdentityMatrix.SetIdentity();
        gMatrixInitialized = true;
    }
}

void Vx3DMatrixIdentity(VxMatrix &Mat) {
    InitializeIdentityMatrix();
    Mat = TheIdentityMatrix;
}

const VxMatrix &VxMatrix::Identity() {
    InitializeIdentityMatrix();
    return TheIdentityMatrix;
}

void Vx3DMultiplyMatrixVector(VxVector *ResultVector, const VxMatrix &Mat, const VxVector *Vector) {
    VxVector temp;
    temp.x = Vector->x * Mat[0][0] + Vector->y * Mat[1][0] + Vector->z * Mat[2][0] + Mat[3][0];
    temp.y = Vector->x * Mat[0][1] + Vector->y * Mat[1][1] + Vector->z * Mat[2][1] + Mat[3][1];
    temp.z = Vector->x * Mat[0][2] + Vector->y * Mat[1][2] + Vector->z * Mat[2][2] + Mat[3][2];
    *ResultVector = temp;
}

void Vx3DMultiplyMatrixVectorMany(VxVector *ResultVectors, const VxMatrix &Mat, const VxVector *Vectors, int count, int stride) {
    do {
        VxVector temp;
        temp.x = Vectors->x * Mat[0][0] + Vectors->y * Mat[1][0] + Vectors->z * Mat[2][0] + Mat[3][0];
        temp.y = Vectors->x * Mat[0][1] + Vectors->y * Mat[1][1] + Vectors->z * Mat[2][1] + Mat[3][1];
        temp.z = Vectors->x * Mat[0][2] + Vectors->y * Mat[1][2] + Vectors->z * Mat[2][2] + Mat[3][2];
        *ResultVectors = temp;

        ResultVectors = (VxVector *) ((char *) ResultVectors + stride);
        Vectors = (const VxVector *) ((char *) Vectors + stride);
        --count;
    } while (count);
}

void Vx3DMultiplyMatrixVector4(VxVector4 *ResultVector, const VxMatrix &Mat, const VxVector4 *Vector) {
    VxVector4 temp;
    temp.x = Vector->x * Mat[0][0] + Vector->y * Mat[1][0] + Vector->z * Mat[2][0] + Vector->w * Mat[3][0];
    temp.y = Vector->x * Mat[0][1] + Vector->y * Mat[1][1] + Vector->z * Mat[2][1] + Vector->w * Mat[3][1];
    temp.z = Vector->x * Mat[0][2] + Vector->y * Mat[1][2] + Vector->z * Mat[2][2] + Vector->w * Mat[3][2];
    temp.w = Vector->x * Mat[0][3] + Vector->y * Mat[1][3] + Vector->z * Mat[2][3] + Vector->w * Mat[3][3];
    *ResultVector = temp;
}

void Vx3DMultiplyMatrixVector4(VxVector4 *ResultVector, const VxMatrix &Mat, const VxVector *Vector) {
    VxVector4 temp;
    temp.x = Vector->x * Mat[0][0] + Vector->y * Mat[1][0] + Vector->z * Mat[2][0] + Mat[3][0];
    temp.y = Vector->x * Mat[0][1] + Vector->y * Mat[1][1] + Vector->z * Mat[2][1] + Mat[3][1];
    temp.z = Vector->x * Mat[0][2] + Vector->y * Mat[1][2] + Vector->z * Mat[2][2] + Mat[3][2];
    temp.w = Vector->x * Mat[0][3] + Vector->y * Mat[1][3] + Vector->z * Mat[2][3] + Mat[3][3];
    *ResultVector = temp;
}

void Vx3DRotateVector(VxVector *ResultVector, const VxMatrix &Mat, const VxVector *Vector) {
    VxVector temp;
    temp.x = Vector->x * Mat[0][0] + Vector->y * Mat[1][0] + Vector->z * Mat[2][0];
    temp.y = Vector->x * Mat[0][1] + Vector->y * Mat[1][1] + Vector->z * Mat[2][1];
    temp.z = Vector->x * Mat[0][2] + Vector->y * Mat[1][2] + Vector->z * Mat[2][2];
    *ResultVector = temp;
}

void Vx3DRotateVectorMany(VxVector *ResultVector, const VxMatrix &Mat, const VxVector *Vector, int count, int stride) {
    do {
        VxVector temp;
        temp.x = Vector->x * Mat[0][0] + Vector->y * Mat[1][0] + Vector->z * Mat[2][0];
        temp.y = Vector->x * Mat[0][1] + Vector->y * Mat[1][1] + Vector->z * Mat[2][1];
        temp.z = Vector->x * Mat[0][2] + Vector->y * Mat[1][2] + Vector->z * Mat[2][2];
        *ResultVector = temp;

        Vector = (const VxVector *) ((char *) Vector + stride);
        ResultVector = (VxVector *) ((char *) ResultVector + stride);
        --count;
    } while (count);
}

void Vx3DMultiplyMatrix(VxMatrix &ResultMat, const VxMatrix &MatA, const VxMatrix &MatB) {
    // Use a temporary matrix in case ResultMat is the same as MatA or MatB
    VxMatrix tmp;

    // Perform full 4x4 matrix multiplication but preserve the standard 3D transformation structure
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            tmp[i][j] = MatA[0][j] * MatB[i][0] +
                        MatA[1][j] * MatB[i][1] +
                        MatA[2][j] * MatB[i][2] +
                        MatA[3][j] * MatB[i][3];
        }
    }

    // For standard 3D transformations, ensure the bottom row is [0,0,0,1]
    tmp[0][3] = 0.0f;
    tmp[1][3] = 0.0f;
    tmp[2][3] = 0.0f;
    tmp[3][3] = 1.0f;

    ResultMat = tmp;
}

void Vx3DMultiplyMatrix4(VxMatrix &ResultMat, const VxMatrix &MatA, const VxMatrix &MatB) {
    // Use a temporary matrix in case ResultMat is the same as MatA or MatB
    VxMatrix tmp;

    // Full 4x4 matrix multiply
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            tmp[i][j] = MatA[0][j] * MatB[i][0] +
                MatA[1][j] * MatB[i][1] +
                MatA[2][j] * MatB[i][2] +
                MatA[3][j] * MatB[i][3];
        }
    }

    ResultMat = tmp;
}

void Vx3DInverseMatrix(VxMatrix &InverseMat, const VxMatrix &Mat) {
    // Extract the 3x3 rotation/scale submatrix for cleaner code
    const float a00 = Mat[0][0], a01 = Mat[0][1], a02 = Mat[0][2];
    const float a10 = Mat[1][0], a11 = Mat[1][1], a12 = Mat[1][2];
    const float a20 = Mat[2][0], a21 = Mat[2][1], a22 = Mat[2][2];

    // Calculate determinant using the rule of Sarrus expansion
    const double det = a00 * (a11 * a22 - a12 * a21) +
                       a01 * (a12 * a20 - a10 * a22) +
                       a02 * (a10 * a21 - a11 * a20);

    // Check if matrix is invertible (determinant is non-zero)
    if (abs(det) < 1.0e-15) {
        // Matrix is singular - set to identity as fallback
        InverseMat.SetIdentity();
        return;
    }

    const double inv_det = 1.0 / det;

    // Calculate inverse of the 3x3 submatrix using cofactor method
    // Each element is the cofactor divided by the determinant

    // First row of inverse
    InverseMat[0][0] = static_cast<float>((a11 * a22 - a12 * a21) * inv_det);
    InverseMat[0][1] = static_cast<float>((a02 * a21 - a01 * a22) * inv_det);
    InverseMat[0][2] = static_cast<float>((a01 * a12 - a02 * a11) * inv_det);
    InverseMat[0][3] = 0.0f;

    // Second row of inverse
    InverseMat[1][0] = static_cast<float>((a12 * a20 - a10 * a22) * inv_det);
    InverseMat[1][1] = static_cast<float>((a00 * a22 - a02 * a20) * inv_det);
    InverseMat[1][2] = static_cast<float>((a02 * a10 - a00 * a12) * inv_det);
    InverseMat[1][3] = 0.0f;

    // Third row of inverse
    InverseMat[2][0] = static_cast<float>((a10 * a21 - a11 * a20) * inv_det);
    InverseMat[2][1] = static_cast<float>((a01 * a20 - a00 * a21) * inv_det);
    InverseMat[2][2] = static_cast<float>((a00 * a11 - a01 * a10) * inv_det);
    InverseMat[2][3] = 0.0f;

    // Calculate inverse translation: -R^(-1) * T
    // Transform the original translation vector by the inverse rotation matrix
    const float tx = Mat[3][0];
    const float ty = Mat[3][1];
    const float tz = Mat[3][2];

    InverseMat[3][0] = -(InverseMat[0][0] * tx + InverseMat[1][0] * ty + InverseMat[2][0] * tz);
    InverseMat[3][1] = -(InverseMat[0][1] * tx + InverseMat[1][1] * ty + InverseMat[2][1] * tz);
    InverseMat[3][2] = -(InverseMat[0][2] * tx + InverseMat[1][2] * ty + InverseMat[2][2] * tz);
    InverseMat[3][3] = 1.0f;
}

float Vx3DMatrixDeterminant(const VxMatrix &Mat) {
    return (Mat[1][1] * Mat[2][2] - Mat[2][1] * Mat[1][2]) * Mat[0][0]
         - (Mat[1][0] * Mat[2][2] - Mat[2][0] * Mat[1][2]) * Mat[0][1]
         + (Mat[2][1] * Mat[1][0] - Mat[2][0] * Mat[1][1]) * Mat[0][2];
}

void Vx3DMatrixFromRotation(VxMatrix &ResultMat, const VxVector &Vector, float Angle) {
    float c = cosf(Angle);
    float s = sinf(Angle);
    float t = 1.0f - c;

    // Normalize the vector
    float len = Vector.Magnitude();
    float x, y, z;

    if (len > EPSILON) {
        float invLen = 1.0f / len;
        x = Vector.x * invLen;
        y = Vector.y * invLen;
        z = Vector.z * invLen;
    } else {
        // Default to Z axis if vector is too small
        x = 0.0f;
        y = 0.0f;
        z = 1.0f;
    }

    // Compute rotation matrix elements using Rodrigues' rotation formula
    float xx = x * x;
    float yy = y * y;
    float zz = z * z;
    float xy = x * y;
    float xz = x * z;
    float yz = y * z;
    float xs = x * s;
    float ys = y * s;
    float zs = z * s;

    ResultMat[0][0] = xx * t + c;
    ResultMat[0][1] = xy * t + zs;
    ResultMat[0][2] = xz * t - ys;
    ResultMat[0][3] = 0.0f;

    ResultMat[1][0] = xy * t - zs;
    ResultMat[1][1] = yy * t + c;
    ResultMat[1][2] = yz * t + xs;
    ResultMat[1][3] = 0.0f;

    ResultMat[2][0] = xz * t + ys;
    ResultMat[2][1] = yz * t - xs;
    ResultMat[2][2] = zz * t + c;
    ResultMat[2][3] = 0.0f;

    ResultMat[3][0] = 0.0f;
    ResultMat[3][1] = 0.0f;
    ResultMat[3][2] = 0.0f;
    ResultMat[3][3] = 1.0f;
}

void Vx3DMatrixFromRotationAndOrigin(VxMatrix &ResultMat, const VxVector &Vector, const VxVector &Origin, float Angle) {
    // Create rotation matrix
    Vx3DMatrixFromRotation(ResultMat, Vector, Angle);

    // Apply translation to rotate around origin
    // The formula is: T(origin) * R * T(-origin)
    // But we can optimize this by directly computing the final translation

    // First, rotate the negative origin vector
    VxVector rotatedNegOrigin;
    VxVector negOrigin = -Origin;
    Vx3DRotateVector(&rotatedNegOrigin, ResultMat, &negOrigin);

    // The final translation is: origin + rotated(-origin)
    ResultMat[3][0] = Origin.x + rotatedNegOrigin.x;
    ResultMat[3][1] = Origin.y + rotatedNegOrigin.y;
    ResultMat[3][2] = Origin.z + rotatedNegOrigin.z;
}

void Vx3DMatrixFromEulerAngles(VxMatrix &Mat, float eax, float eay, float eaz) {
    float cx = cosf(eax);
    float sx = sinf(eax);
    float cy = cosf(eay);
    float sy = sinf(eay);
    float cz = cosf(eaz);
    float sz = sinf(eaz);

    // Small angle optimizations
    const float SMALL_ANGLE_THRESHOLD = 1e-10f;
    if (fabs(eax) <= SMALL_ANGLE_THRESHOLD) {
        cx = 1.0f;
        sx = 0.0f;
    }
    if (fabs(eay) <= SMALL_ANGLE_THRESHOLD) {
        cy = 1.0f;
        sy = 0.0f;
    }
    if (fabs(eaz) <= SMALL_ANGLE_THRESHOLD) {
        cz = 1.0f;
        sz = 0.0f;
    }

    // Rotation order: Z, Y, X (typical convention)
    Mat[0][0] = cy * cz;
    Mat[0][1] = cy * sz;
    Mat[0][2] = -sy;
    Mat[0][3] = 0.0f;

    Mat[1][0] = sx * sy * cz - cx * sz;
    Mat[1][1] = sx * sy * sz + cx * cz;
    Mat[1][2] = sx * cy;
    Mat[1][3] = 0.0f;

    Mat[2][0] = cx * sy * cz + sx * sz;
    Mat[2][1] = cx * sy * sz - sx * cz;
    Mat[2][2] = cx * cy;
    Mat[2][3] = 0.0f;

    Mat[3][0] = 0.0f;
    Mat[3][1] = 0.0f;
    Mat[3][2] = 0.0f;
    Mat[3][3] = 1.0f;
}

void Vx3DMatrixToEulerAngles(const VxMatrix &Mat, float *eax, float *eay, float *eaz) {
    // Compute the magnitude to detect gimbal lock
    float magnitude = sqrtf(Mat[0][0] * Mat[0][0] + Mat[0][1] * Mat[0][1]);

    // Check for gimbal lock
    if (magnitude < EPSILON) {
        // Gimbal lock detected
        *eay = atan2f(-Mat[0][2], magnitude);
        *eax = atan2f(-Mat[2][1], Mat[1][1]);
        *eaz = 0.0f;
    } else {
        // No gimbal lock
        *eay = atan2f(-Mat[0][2], magnitude);
        *eax = atan2f(Mat[1][2], Mat[2][2]);
        *eaz = atan2f(Mat[0][1], Mat[0][0]);
    }
}

void Vx3DInterpolateMatrix(float step, VxMatrix &Res, const VxMatrix &A, const VxMatrix &B) {
    // Extract components from matrices A and B
    VxQuaternion quatA, quatB;
    VxVector posA, posB, scaleA, scaleB;
    VxQuaternion uRotA, uRotB;

    Vx3DDecomposeMatrixTotal(A, quatA, posA, scaleA, uRotA);
    Vx3DDecomposeMatrixTotal(B, quatB, posB, scaleB, uRotB);

    // Interpolate components
    VxQuaternion quatRes = Slerp(step, quatA, quatB);
    VxVector posRes = Interpolate(step, posA, posB);
    VxVector scaleRes = Interpolate(step, scaleA, scaleB);
    VxQuaternion uRotRes = Slerp(step, uRotA, uRotB);

    // Reconstruct matrix
    Res.SetIdentity();

    // Apply scale
    VxMatrix scaleMat;
    scaleMat.SetIdentity();
    scaleMat[0][0] = scaleRes.x;
    scaleMat[1][1] = scaleRes.y;
    scaleMat[2][2] = scaleRes.z;

    // Apply rotation
    VxMatrix rotMat;
    quatRes.ToMatrix(rotMat);

    // Apply stretch rotation
    VxMatrix uRotMat;
    uRotRes.ToMatrix(uRotMat);

    // Combine transformations: R * U * S
    VxMatrix temp1, temp2;
    Vx3DMultiplyMatrix(temp1, uRotMat, scaleMat);
    Vx3DMultiplyMatrix(temp2, rotMat, temp1);

    // Copy the resulting matrix and set the translation
    Res = temp2;
    Res[3][0] = posRes.x;
    Res[3][1] = posRes.y;
    Res[3][2] = posRes.z;
}

void Vx3DInterpolateMatrixNoScale(float step, VxMatrix &Res, const VxMatrix &A, const VxMatrix &B) {
    // Extract components from matrices A and B
    VxQuaternion quatA, quatB;
    VxVector posA, posB, scaleA, scaleB;

    Vx3DDecomposeMatrix(A, quatA, posA, scaleA);
    Vx3DDecomposeMatrix(B, quatB, posB, scaleB);

    // Interpolate components (ignoring scale)
    VxQuaternion quatRes = Slerp(step, quatA, quatB);
    VxVector posRes = Interpolate(step, posA, posB);

    // Reconstruct matrix
    quatRes.ToMatrix(Res);

    // Set translation
    Res[3][0] = posRes.x;
    Res[3][1] = posRes.y;
    Res[3][2] = posRes.z;
}

void Vx3DMultiplyMatrixVectorStrided(VxStridedData *Dest, VxStridedData *Src, const VxMatrix &Mat, int count) {
    if (!Dest || !Src || !Dest->Ptr || !Src->Ptr) return;

    for (int i = 0; i < count; i++) {
        const VxVector *srcVec = reinterpret_cast<const VxVector *>(reinterpret_cast<const char *>(Src->Ptr) + i * Src->
            Stride);
        VxVector *destVec = reinterpret_cast<VxVector *>(reinterpret_cast<char *>(Dest->Ptr) + i * Dest->Stride);
        Vx3DMultiplyMatrixVector(destVec, Mat, srcVec);
    }
}

void Vx3DMultiplyMatrixVector4Strided(VxStridedData *Dest, VxStridedData *Src, const VxMatrix &Mat, int count) {
    if (!Dest || !Src || !Dest->Ptr || !Src->Ptr) return;

    for (int i = 0; i < count; i++) {
        const VxVector4 *srcVec = reinterpret_cast<const VxVector4 *>(reinterpret_cast<const char *>(Src->Ptr) + i * Src
            ->Stride);
        VxVector4 *destVec = reinterpret_cast<VxVector4 *>(reinterpret_cast<char *>(Dest->Ptr) + i * Dest->Stride);
        Vx3DMultiplyMatrixVector4(destVec, Mat, srcVec);
    }
}

void Vx3DRotateVectorStrided(VxStridedData *Dest, VxStridedData *Src, const VxMatrix &Mat, int count) {
    if (!Dest || !Src || !Dest->Ptr || !Src->Ptr) return;

    for (int i = 0; i < count; i++) {
        const VxVector *srcVec = reinterpret_cast<const VxVector *>(reinterpret_cast<const char *>(Src->Ptr) + i * Src->
            Stride);
        VxVector *destVec = reinterpret_cast<VxVector *>(reinterpret_cast<char *>(Dest->Ptr) + i * Dest->Stride);
        Vx3DRotateVector(destVec, Mat, srcVec);
    }
}

void Vx3DMatrixAdjoint(const VxMatrix &in, VxMatrix &out) {
    out[0][0] = in[1][1] * in[2][2] - in[1][2] * in[2][1];
    out[1][0] = in[1][2] * in[2][0] - in[1][0] * in[2][2];
    out[2][0] = in[1][0] * in[2][1] - in[1][1] * in[2][0];

    out[0][1] = in[0][2] * in[2][1] - in[0][1] * in[2][2];
    out[1][1] = in[0][0] * in[2][2] - in[0][2] * in[2][0];
    out[2][1] = in[0][1] * in[2][0] - in[0][0] * in[2][1];

    out[0][2] = in[0][1] * in[1][2] - in[0][2] * in[1][1];
    out[1][2] = in[0][2] * in[1][0] - in[0][0] * in[1][2];
    out[2][2] = in[0][0] * in[1][1] - in[0][1] * in[1][0];
}

float Vx3DMatrixNorm(const VxMatrix &M, bool isOneNorm) {
    float maxNorm = 0.0f;
    for (int j = 0; j < 3; ++j) {
        float sum = 0.0f;
        for (int i = 0; i < 3; ++i) {
            sum += fabsf(isOneNorm ? M[i][j] : M[j][i]);
        }
        if (sum > maxNorm) {
            maxNorm = sum;
        }
    }
    return maxNorm;
}

float Vx3DMatrixPolarDecomposition(const VxMatrix &M_in, VxMatrix &Q, VxMatrix &S) {
    VxMatrix E;
    Vx3DTransposeMatrix(E, M_in);
    E[0][3] = E[1][3] = E[2][3] = E[3][0] = E[3][1] = E[3][2] = 0.0f;
    E[3][3] = 1.0f;

    float E_one_norm = Vx3DMatrixNorm(E, true);
    float E_inf_norm = Vx3DMatrixNorm(E, false);

    for (int iter = 0; iter < 20; ++iter) {
        VxMatrix E_adj;
        Vx3DMatrixAdjoint(E, E_adj);

        float det_E = DotProduct(VxVector(E[0][0], E[0][1], E[0][2]), VxVector(E_adj[0][0], E_adj[0][1], E_adj[0][2]));

        if (fabsf(det_E) < 1e-12f) break;

        float E_adj_one_norm = Vx3DMatrixNorm(E_adj, true);
        float E_adj_inf_norm = Vx3DMatrixNorm(E_adj, false);

        float gamma = sqrtf(sqrtf((E_adj_one_norm * E_adj_inf_norm) / (E_one_norm * E_inf_norm)) / fabsf(det_E));
        float c1 = 0.5f * gamma;
        float c2 = 0.5f / (gamma * det_E);

        VxMatrix E_next;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                E_next[i][j] = c1 * E[i][j] + c2 * E_adj[j][i]; // Adjoint is already transposed form
            }

        VxMatrix E_diff;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                E_diff[i][j] = E_next[i][j] - E[i][j];

        E = E_next;
        if (Vx3DMatrixNorm(E_diff, true) < E_one_norm * 1e-6f) break;

        E_one_norm = Vx3DMatrixNorm(E, true);
        E_inf_norm = Vx3DMatrixNorm(E, false);
    }

    Vx3DTransposeMatrix(Q, E);
    VxMatrix Q_T;
    Vx3DTransposeMatrix(Q_T, Q);
    Vx3DMultiplyMatrix(S, Q_T, M_in);

    // Symmetrize S
    for (int i = 0; i < 3; ++i)
        for (int j = i; j < 3; ++j) {
            float val = 0.5f * ((S)[i][j] + (S)[j][i]);
            (S)[i][j] = val;
            (S)[j][i] = val;
        }

    for (int i = 0; i < 3; ++i) {
        Q[i][3] = Q[3][i] = 0.0f;
        S[i][3] = S[3][i] = 0.0f;
    }
    Q[3][3] = 1.0f;
    S[3][3] = 1.0f;

    return Vx3DMatrixDeterminant(Q);
}

VxVector Vx3DMatrixSpectralDecomposition(const VxMatrix &S_in, VxMatrix &U_out) {
    U_out.SetIdentity();

    float d[3] = {S_in[0][0], S_in[1][1], S_in[2][2]};
    float o[3] = {S_in[0][1], S_in[0][2], S_in[1][2]};

    for (int sweep = 0; sweep < 20; ++sweep) {
        float sum_off_diag = fabsf(o[0]) + fabsf(o[1]) + fabsf(o[2]);
        if (sum_off_diag < 1e-9f) break;

        // Iterate through off-diagonal pairs (0,1), (0,2), (1,2)
        const int p_map[3] = {0, 0, 1};
        const int q_map[3] = {1, 2, 2};
        for (int i = 0; i < 3; ++i) {
            int p = p_map[i];
            int q = q_map[i];

            float S_pq = (p == 0 && q == 1) ? o[0] : ((p == 0 && q == 2) ? o[1] : o[2]);
            if (fabsf(S_pq) < 1e-9f) continue;

            float diff = d[q] - d[p];
            float t;
            if (fabsf(diff) + fabsf(S_pq) * 100.0f == fabsf(diff)) {
                t = S_pq / diff;
            } else {
                float theta = diff / (2.0f * S_pq);
                t = 1.0f / (fabsf(theta) + sqrtf(theta * theta + 1.0f));
                if (theta < 0.0f) t = -t;
            }
            float c = 1.0f / sqrtf(1.0f + t * t);
            float s = t * c;
            float tau = s / (1.0f + c);
            float h = t * S_pq;

            d[p] -= h;
            d[q] += h;

            // Update other off-diagonal elements
            int r = 3 - p - q; // The remaining index
            float S_pr = (p == 0 && r == 1) ? o[0] : ((p == 0 && r == 2) ? o[1] : o[2]);
            if (p > r) S_pr = (r == 0 && p == 1) ? o[0] : ((r == 0 && p == 2) ? o[1] : o[2]);
            float S_qr = (q == 0 && r == 1) ? o[0] : ((q == 0 && r == 2) ? o[1] : o[2]);
            if (q > r) S_qr = (r == 0 && q == 1) ? o[0] : ((r == 0 && q == 2) ? o[1] : o[2]);

            float next_S_pr = S_pr - s * (S_qr + S_pr * tau);
            float next_S_qr = S_qr + s * (S_pr - S_qr * tau);
            if (p == 0 && r == 1) o[0] = next_S_pr;
            else if (p == 0 && r == 2) o[1] = next_S_pr;
            else o[2] = next_S_pr;
            if (q == 0 && r == 1) o[0] = next_S_qr;
            else if (q == 0 && r == 2) o[1] = next_S_qr;
            else o[2] = next_S_qr;
            if (i == 0) o[0] = 0;
            if (i == 1) o[1] = 0;
            if (i == 2) o[2] = 0;


            // Update eigenvector matrix U
            for (int k = 0; k < 3; ++k) {
                float g = U_out[k][p];
                float h_u = U_out[k][q];
                U_out[k][p] = g - s * (h_u + g * tau);
                U_out[k][q] = h_u + s * (g - h_u * tau);
            }
        }
    }

    return VxVector(d[0], d[1], d[2]);
}

void Vx3DTransposeMatrix(VxMatrix &Result, const VxMatrix &A) {
    // Use a temporary matrix in case Result is the same as A
    VxMatrix temp;

    temp[0][0] = A[0][0];
    temp[0][1] = A[1][0];
    temp[0][2] = A[2][0];
    temp[0][3] = A[3][0];
    temp[1][0] = A[0][1];
    temp[1][1] = A[1][1];
    temp[1][2] = A[2][1];
    temp[1][3] = A[3][1];
    temp[2][0] = A[0][2];
    temp[2][1] = A[1][2];
    temp[2][2] = A[2][2];
    temp[2][3] = A[3][2];
    temp[3][0] = A[0][3];
    temp[3][1] = A[1][3];
    temp[3][2] = A[2][3];
    temp[3][3] = A[3][3];

    Result = temp;
}

void Vx3DDecomposeMatrix(const VxMatrix &A, VxQuaternion &Quat, VxVector &Pos, VxVector &Scale) {
    // Extract position from translation part
    Pos = VxVector(A[3][0], A[3][1], A[3][2]);

    // Copy matrix to working copy
    VxMatrix Mat = A;

    // Extract quaternion from the matrix copy
    Quat.FromMatrix(Mat, FALSE, FALSE);

    // Calculate scale as dot products of normalized rows with original rows
    Scale.x = Mat[0][0] * A[0][0] + Mat[0][1] * A[0][1] + Mat[0][2] * A[0][2];
    Scale.y = Mat[1][0] * A[1][0] + Mat[1][1] * A[1][1] + Mat[1][2] * A[1][2];
    Scale.z = Mat[2][0] * A[2][0] + Mat[2][1] * A[2][1] + Mat[2][2] * A[2][2];
}

float Vx3DDecomposeMatrixTotal(const VxMatrix &A, VxQuaternion &Quat, VxVector &Pos, VxVector &Scale, VxQuaternion &URot) {
    // Extract position
    Pos = VxVector(A[3][0], A[3][1], A[3][2]);

    VxMatrix Q, S; // For polar decomposition
    float det = Vx3DMatrixPolarDecomposition(A, Q, S);
    if (det < 0.0f) {
        // Flip Q matrix if determinant is negative
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                Q[i][j] = -Q[i][j];
            }
        }
        det = -1.0f;
    } else {
        det = 1.0f;
    }

    // Extract rotation quaternion from Q
    Quat = Vx3DQuaternionFromMatrix(Q);

    // Perform spectral decomposition on S to get scale and U rotation
    VxMatrix U;
    Scale = Vx3DMatrixSpectralDecomposition(S, U);

    // Extract U rotation quaternion
    URot = Vx3DQuaternionFromMatrix(U);

    // Apply snuggle correction
    VxQuaternion snuggleQuat = Vx3DQuaternionSnuggle(&URot, &Scale);
    URot = Vx3DQuaternionMultiply(URot, snuggleQuat);

    return det;
}

float Vx3DDecomposeMatrixTotalPtr(const VxMatrix &A, VxQuaternion *Quat, VxVector *Pos, VxVector *Scale, VxQuaternion *URot) {
    // Extract position if requested
    if (Pos) {
        *Pos = VxVector(A[3][0], A[3][1], A[3][2]);
    }

    // If nothing else is requested, return early
    if (!Quat && !Scale && !URot) {
        return 1.0f;
    }

    VxMatrix Q, S; // For polar decomposition
    float det = Vx3DMatrixPolarDecomposition(A, Q, S);

    if (det < 0.0f) {
        // Flip Q matrix if determinant is negative
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                Q[i][j] = -Q[i][j];
            }
        }
        det = -1.0f;
    } else {
        det = 1.0f;
    }

    // Extract rotation quaternion from Q if requested
    if (Quat) {
        *Quat = Vx3DQuaternionFromMatrix(Q);
    }

    // Handle scale and URot if requested
    if (Scale || URot) {
        VxMatrix U;
        VxVector tempScale = Vx3DMatrixSpectralDecomposition(S, U);
        VxQuaternion tempURot = Vx3DQuaternionFromMatrix(U);

        // Apply snuggle correction
        VxQuaternion snuggleQuat = Vx3DQuaternionSnuggle(&tempURot, &tempScale);

        if (URot) {
            *URot = Vx3DQuaternionMultiply(tempURot, snuggleQuat);
        }

        if (Scale) {
            *Scale = tempScale;
        }
    }

    return det;
}
