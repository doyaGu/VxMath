#ifndef VXMATRIX_H
#define VXMATRIX_H

#include "VxQuaternion.h"
#include "VxVector.h"
#include "XUtil.h"

class VxMatrix;
struct Vx2DVector;
struct VxStridedData;

/// @name Matrix Utility Functions
/// @{

/**
 * @brief Sets a matrix to the identity matrix.
 * @param Mat The matrix to be set to identity.
 */
inline void Vx3DMatrixIdentity(VxMatrix &Mat);

/**
 * @brief Transforms a 3D vector by a matrix (assumes w=1 for input, performs perspective divide).
 * @param ResultVector Pointer to store the resulting transformed vector.
 * @param Mat The transformation matrix.
 * @param Vector Pointer to the input vector to transform.
 */
inline void Vx3DMultiplyMatrixVector(VxVector *ResultVector, const VxMatrix &Mat, const VxVector *Vector);

/**
 * @brief Transforms an array of 3D vectors by a matrix.
 * @param ResultVectors Pointer to the output array of transformed vectors.
 * @param Mat The transformation matrix.
 * @param Vectors Pointer to the input array of vectors.
 * @param count The number of vectors to transform.
 * @param stride The byte offset between consecutive vectors in the input and output arrays.
 */
inline void Vx3DMultiplyMatrixVectorMany(VxVector *ResultVectors, const VxMatrix &Mat, const VxVector *Vectors, int count, int stride);

/**
 * @brief Transforms a 4D vector by a matrix.
 * @param ResultVector Pointer to store the resulting transformed 4D vector.
 * @param Mat The transformation matrix.
 * @param Vector Pointer to the input 4D vector.
 */
inline void Vx3DMultiplyMatrixVector4(VxVector4 *ResultVector, const VxMatrix &Mat, const VxVector4 *Vector);

/**
 * @brief Transforms a 3D vector by a matrix, returning a 4D vector (w component is calculated).
 * @param ResultVector Pointer to store the resulting transformed 4D vector.
 * @param Mat The transformation matrix.
 * @param Vector Pointer to the input 3D vector (w is assumed to be 1).
 */
inline void Vx3DMultiplyMatrixVector4(VxVector4 *ResultVector, const VxMatrix &Mat, const VxVector *Vector);

/**
 * @brief Rotates a 3D vector by a matrix, ignoring the translation part of the matrix.
 * @param ResultVector Pointer to store the resulting rotated vector.
 * @param Mat The rotation matrix.
 * @param Vector Pointer to the input vector to rotate.
 */
inline void Vx3DRotateVector(VxVector *ResultVector, const VxMatrix &Mat, const VxVector *Vector);

/**
 * @brief Rotates an array of 3D vectors by a matrix.
 * @param ResultVector Pointer to the output array of rotated vectors.
 * @param Mat The rotation matrix.
 * @param Vector Pointer to the input array of vectors.
 * @param count The number of vectors to rotate.
 * @param stride The byte offset between consecutive vectors.
 */
inline void Vx3DRotateVectorMany(VxVector *ResultVector, const VxMatrix &Mat, const VxVector *Vector, int count, int stride);

/**
 * @brief Multiplies two affine 3D transformation matrices.
 * @param ResultMat The resulting matrix (MatA * MatB).
 * @param MatA The first matrix operand.
 * @param MatB The second matrix operand.
 * @remarks This function enforces an affine output matrix by forcing the last column to [0,0,0,1].
 * Use `Vx3DMultiplyMatrix4()` when projective terms must be preserved.
 */
inline void Vx3DMultiplyMatrix(VxMatrix &ResultMat, const VxMatrix &MatA, const VxMatrix &MatB);

/**
 * @brief Multiplies two full 4x4 matrices.
 * @param ResultMat The resulting matrix (MatA * MatB).
 * @param MatA The first matrix operand.
 * @param MatB The second matrix operand.
 */
inline void Vx3DMultiplyMatrix4(VxMatrix &ResultMat, const VxMatrix &MatA, const VxMatrix &MatB);

/**
 * @brief Computes the inverse of a given 4x4 matrix.
 * @param InverseMat The resulting inverse matrix.
 * @param Mat The matrix to invert.
 */
inline void Vx3DInverseMatrix(VxMatrix &InverseMat, const VxMatrix &Mat);

/**
 * @brief Computes the determinant of a 4x4 matrix.
 * @param Mat The input matrix.
 * @return The determinant of the matrix.
 */
inline float Vx3DMatrixDeterminant(const VxMatrix &Mat);

/**
 * @brief Creates a rotation matrix from an axis and an angle.
 * @param ResultMat The resulting rotation matrix.
 * @param Vector The axis of rotation (should be normalized).
 * @param Angle The angle of rotation in radians.
 */
inline void Vx3DMatrixFromRotation(VxMatrix &ResultMat, const VxVector &Vector, float Angle);

/**
 * @brief Creates a transformation matrix that rotates around a specific origin point.
 * @param ResultMat The resulting transformation matrix.
 * @param Vector The axis of rotation (should be normalized).
 * @param Origin The point to rotate around.
 * @param Angle The angle of rotation in radians.
 */
inline void Vx3DMatrixFromRotationAndOrigin(VxMatrix &ResultMat, const VxVector &Vector, const VxVector &Origin, float Angle);

/**
 * @brief Creates a rotation matrix from Euler angles.
 * @param Mat The resulting rotation matrix.
 * @param eax The rotation around the X-axis (pitch) in radians.
 * @param eay The rotation around the Y-axis (yaw) in radians.
 * @param eaz The rotation around the Z-axis (roll) in radians.
 */
inline void Vx3DMatrixFromEulerAngles(VxMatrix &Mat, float eax, float eay, float eaz);

/**
 * @brief Extracts Euler angles from a rotation matrix.
 * @param Mat The input rotation matrix.
 * @param eax Pointer to store the X-axis rotation (pitch) in radians.
 * @param eay Pointer to store the Y-axis rotation (yaw) in radians.
 * @param eaz Pointer to store the Z-axis rotation (roll) in radians.
 */
inline void Vx3DMatrixToEulerAngles(const VxMatrix &Mat, float *eax, float *eay, float *eaz);

/**
 * @brief Performs a linear interpolation between two matrices.
 * @param step The interpolation factor (0.0 for matrix A, 1.0 for matrix B).
 * @param Res The resulting interpolated matrix.
 * @param A The first matrix.
 * @param B The second matrix.
 */
inline void Vx3DInterpolateMatrix(float step, VxMatrix &Res, const VxMatrix &A, const VxMatrix &B);

/**
 * @brief Interpolates between two matrices, handling rotation and translation separately to avoid scaling issues.
 * @param step The interpolation factor (0.0 for matrix A, 1.0 for matrix B).
 * @param Res The resulting interpolated matrix.
 * @param A The first matrix.
 * @param B The second matrix.
 */
inline void Vx3DInterpolateMatrixNoScale(float step, VxMatrix &Res, const VxMatrix &A, const VxMatrix &B);

/**
 * @brief Transforms an array of vectors with a specific stride by a matrix.
 * @param Dest Pointer to the destination strided data structure.
 * @param Src Pointer to the source strided data structure.
 * @param Mat The transformation matrix.
 * @param count The number of vectors to transform.
 */
inline void Vx3DMultiplyMatrixVectorStrided(VxStridedData *Dest, VxStridedData *Src, const VxMatrix &Mat, int count);

/**
 * @brief Transforms an array of 4D vectors with a specific stride by a matrix.
 * @param Dest Pointer to the destination strided data structure.
 * @param Src Pointer to the source strided data structure.
 * @param Mat The transformation matrix.
 * @param count The number of vectors to transform.
 */
inline void Vx3DMultiplyMatrixVector4Strided(VxStridedData *Dest, VxStridedData *Src, const VxMatrix &Mat, int count);

/**
 * @brief Rotates an array of vectors with a specific stride by a matrix.
 * @param Dest Pointer to the destination strided data structure.
 * @param Src Pointer to the source strided data structure.
 * @param Mat The rotation matrix.
 * @param count The number of vectors to rotate.
 */
inline void Vx3DRotateVectorStrided(VxStridedData *Dest, VxStridedData *Src, const VxMatrix &Mat, int count);

/**
 * @brief Computes the transpose of a 4x4 matrix.
 * @param Result The resulting transposed matrix.
 * @param A The input matrix.
 */
inline void Vx3DTransposeMatrix(VxMatrix &Result, const VxMatrix &A);

/**
 * @brief Decomposes a transformation matrix into its scale, rotation, and translation components.
 * @param A The input transformation matrix.
 * @param Quat The output rotation as a quaternion.
 * @param Pos The output translation vector.
 * @param Scale The output scaling vector.
 */
inline void Vx3DDecomposeMatrix(const VxMatrix &A, VxQuaternion &Quat, VxVector &Pos, VxVector &Scale);

/**
 * @brief Decomposes a transformation matrix, including an additional rotation for handling shear or complex transforms.
 * @param A The input transformation matrix.
 * @param Quat The output primary rotation quaternion.
 * @param Pos The output translation vector.
 * @param Scale The output scaling vector.
 * @param URot The output secondary rotation quaternion.
 * @return A metric representing the overall scaling factor.
 */
inline float Vx3DDecomposeMatrixTotal(const VxMatrix &A, VxQuaternion &Quat, VxVector &Pos, VxVector &Scale, VxQuaternion &URot);

/**
 * @brief Decomposes a transformation matrix into components, using pointers for optional outputs.
 * @param A The input transformation matrix.
 * @param Quat Pointer to store the output rotation quaternion (can be NULL).
 * @param Pos Pointer to store the output translation vector (can be NULL).
 * @param Scale Pointer to store the output scaling vector (can be NULL).
 * @param URot Pointer to store the output secondary rotation quaternion (can be NULL).
 * @return A metric representing the overall scaling factor.
 */
inline float Vx3DDecomposeMatrixTotalPtr(const VxMatrix &A, VxQuaternion *Quat, VxVector *Pos, VxVector *Scale, VxQuaternion *URot);
/// @}

/**
 * @brief A class representing a 4x4 matrix.
 */
class VxMatrix {
public:
    /// @brief Default constructor. Does not initialize the matrix.
    VxMatrix() : m_Data() {}

    /**
     * @brief Constructs a matrix from a 2D float array.
     * @param m A 4x4 array of floats to initialize the matrix with.
     */
    VxMatrix(float m[4][4]) { memcpy(m_Data, m, sizeof(VxMatrix)); }

    /**
     * @brief Returns a reference to a static identity matrix.
     * @return A const reference to the identity matrix.
     */
    static const VxMatrix &Identity() {
        static const VxMatrix s = []{ VxMatrix m; m.SetIdentity(); return m; }();
        return s;
    }

    // Matrix construction
    /// @brief Sets all elements of the matrix to zero.
    void Clear() { memset(m_Data, 0, sizeof(VxMatrix)); }
    /// @brief Sets this matrix to the identity matrix.
    void SetIdentity();

    /**
     * @brief Constructs an orthographic projection matrix.
     * @param Zoom The zoom factor for both X and Y axes.
     * @param Aspect The aspect ratio (width / height).
     * @param Near_plane The distance to the near clipping plane.
     * @param Far_plane The distance to the far clipping plane.
     * @remarks The resulting matrix is:
     * @code
     *   F = Far_plane, N = Near_plane
     *
     *   [ Zoom      0           0         0 ]
     *   [ 0         Zoom*Aspect 0         0 ]
     *   [ 0         0           1/(F-N)   0 ]
     *   [ 0         0           -N/(F-N)  1 ]
     * @endcode
     * @see Perspective, OrthographicRect
     */
    void Orthographic(float Zoom, float Aspect, float Near_plane, float Far_plane);

    /**
     * @brief Constructs a perspective projection matrix.
     * @param Fov The vertical field of view, in radians.
     * @param Aspect The aspect ratio (width / height).
     * @param Near_plane The distance to the near clipping plane.
     * @param Far_plane The distance to the far clipping plane.
     * @remarks The resulting matrix is:
     * @code
     *   A = cot(Fov/2), F = Far_plane, N = Near_plane
     *
     *   [ A         0           0         0 ]
     *   [ 0         A*Aspect    0         0 ]
     *   [ 0         0           F/(F-N)   1 ]
     *   [ 0         0           -F*N/(F-N) 0 ]
     * @endcode
     * @see Orthographic, PerspectiveRect
     */
    void Perspective(float Fov, float Aspect, float Near_plane, float Far_plane);

    /**
     * @brief Constructs an orthographic projection matrix from a view rectangle.
     * @param Left The coordinate for the left vertical clipping plane.
     * @param Right The coordinate for the right vertical clipping plane.
     * @param Top The coordinate for the top horizontal clipping plane.
     * @param Bottom The coordinate for the bottom horizontal clipping plane.
     * @param Near_plane The distance to the near clipping plane.
     * @param Far_plane The distance to the far clipping plane.
     * @see PerspectiveRect, Orthographic
     */
    void OrthographicRect(float Left, float Right, float Top, float Bottom, float Near_plane, float Far_plane);

    /**
     * @brief Constructs a perspective projection matrix from a view rectangle.
     * @param Left The coordinate for the left vertical clipping plane on the near plane.
     * @param Right The coordinate for the right vertical clipping plane on the near plane.
     * @param Top The coordinate for the top horizontal clipping plane on the near plane.
     * @param Bottom The coordinate for the bottom horizontal clipping plane on the near plane.
     * @param Near_plane The distance to the near clipping plane.
     * @param Far_plane The distance to the far clipping plane.
     * @see Perspective, OrthographicRect
     */
    void PerspectiveRect(float Left, float Right, float Top, float Bottom, float Near_plane, float Far_plane);

    // operators
    /// @brief Provides const access to a row of the matrix.
    const VxVector4 &operator[](int i) const { return (const VxVector4 &) (*(VxVector4 *) (m_Data + i)); }
    /// @brief Provides mutable access to a row of the matrix.
    VxVector4 &operator[](int i) { return (VxVector4 &) (*(VxVector4 *) (m_Data + i)); }

    /// @brief Implicitly converts the matrix to a const void pointer to its data.
    operator const void *() const { return &m_Data[0]; }
    /// @brief Implicitly converts the matrix to a void pointer to its data.
    operator void *() { return &m_Data[0]; }

    /**
     * @brief Compares two matrices for equality.
     * @remarks This operator compares the contents of the matrices.
     * @return TRUE if the matrices have the same contents, FALSE otherwise.
     */
    XBOOL operator==(const VxMatrix &mat) const
    {
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                if (m_Data[i][j] != mat.m_Data[i][j]) {
                    return FALSE;
                }
            }
        }
        return TRUE;
    }

    /**
     * @brief Compares two matrices for inequality.
     * @remarks This operator compares the contents of the matrices.
     * @return TRUE if the matrices have different contents, FALSE otherwise.
     */
    XBOOL operator!=(const VxMatrix &mat) const { return !(*this == mat); }

    /**
     * @brief Multiplies this matrix by another matrix and assigns the result to this matrix.
     * @param mat The matrix to multiply with.
     * @return A reference to this modified matrix.
     * @remarks Uses affine multiplication (`Vx3DMultiplyMatrix`).
     */
    VxMatrix &operator*=(const VxMatrix &mat) {
        VxMatrix tmp = *this;
        Vx3DMultiplyMatrix(*this, tmp, mat);
        return *this;
    }

    /**
     * @brief Multiplies this matrix by another matrix.
     * @param iMat The matrix to multiply with.
     * @return The resulting new matrix.
     * @remarks Uses affine multiplication (`Vx3DMultiplyMatrix`).
     */
    VxMatrix operator*(const VxMatrix &iMat) const {
        VxMatrix temp;
        Vx3DMultiplyMatrix(temp, *this, iMat);
        return temp;
    }

    /// @brief Transforms a 3D vector by this matrix.
    friend VxVector operator*(const VxMatrix &m, const VxVector &v);
    /// @brief Transforms a 4D vector by this matrix.
    friend VxVector4 operator*(const VxMatrix &m, const VxVector4 &v);
    /// @brief Transforms a 3D vector by this matrix.
    friend VxVector operator*(const VxVector &v, const VxMatrix &m);
    /// @brief Transforms a 4D vector by this matrix.
    friend VxVector4 operator*(const VxVector4 &v, const VxMatrix &m);

protected:
    float m_Data[4][4]; ///< The 16 floating-point values of the matrix.
};

//////////////////////////////////////////////////////////////////////
////////////////////////// Matrix operations /////////////////////////
//////////////////////////////////////////////////////////////////////

inline void VxMatrix::SetIdentity() {
    m_Data[0][1] = m_Data[0][2] = m_Data[0][3] =
    m_Data[1][0] = m_Data[1][2] = m_Data[1][3] =
    m_Data[2][0] = m_Data[2][1] = m_Data[2][3] =
    m_Data[3][0] = m_Data[3][1] = m_Data[3][2] = 0;
    m_Data[0][0] = m_Data[1][1] = m_Data[2][2] = m_Data[3][3] = 1.0f;
}

inline VxVector operator*(const VxVector &v, const VxMatrix &m) {
    return VxVector(m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z + m[3][0],
                    m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z + m[3][1],
                    m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z + m[3][2]);
}

inline VxVector operator*(const VxMatrix &m, const VxVector &v) {
    return VxVector(m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z + m[3][0],
                    m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z + m[3][1],
                    m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z + m[3][2]);
}

inline VxVector4 operator*(const VxMatrix &m, const VxVector4 &v) {
    return VxVector4(m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z + m[3][0],
                     m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z + m[3][1],
                     m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z + m[3][2],
                     m[0][3] * v.x + m[1][3] * v.y + m[2][3] * v.z + m[3][3]);
}

inline VxVector4 operator*(const VxVector4 &v, const VxMatrix &m) {
    return VxVector4(m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z + m[3][0],
                     m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z + m[3][1],
                     m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z + m[3][2],
                     m[0][3] * v.x + m[1][3] * v.y + m[2][3] * v.z + m[3][3]);
}

inline void VxMatrix::Perspective(float Fov, float Aspect, float Near_plane, float Far_plane) {
    Clear();
    m_Data[0][0] = cosf(Fov * 0.5f) / sinf(Fov * 0.5f);
    m_Data[1][1] = m_Data[0][0] * Aspect;
    m_Data[2][2] = Far_plane / (Far_plane - Near_plane);
    m_Data[3][2] = -m_Data[2][2] * Near_plane;
    m_Data[2][3] = 1;
}

inline void VxMatrix::PerspectiveRect(float Left, float Right, float Top, float Bottom, float Near_plane, float Far_plane) {
    Clear();
    float RL = 1.0f / (Right - Left);
    float TB = 1.0f / (Top - Bottom);
    m_Data[0][0] = 2.0f * Near_plane * RL;
    m_Data[1][1] = 2.0f * Near_plane * TB;
    m_Data[2][0] = -(Right + Left) * RL;
    m_Data[2][1] = -(Top + Bottom) * TB;
    m_Data[2][2] = Far_plane / (Far_plane - Near_plane);
    m_Data[3][2] = -m_Data[2][2] * Near_plane;
    m_Data[2][3] = 1;
}

inline void VxMatrix::Orthographic(float Zoom, float Aspect, float Near_plane, float Far_plane) {
    Clear();
    float iz = 1.0f / (Far_plane - Near_plane);
    m_Data[0][0] = Zoom;
    m_Data[1][1] = Zoom * Aspect;
    m_Data[2][2] = iz;
    m_Data[3][2] = -Near_plane * iz;
    m_Data[3][3] = 1.0f;
}

inline void VxMatrix::OrthographicRect(float Left, float Right, float Top, float Bottom, float Near_plane, float Far_plane) {
    Clear();
    float ix = 1.0f / (Right - Left);
    float iy = 1.0f / (Top - Bottom);
    float iz = 1.0f / (Far_plane - Near_plane);
    m_Data[0][0] = 2.0f * ix;
    m_Data[1][1] = -2.0f * iy;
    m_Data[2][2] = iz;
    m_Data[3][0] = -(Left + Right) * ix;
    m_Data[3][1] = (Top + Bottom) * iy;
    m_Data[3][2] = -Near_plane * iz;
    m_Data[3][3] = 1.0f;
}

inline void Vx3DMultiplyMatrixVector(VxVector *ResultVector, const VxMatrix &Mat, const VxVector *Vector) {
    const float vx = Vector->x;
    const float vy = Vector->y;
    const float vz = Vector->z;

    const float *m0 = Mat[0];
    const float *m1 = Mat[1];
    const float *m2 = Mat[2];
    const float *m3 = Mat[3];

    ResultVector->x = vx * m0[0] + vy * m1[0] + vz * m2[0] + m3[0];
    ResultVector->y = vx * m0[1] + vy * m1[1] + vz * m2[1] + m3[1];
    ResultVector->z = vx * m0[2] + vy * m1[2] + vz * m2[2] + m3[2];
}

inline void Vx3DRotateVector(VxVector *ResultVector, const VxMatrix &Mat, const VxVector *Vector) {
    const float vx = Vector->x;
    const float vy = Vector->y;
    const float vz = Vector->z;

    const float *m0 = Mat[0];
    const float *m1 = Mat[1];
    const float *m2 = Mat[2];

    ResultVector->x = vx * m0[0] + vy * m1[0] + vz * m2[0];
    ResultVector->y = vx * m0[1] + vy * m1[1] + vz * m2[1];
    ResultVector->z = vx * m0[2] + vy * m1[2] + vz * m2[2];
}

inline void VxVector::Rotate(const VxMatrix &M) {
    const float ox = x, oy = y, oz = z;

    const float m00 = M[0][0], m10 = M[1][0], m20 = M[2][0];
    const float m01 = M[0][1], m11 = M[1][1], m21 = M[2][1];
    const float m02 = M[0][2], m12 = M[1][2], m22 = M[2][2];

    x = m00 * ox + m10 * oy + m20 * oz;
    y = m01 * ox + m11 * oy + m21 * oz;
    z = m02 * ox + m12 * oy + m22 * oz;
}

inline const VxVector Rotate(const VxMatrix &mat, const VxVector &pt) {
    const float m00 = mat[0][0], m10 = mat[1][0], m20 = mat[2][0];
    const float m01 = mat[0][1], m11 = mat[1][1], m21 = mat[2][1];
    const float m02 = mat[0][2], m12 = mat[1][2], m22 = mat[2][2];

    return VxVector(
        m00 * pt.x + m10 * pt.y + m20 * pt.z,
        m01 * pt.x + m11 * pt.y + m21 * pt.z,
        m02 * pt.x + m12 * pt.y + m22 * pt.z
    );
}

inline void Vx3DMatrixIdentity(VxMatrix &Mat) { Mat.SetIdentity(); }

inline void Vx3DMultiplyMatrixVector4(VxVector4 *ResultVector, const VxMatrix &Mat, const VxVector4 *Vector) {
    const float vx = Vector->x, vy = Vector->y, vz = Vector->z, vw = Vector->w;
    const float *m0 = Mat[0], *m1 = Mat[1], *m2 = Mat[2], *m3 = Mat[3];
    ResultVector->x = vx*m0[0] + vy*m1[0] + vz*m2[0] + vw*m3[0];
    ResultVector->y = vx*m0[1] + vy*m1[1] + vz*m2[1] + vw*m3[1];
    ResultVector->z = vx*m0[2] + vy*m1[2] + vz*m2[2] + vw*m3[2];
    ResultVector->w = vx*m0[3] + vy*m1[3] + vz*m2[3] + vw*m3[3];
}

inline void Vx3DMultiplyMatrixVector4(VxVector4 *ResultVector, const VxMatrix &Mat, const VxVector *Vector) {
    const float vx = Vector->x, vy = Vector->y, vz = Vector->z;
    const float *m0 = Mat[0], *m1 = Mat[1], *m2 = Mat[2], *m3 = Mat[3];
    ResultVector->x = vx*m0[0] + vy*m1[0] + vz*m2[0] + m3[0];
    ResultVector->y = vx*m0[1] + vy*m1[1] + vz*m2[1] + m3[1];
    ResultVector->z = vx*m0[2] + vy*m1[2] + vz*m2[2] + m3[2];
    ResultVector->w = vx*m0[3] + vy*m1[3] + vz*m2[3] + m3[3];
}

inline void Vx3DMultiplyMatrix(VxMatrix &ResultMat, const VxMatrix &MatA, const VxMatrix &MatB) {
    VxMatrix temp;
    for (int i = 0; i < 4; i++) {
        const float bi0=MatB[i][0], bi1=MatB[i][1], bi2=MatB[i][2], bi3=MatB[i][3];
        for (int j = 0; j < 4; j++)
            temp[i][j] = MatA[0][j]*bi0 + MatA[1][j]*bi1 + MatA[2][j]*bi2 + MatA[3][j]*bi3;
    }
    temp[0][3] = 0.0f; temp[1][3] = 0.0f; temp[2][3] = 0.0f; temp[3][3] = 1.0f;
    ResultMat = temp;
}

inline void Vx3DMultiplyMatrix4(VxMatrix &ResultMat, const VxMatrix &MatA, const VxMatrix &MatB) {
    VxMatrix temp;
    for (int i = 0; i < 4; i++) {
        const float bi0=MatB[i][0], bi1=MatB[i][1], bi2=MatB[i][2], bi3=MatB[i][3];
        for (int j = 0; j < 4; j++)
            temp[i][j] = MatA[0][j]*bi0 + MatA[1][j]*bi1 + MatA[2][j]*bi2 + MatA[3][j]*bi3;
    }
    ResultMat = temp;
}

inline void Vx3DInverseMatrix(VxMatrix &InverseMat, const VxMatrix &Mat) {
    const float a00=Mat[0][0], a01=Mat[0][1], a02=Mat[0][2];
    const float a10=Mat[1][0], a11=Mat[1][1], a12=Mat[1][2];
    const float a20=Mat[2][0], a21=Mat[2][1], a22=Mat[2][2];
    const float minor1 = a11*a22 - a12*a21;
    const float minor2 = a12*a20 - a10*a22;
    const float minor3 = a10*a21 - a11*a20;
    const double det = a00*minor1 + a01*minor2 + a02*minor3;
    if (fabs(det) < EPSILON) { InverseMat.SetIdentity(); return; }
    const double id = 1.0 / det;
    InverseMat[0][0] = static_cast<float>((a11*a22 - a12*a21)*id);
    InverseMat[0][1] = static_cast<float>((a02*a21 - a01*a22)*id);
    InverseMat[0][2] = static_cast<float>((a01*a12 - a02*a11)*id);
    InverseMat[0][3] = 0.0f;
    InverseMat[1][0] = static_cast<float>((a12*a20 - a10*a22)*id);
    InverseMat[1][1] = static_cast<float>((a00*a22 - a02*a20)*id);
    InverseMat[1][2] = static_cast<float>((a02*a10 - a00*a12)*id);
    InverseMat[1][3] = 0.0f;
    InverseMat[2][0] = static_cast<float>((a10*a21 - a11*a20)*id);
    InverseMat[2][1] = static_cast<float>((a01*a20 - a00*a21)*id);
    InverseMat[2][2] = static_cast<float>((a00*a11 - a01*a10)*id);
    InverseMat[2][3] = 0.0f;
    const float tx=Mat[3][0], ty=Mat[3][1], tz=Mat[3][2];
    InverseMat[3][0] = -(InverseMat[0][0]*tx + InverseMat[1][0]*ty + InverseMat[2][0]*tz);
    InverseMat[3][1] = -(InverseMat[0][1]*tx + InverseMat[1][1]*ty + InverseMat[2][1]*tz);
    InverseMat[3][2] = -(InverseMat[0][2]*tx + InverseMat[1][2]*ty + InverseMat[2][2]*tz);
    InverseMat[3][3] = 1.0f;
}

inline float Vx3DMatrixDeterminant(const VxMatrix &Mat) {
    const float a00=Mat[0][0], a01=Mat[0][1], a02=Mat[0][2];
    const float a10=Mat[1][0], a11=Mat[1][1], a12=Mat[1][2];
    const float a20=Mat[2][0], a21=Mat[2][1], a22=Mat[2][2];
    return a00*(a11*a22-a12*a21) + a01*(a12*a20-a10*a22) + a02*(a10*a21-a11*a20);
}

inline void Vx3DTransposeMatrix(VxMatrix &Result, const VxMatrix &A) {
    VxMatrix temp;
    temp[0][0]=A[0][0]; temp[0][1]=A[1][0]; temp[0][2]=A[2][0]; temp[0][3]=A[3][0];
    temp[1][0]=A[0][1]; temp[1][1]=A[1][1]; temp[1][2]=A[2][1]; temp[1][3]=A[3][1];
    temp[2][0]=A[0][2]; temp[2][1]=A[1][2]; temp[2][2]=A[2][2]; temp[2][3]=A[3][2];
    temp[3][0]=A[0][3]; temp[3][1]=A[1][3]; temp[3][2]=A[2][3]; temp[3][3]=A[3][3];
    Result = temp;
}

//////////////////////////////////////////////////////////////////////
////////////////// VxQuaternion methods needing VxMatrix /////////////
//////////////////////////////////////////////////////////////////////

inline VxQuaternion Vx3DQuaternionFromMatrix(const VxMatrix &Mat) {
    VxQuaternion quat;
    quat.FromMatrix(Mat, TRUE, TRUE);
    return quat;
}

inline void VxQuaternion::FromMatrix(const VxMatrix &Mat, XBOOL MatIsUnit, XBOOL RestoreMat) {
    float original[9];
    if (!MatIsUnit && RestoreMat) {
        original[0] = Mat[0][0]; original[1] = Mat[0][1]; original[2] = Mat[0][2];
        original[3] = Mat[1][0]; original[4] = Mat[1][1]; original[5] = Mat[1][2];
        original[6] = Mat[2][0]; original[7] = Mat[2][1]; original[8] = Mat[2][2];
    }

    if (!MatIsUnit) {
        VxMatrix &nonConstMat = const_cast<VxMatrix &>(Mat);
        VxVector row0(Mat[0][0], Mat[0][1], Mat[0][2]);
        VxVector row1(Mat[1][0], Mat[1][1], Mat[1][2]);
        row0.Normalize();
        row1.Normalize();
        nonConstMat[0][0] = row0.x; nonConstMat[0][1] = row0.y; nonConstMat[0][2] = row0.z;
        nonConstMat[1][0] = row1.x; nonConstMat[1][1] = row1.y; nonConstMat[1][2] = row1.z;
        VxVector row2 = CrossProduct(row0, row1);
        nonConstMat[2][0] = row2.x; nonConstMat[2][1] = row2.y; nonConstMat[2][2] = row2.z;
    }

    float trace = Mat[0][0] + Mat[1][1] + Mat[2][2];
    if (trace > 0.0f) {
        float s = sqrtf(trace + 1.0f);
        w = s * 0.5f;
        s = 0.5f / s;
        x = (Mat[2][1] - Mat[1][2]) * s;
        y = (Mat[0][2] - Mat[2][0]) * s;
        z = (Mat[1][0] - Mat[0][1]) * s;
    } else {
        int i = 0;
        if (Mat[1][1] > Mat[0][0]) i = 1;
        if (Mat[2][2] > Mat[i][i]) i = 2;
        static const int next[3] = {1, 2, 0};
        int j = next[i];
        int k = next[j];
        float s = sqrtf(Mat[i][i] - Mat[j][j] - Mat[k][k] + 1.0f);
        float *q[4] = {&x, &y, &z, &w};
        *q[i] = s * 0.5f;
        if (s > EPSILON) {
            s = 0.5f / s;
            *q[3] = (Mat[k][j] - Mat[j][k]) * s;
            *q[j] = (Mat[j][i] + Mat[i][j]) * s;
            *q[k] = (Mat[k][i] + Mat[i][k]) * s;
        } else {
            *q[3] = 1.0f; *q[j] = 0.0f; *q[k] = 0.0f;
        }
    }

    if (!MatIsUnit && RestoreMat) {
        VxMatrix &nonConstMat = const_cast<VxMatrix &>(Mat);
        nonConstMat[0][0] = original[0]; nonConstMat[0][1] = original[1]; nonConstMat[0][2] = original[2];
        nonConstMat[1][0] = original[3]; nonConstMat[1][1] = original[4]; nonConstMat[1][2] = original[5];
        nonConstMat[2][0] = original[6]; nonConstMat[2][1] = original[7]; nonConstMat[2][2] = original[8];
    }
}

inline void VxQuaternion::ToMatrix(VxMatrix &Mat) const {
    float norm = x * x + y * y + z * z + w * w;
    if (norm < EPSILON) { Mat.SetIdentity(); return; }
    float s = 2.0f / norm;
    float xs = x*s, ys = y*s, zs = z*s;
    float wx = w*xs, wy = w*ys, wz = w*zs;
    float xx = x*xs, xy = x*ys, xz = x*zs;
    float yy = y*ys, yz = y*zs, zz = z*zs;
    Mat[0][0] = 1.0f - (yy+zz); Mat[0][1] = xy-wz;          Mat[0][2] = xz+wy;          Mat[0][3] = 0.0f;
    Mat[1][0] = xy+wz;           Mat[1][1] = 1.0f - (xx+zz); Mat[1][2] = yz-wx;          Mat[1][3] = 0.0f;
    Mat[2][0] = xz-wy;           Mat[2][1] = yz+wx;           Mat[2][2] = 1.0f - (xx+yy); Mat[2][3] = 0.0f;
    Mat[3][0] = 0.0f;             Mat[3][1] = 0.0f;            Mat[3][2] = 0.0f;            Mat[3][3] = 1.0f;
}

inline void VxQuaternion::FromRotation(const VxVector &Vector, float Angle) {
    VxMatrix Mat;
    Vx3DMatrixFromRotation(Mat, Vector, Angle);
    FromMatrix(Mat, TRUE, TRUE);
}

inline void VxQuaternion::FromEulerAngles(float eax, float eay, float eaz) {
    VxMatrix Mat;
    Vx3DMatrixFromEulerAngles(Mat, eax, eay, eaz);
    FromMatrix(Mat, TRUE, TRUE);
}

inline void VxQuaternion::ToEulerAngles(float *eax, float *eay, float *eaz) const {
    VxMatrix Mat;
    ToMatrix(Mat);
    Vx3DMatrixToEulerAngles(Mat, eax, eay, eaz);
}

//////////////////////////////////////////////////////////////////////
////////////////// VxMatrix many/strided functions ///////////////////
//////////////////////////////////////////////////////////////////////

inline void Vx3DMultiplyMatrixVectorMany(VxVector *ResultVectors, const VxMatrix &Mat, const VxVector *Vectors, int count, int stride) {
    if (count <= 0) return;
    const float m00=Mat[0][0], m01=Mat[0][1], m02=Mat[0][2];
    const float m10=Mat[1][0], m11=Mat[1][1], m12=Mat[1][2];
    const float m20=Mat[2][0], m21=Mat[2][1], m22=Mat[2][2];
    const float m30=Mat[3][0], m31=Mat[3][1], m32=Mat[3][2];
    const char *srcPtr = reinterpret_cast<const char *>(Vectors);
    char *dstPtr = reinterpret_cast<char *>(ResultVectors);
    int blockCount = count / 4, remainder = count % 4;
    for (int block = 0; block < blockCount; ++block) {
        for (int i = 0; i < 4; ++i) {
            const VxVector *vec = reinterpret_cast<const VxVector *>(srcPtr + (block*4+i)*stride);
            VxVector *result   = reinterpret_cast<VxVector *>(dstPtr + (block*4+i)*stride);
            const float vx=vec->x, vy=vec->y, vz=vec->z;
            result->x = vx*m00 + vy*m10 + vz*m20 + m30;
            result->y = vx*m01 + vy*m11 + vz*m21 + m31;
            result->z = vx*m02 + vy*m12 + vz*m22 + m32;
        }
    }
    for (int i = 0; i < remainder; ++i) {
        const VxVector *vec = reinterpret_cast<const VxVector *>(srcPtr + (blockCount*4+i)*stride);
        VxVector *result   = reinterpret_cast<VxVector *>(dstPtr + (blockCount*4+i)*stride);
        const float vx=vec->x, vy=vec->y, vz=vec->z;
        result->x = vx*m00 + vy*m10 + vz*m20 + m30;
        result->y = vx*m01 + vy*m11 + vz*m21 + m31;
        result->z = vx*m02 + vy*m12 + vz*m22 + m32;
    }
}

inline void Vx3DRotateVectorMany(VxVector *ResultVector, const VxMatrix &Mat, const VxVector *Vector, int count, int stride) {
    if (count <= 0) return;
    const float m00=Mat[0][0], m01=Mat[0][1], m02=Mat[0][2];
    const float m10=Mat[1][0], m11=Mat[1][1], m12=Mat[1][2];
    const float m20=Mat[2][0], m21=Mat[2][1], m22=Mat[2][2];
    const char *srcPtr = reinterpret_cast<const char *>(Vector);
    char *dstPtr = reinterpret_cast<char *>(ResultVector);
    for (int i = 0; i < count; i += 4) {
        int remaining = (count - i < 4) ? count - i : 4;
        for (int j = 0; j < remaining; ++j) {
            const VxVector *vec = reinterpret_cast<const VxVector *>(srcPtr + (i+j)*stride);
            VxVector *result   = reinterpret_cast<VxVector *>(dstPtr + (i+j)*stride);
            const float vx=vec->x, vy=vec->y, vz=vec->z;
            result->x = vx*m00 + vy*m10 + vz*m20;
            result->y = vx*m01 + vy*m11 + vz*m21;
            result->z = vx*m02 + vy*m12 + vz*m22;
        }
    }
}

inline void Vx3DMultiplyMatrixVectorStrided(VxStridedData *Dest, VxStridedData *Src, const VxMatrix &Mat, int count) {
    if (!Dest || !Src || !Dest->Ptr || !Src->Ptr || count <= 0) return;
    const float m00=Mat[0][0], m01=Mat[0][1], m02=Mat[0][2];
    const float m10=Mat[1][0], m11=Mat[1][1], m12=Mat[1][2];
    const float m20=Mat[2][0], m21=Mat[2][1], m22=Mat[2][2];
    const float m30=Mat[3][0], m31=Mat[3][1], m32=Mat[3][2];
    const char *srcPtr = static_cast<const char *>(Src->Ptr);
    char *destPtr = static_cast<char *>(Dest->Ptr);
    for (int i = 0; i < count; ++i) {
        const VxVector *srcVec = reinterpret_cast<const VxVector *>(srcPtr);
        VxVector *destVec = reinterpret_cast<VxVector *>(destPtr);
        const float vx=srcVec->x, vy=srcVec->y, vz=srcVec->z;
        destVec->x = vx*m00 + vy*m10 + vz*m20 + m30;
        destVec->y = vx*m01 + vy*m11 + vz*m21 + m31;
        destVec->z = vx*m02 + vy*m12 + vz*m22 + m32;
        srcPtr += Src->Stride;
        destPtr += Dest->Stride;
    }
}

inline void Vx3DMultiplyMatrixVector4Strided(VxStridedData *Dest, VxStridedData *Src, const VxMatrix &Mat, int count) {
    if (!Dest || !Src || !Dest->Ptr || !Src->Ptr || count <= 0) return;
    const float *m0=Mat[0], *m1=Mat[1], *m2=Mat[2], *m3=Mat[3];
    const char *srcPtr = static_cast<const char *>(Src->Ptr);
    char *destPtr = static_cast<char *>(Dest->Ptr);
    for (int i = 0; i < count; ++i) {
        const VxVector4 *srcVec = reinterpret_cast<const VxVector4 *>(srcPtr);
        VxVector4 *destVec = reinterpret_cast<VxVector4 *>(destPtr);
        const float vx=srcVec->x, vy=srcVec->y, vz=srcVec->z, vw=srcVec->w;
        destVec->x = vx*m0[0] + vy*m1[0] + vz*m2[0] + vw*m3[0];
        destVec->y = vx*m0[1] + vy*m1[1] + vz*m2[1] + vw*m3[1];
        destVec->z = vx*m0[2] + vy*m1[2] + vz*m2[2] + vw*m3[2];
        destVec->w = vx*m0[3] + vy*m1[3] + vz*m2[3] + vw*m3[3];
        srcPtr += Src->Stride;
        destPtr += Dest->Stride;
    }
}

inline void Vx3DRotateVectorStrided(VxStridedData *Dest, VxStridedData *Src, const VxMatrix &Mat, int count) {
    if (!Dest || !Src || !Dest->Ptr || !Src->Ptr || count <= 0) return;
    const float m00=Mat[0][0], m01=Mat[0][1], m02=Mat[0][2];
    const float m10=Mat[1][0], m11=Mat[1][1], m12=Mat[1][2];
    const float m20=Mat[2][0], m21=Mat[2][1], m22=Mat[2][2];
    const char *srcPtr = static_cast<const char *>(Src->Ptr);
    char *destPtr = static_cast<char *>(Dest->Ptr);
    for (int i = 0; i < count; ++i) {
        const VxVector *srcVec = reinterpret_cast<const VxVector *>(srcPtr);
        VxVector *destVec = reinterpret_cast<VxVector *>(destPtr);
        const float vx=srcVec->x, vy=srcVec->y, vz=srcVec->z;
        destVec->x = vx*m00 + vy*m10 + vz*m20;
        destVec->y = vx*m01 + vy*m11 + vz*m21;
        destVec->z = vx*m02 + vy*m12 + vz*m22;
        srcPtr += Src->Stride;
        destPtr += Dest->Stride;
    }
}

//////////////////////////////////////////////////////////////////////
////////////////// Matrix rotation/Euler builders ////////////////////
//////////////////////////////////////////////////////////////////////

inline void Vx3DMatrixFromRotation(VxMatrix &ResultMat, const VxVector &Vector, float Angle) {
    if (fabsf(Angle) < EPSILON) { ResultMat.SetIdentity(); return; }
    const float c = cosf(Angle), s = sinf(Angle), t = 1.0f - c;
    const float lenSq = Vector.x*Vector.x + Vector.y*Vector.y + Vector.z*Vector.z;
    float x, y, z;
    if (lenSq > EPSILON) {
        const float invLen = 1.0f / sqrtf(lenSq);
        x = Vector.x*invLen; y = Vector.y*invLen; z = Vector.z*invLen;
    } else {
        x = 0.0f; y = 0.0f; z = 1.0f;
    }
    const float xx=x*x, yy=y*y, zz=z*z;
    const float xy=x*y, xz=x*z, yz=y*z;
    const float xs=x*s, ys=y*s, zs=z*s;
    const float xyt=xy*t, xzt=xz*t, yzt=yz*t;
    ResultMat[0][0] = xx*t+c;  ResultMat[0][1] = xyt+zs; ResultMat[0][2] = xzt-ys; ResultMat[0][3] = 0.0f;
    ResultMat[1][0] = xyt-zs; ResultMat[1][1] = yy*t+c;  ResultMat[1][2] = yzt+xs; ResultMat[1][3] = 0.0f;
    ResultMat[2][0] = xzt+ys; ResultMat[2][1] = yzt-xs; ResultMat[2][2] = zz*t+c;  ResultMat[2][3] = 0.0f;
    ResultMat[3][0] = 0.0f;    ResultMat[3][1] = 0.0f;    ResultMat[3][2] = 0.0f;    ResultMat[3][3] = 1.0f;
}

inline void Vx3DMatrixFromRotationAndOrigin(VxMatrix &ResultMat, const VxVector &Vector, const VxVector &Origin, float Angle) {
    Vx3DMatrixFromRotation(ResultMat, Vector, Angle);
    const float negOx=-Origin.x, negOy=-Origin.y, negOz=-Origin.z;
    const float *r0=ResultMat[0], *r1=ResultMat[1], *r2=ResultMat[2];
    ResultMat[3][0] = Origin.x + negOx*r0[0] + negOy*r1[0] + negOz*r2[0];
    ResultMat[3][1] = Origin.y + negOx*r0[1] + negOy*r1[1] + negOz*r2[1];
    ResultMat[3][2] = Origin.z + negOx*r0[2] + negOy*r1[2] + negOz*r2[2];
}

inline void Vx3DMatrixFromEulerAngles(VxMatrix &Mat, float eax, float eay, float eaz) {
    float cx, sx, cy, sy, cz, sz;
    if (fabsf(eax) <= EPSILON) { cx = 1.0f; sx = eax; } else { cx = cosf(eax); sx = sinf(eax); }
    if (fabsf(eay) <= EPSILON) { cy = 1.0f; sy = eay; } else { cy = cosf(eay); sy = sinf(eay); }
    if (fabsf(eaz) <= EPSILON) { cz = 1.0f; sz = eaz; } else { cz = cosf(eaz); sz = sinf(eaz); }
    const float sxsy=sx*sy, cxsy=cx*sy, cycz=cy*cz, cysz=cy*sz;
    Mat[0][0] = cycz;              Mat[0][1] = cysz;              Mat[0][2] = -sy;    Mat[0][3] = 0.0f;
    Mat[1][0] = sxsy*cz - cx*sz;  Mat[1][1] = sxsy*sz + cx*cz;  Mat[1][2] = sx*cy;  Mat[1][3] = 0.0f;
    Mat[2][0] = cxsy*cz + sx*sz;  Mat[2][1] = cxsy*sz - sx*cz;  Mat[2][2] = cx*cy;  Mat[2][3] = 0.0f;
    Mat[3][0] = 0.0f;              Mat[3][1] = 0.0f;              Mat[3][2] = 0.0f;   Mat[3][3] = 1.0f;
}

inline void Vx3DMatrixToEulerAngles(const VxMatrix &Mat, float *eax, float *eay, float *eaz) {
    const float m00=Mat[0][0], m01=Mat[0][1], m02=Mat[0][2];
    const float m12=Mat[1][2], m22=Mat[2][2], m21=Mat[2][1], m11=Mat[1][1];
    const float magnitude = sqrtf(m00*m00 + m01*m01);
    if (magnitude < EPSILON) {
        if (eay) *eay = atan2f(-m02, magnitude);
        if (eax) *eax = atan2f(-m21, m11);
        if (eaz) *eaz = 0.0f;
    } else {
        if (eay) *eay = atan2f(-m02, magnitude);
        if (eax) *eax = atan2f(m12, m22);
        if (eaz) *eaz = atan2f(m01, m00);
    }
}

//////////////////////////////////////////////////////////////////////
////////////////// Internal matrix decomposition helpers /////////////
//////////////////////////////////////////////////////////////////////

inline void Vx3DMatrixAdjoint(const VxMatrix &in, VxMatrix &out) {
    const float m00=in[0][0], m01=in[0][1], m02=in[0][2];
    const float m10=in[1][0], m11=in[1][1], m12=in[1][2];
    const float m20=in[2][0], m21=in[2][1], m22=in[2][2];
    out[0][0] = m11*m22 - m12*m21; out[1][0] = m12*m20 - m10*m22; out[2][0] = m10*m21 - m11*m20;
    out[0][1] = m02*m21 - m01*m22; out[1][1] = m00*m22 - m02*m20; out[2][1] = m01*m20 - m00*m21;
    out[0][2] = m01*m12 - m02*m11; out[1][2] = m02*m10 - m00*m12; out[2][2] = m00*m11 - m01*m10;
}

inline float Vx3DMatrixNorm(const VxMatrix &M, bool isOneNorm) {
    float maxNorm = 0.0f;
    for (int j = 0; j < 3; ++j) {
        float sum = 0.0f;
        for (int i = 0; i < 3; ++i)
            sum += fabsf(isOneNorm ? M[i][j] : M[j][i]);
        if (sum > maxNorm) maxNorm = sum;
    }
    return maxNorm;
}

inline float Vx3DMatrixPolarDecomposition(const VxMatrix &M_in, VxMatrix &Q, VxMatrix &S) {
    VxMatrix E;
    Vx3DTransposeMatrix(E, M_in);
    E[0][3] = E[1][3] = E[2][3] = E[3][0] = E[3][1] = E[3][2] = 0.0f;
    E[3][3] = 1.0f;
    float E_one_norm = Vx3DMatrixNorm(E, true);
    float E_inf_norm = Vx3DMatrixNorm(E, false);
    for (int iter = 0; iter < 20; ++iter) {
        VxMatrix E_adj;
        Vx3DMatrixAdjoint(E, E_adj);
        float det_E = DotProduct(VxVector(E[0][0],E[0][1],E[0][2]), VxVector(E_adj[0][0],E_adj[0][1],E_adj[0][2]));
        if (fabsf(det_E) < 1e-12f) break;
        float E_adj_one_norm = Vx3DMatrixNorm(E_adj, true);
        float E_adj_inf_norm = Vx3DMatrixNorm(E_adj, false);
        float gamma = sqrtf(sqrtf((E_adj_one_norm*E_adj_inf_norm)/(E_one_norm*E_inf_norm)) / fabsf(det_E));
        float c1 = 0.5f * gamma, c2 = 0.5f / (gamma * det_E);
        VxMatrix E_next;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                E_next[i][j] = c1*E[i][j] + c2*E_adj[j][i];
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
    for (int i = 0; i < 3; ++i)
        for (int j = i; j < 3; ++j) {
            float val = 0.5f * (S[i][j] + S[j][i]);
            S[i][j] = val; S[j][i] = val;
        }
    for (int i = 0; i < 3; ++i) {
        Q[i][3] = Q[3][i] = 0.0f;
        S[i][3] = S[3][i] = 0.0f;
    }
    Q[3][3] = 1.0f; S[3][3] = 1.0f;
    return Vx3DMatrixDeterminant(Q);
}

inline VxVector Vx3DMatrixSpectralDecomposition(const VxMatrix &S_in, VxMatrix &U_out) {
    U_out.SetIdentity();
    float d[3] = {S_in[0][0], S_in[1][1], S_in[2][2]};
    float o[3] = {S_in[0][1], S_in[0][2], S_in[1][2]};
    for (int sweep = 0; sweep < 20; ++sweep) {
        float sum_off_diag = fabsf(o[0]) + fabsf(o[1]) + fabsf(o[2]);
        if (sum_off_diag < 1e-9f) break;
        const int p_map[3] = {0,0,1}, q_map[3] = {1,2,2};
        for (int i = 0; i < 3; ++i) {
            int p = p_map[i], q = q_map[i];
            float S_pq = (p==0&&q==1)?o[0]:((p==0&&q==2)?o[1]:o[2]);
            if (fabsf(S_pq) < 1e-9f) continue;
            float diff = d[q] - d[p];
            float t_val;
            if (fabsf(diff) + fabsf(S_pq)*100.0f == fabsf(diff)) {
                t_val = S_pq / diff;
            } else {
                float theta = diff / (2.0f * S_pq);
                t_val = 1.0f / (fabsf(theta) + sqrtf(theta*theta + 1.0f));
                if (theta < 0.0f) t_val = -t_val;
            }
            float c = 1.0f / sqrtf(1.0f + t_val*t_val);
            float s = t_val * c;
            float tau = s / (1.0f + c);
            float h = t_val * S_pq;
            d[p] -= h; d[q] += h;
            int r = 3 - p - q;
            float S_pr = (p==0&&r==1)?o[0]:((p==0&&r==2)?o[1]:o[2]);
            if (p > r) S_pr = (r==0&&p==1)?o[0]:((r==0&&p==2)?o[1]:o[2]);
            float S_qr = (q==0&&r==1)?o[0]:((q==0&&r==2)?o[1]:o[2]);
            if (q > r) S_qr = (r==0&&q==1)?o[0]:((r==0&&q==2)?o[1]:o[2]);
            float next_S_pr = S_pr - s*(S_qr + S_pr*tau);
            float next_S_qr = S_qr + s*(S_pr - S_qr*tau);
            if (p==0&&r==1) o[0]=next_S_pr; else if(p==0&&r==2) o[1]=next_S_pr; else o[2]=next_S_pr;
            if (q==0&&r==1) o[0]=next_S_qr; else if(q==0&&r==2) o[1]=next_S_qr; else o[2]=next_S_qr;
            if (i==0) o[0]=0; if (i==1) o[1]=0; if (i==2) o[2]=0;
            for (int k = 0; k < 3; ++k) {
                float g = U_out[k][p], h_u = U_out[k][q];
                U_out[k][p] = g - s*(h_u + g*tau);
                U_out[k][q] = h_u + s*(g - h_u*tau);
            }
        }
    }
    return VxVector(d[0], d[1], d[2]);
}

//////////////////////////////////////////////////////////////////////
////////////////// Matrix decomposition functions ////////////////////
//////////////////////////////////////////////////////////////////////

inline void Vx3DDecomposeMatrix(const VxMatrix &A, VxQuaternion &Quat, VxVector &Pos, VxVector &Scale) {
    Pos = VxVector(A[3][0], A[3][1], A[3][2]);
    VxMatrix Mat = A;
    Quat.FromMatrix(Mat, FALSE, FALSE);
    Scale.x = Mat[0][0]*A[0][0] + Mat[0][1]*A[0][1] + Mat[0][2]*A[0][2];
    Scale.y = Mat[1][0]*A[1][0] + Mat[1][1]*A[1][1] + Mat[1][2]*A[1][2];
    Scale.z = Mat[2][0]*A[2][0] + Mat[2][1]*A[2][1] + Mat[2][2]*A[2][2];
}

inline float Vx3DDecomposeMatrixTotal(const VxMatrix &A, VxQuaternion &Quat, VxVector &Pos, VxVector &Scale, VxQuaternion &URot) {
    Pos = VxVector(A[3][0], A[3][1], A[3][2]);
    VxMatrix Q, S;
    float det = Vx3DMatrixPolarDecomposition(A, Q, S);
    if (det < 0.0f) {
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                Q[i][j] = -Q[i][j];
        det = -1.0f;
    } else {
        det = 1.0f;
    }
    Quat = Vx3DQuaternionFromMatrix(Q);
    VxMatrix U;
    Scale = Vx3DMatrixSpectralDecomposition(S, U);
    URot = Vx3DQuaternionFromMatrix(U);
    VxQuaternion snuggleQuat = Vx3DQuaternionSnuggle(&URot, &Scale);
    URot = Vx3DQuaternionMultiply(URot, snuggleQuat);
    return det;
}

inline float Vx3DDecomposeMatrixTotalPtr(const VxMatrix &A, VxQuaternion *Quat, VxVector *Pos, VxVector *Scale, VxQuaternion *URot) {
    if (Pos) *Pos = VxVector(A[3][0], A[3][1], A[3][2]);
    if (!Quat && !Scale && !URot) return 1.0f;
    VxMatrix Q, S;
    float det = Vx3DMatrixPolarDecomposition(A, Q, S);
    if (det < 0.0f) {
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                Q[i][j] = -Q[i][j];
        det = -1.0f;
    } else {
        det = 1.0f;
    }
    if (Quat) *Quat = Vx3DQuaternionFromMatrix(Q);
    if (Scale || URot) {
        VxMatrix U;
        VxVector tempScale = Vx3DMatrixSpectralDecomposition(S, U);
        VxQuaternion tempURot = Vx3DQuaternionFromMatrix(U);
        VxQuaternion snuggleQuat = Vx3DQuaternionSnuggle(&tempURot, &tempScale);
        if (URot) *URot = Vx3DQuaternionMultiply(tempURot, snuggleQuat);
        if (Scale) *Scale = tempScale;
    }
    return det;
}

inline void Vx3DInterpolateMatrix(float step, VxMatrix &Res, const VxMatrix &A, const VxMatrix &B) {
    VxQuaternion quatA, quatB, uRotA, uRotB;
    VxVector posA, posB, scaleA, scaleB;
    Vx3DDecomposeMatrixTotal(A, quatA, posA, scaleA, uRotA);
    Vx3DDecomposeMatrixTotal(B, quatB, posB, scaleB, uRotB);
    VxQuaternion quatRes = Slerp(step, quatA, quatB);
    VxVector posRes = Interpolate(step, posA, posB);
    VxVector scaleRes = Interpolate(step, scaleA, scaleB);
    VxQuaternion uRotRes = Slerp(step, uRotA, uRotB);
    VxMatrix scaleMat, rotMat, uRotMat;
    scaleMat.SetIdentity();
    scaleMat[0][0] = scaleRes.x;
    scaleMat[1][1] = scaleRes.y;
    scaleMat[2][2] = scaleRes.z;
    quatRes.ToMatrix(rotMat);
    uRotRes.ToMatrix(uRotMat);
    VxMatrix temp;
    Vx3DMultiplyMatrix(temp, uRotMat, scaleMat);
    Vx3DMultiplyMatrix(Res, rotMat, temp);
    Res[3][0] = posRes.x;
    Res[3][1] = posRes.y;
    Res[3][2] = posRes.z;
}

inline void Vx3DInterpolateMatrixNoScale(float step, VxMatrix &Res, const VxMatrix &A, const VxMatrix &B) {
    VxQuaternion quatA, quatB;
    VxVector posA, posB, scaleA, scaleB;
    Vx3DDecomposeMatrix(A, quatA, posA, scaleA);
    Vx3DDecomposeMatrix(B, quatB, posB, scaleB);
    VxQuaternion quatRes = Slerp(step, quatA, quatB);
    VxVector posRes = Interpolate(step, posA, posB);
    quatRes.ToMatrix(Res);
    Res[3][0] = posRes.x;
    Res[3][1] = posRes.y;
    Res[3][2] = posRes.z;
}

#endif // VXMATRIX_H
