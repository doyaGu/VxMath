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
 * @brief Transforms a 3D point with implicit w=1, without perspective division.
 * @param ResultVector Pointer to store the resulting transformed vector.
 * @param Mat The transformation matrix.
 * @param Vector Pointer to the input vector to transform.
 * @remarks Ignores the matrix's fourth column, writes only XYZ, and supports ResultVector == Vector.
 */
inline void Vx3DMultiplyMatrixVector(VxVector *ResultVector, const VxMatrix &Mat, const VxVector *Vector);

/**
 * @brief Transforms an array of 3D vectors by a matrix.
 * @param ResultVectors Pointer to the output array of transformed vectors.
 * @param Mat The transformation matrix.
 * @param Vectors Pointer to the input array of vectors.
 * @param count The number of vectors to transform.
 * @param stride The byte offset between consecutive vectors in the input and output arrays.
 * @remarks Transforms XYZ with implicit w=1, without perspective division; writes only 12 bytes per record.
 *          Both arrays advance by the same signed stride, including negative or zero stride.
 *          Each element is read and written before advancing, including exact in-place or overlapping records.
 *          The matrix must remain separate from the output records. Nonpositive count performs no access.
 */
inline void Vx3DMultiplyMatrixVectorMany(VxVector *ResultVectors, const VxMatrix &Mat, const VxVector *Vectors, int count, int stride);

/**
 * @brief Transforms a 4D vector by a matrix.
 * @param ResultVector Pointer to store the resulting transformed 4D vector.
 * @param Mat The transformation matrix.
 * @param Vector Pointer to the input 4D vector.
 * @remarks Reads all XYZW and writes all four components without perspective division.
 *          Supports identical input/output and matrix row aliases.
 */
inline void Vx3DMultiplyMatrixVector4(VxVector4 *ResultVector, const VxMatrix &Mat, const VxVector4 *Vector);

/**
 * @brief Transforms a 3D vector by a matrix, returning a 4D vector (w component is calculated).
 * @param ResultVector Pointer to store the resulting transformed 4D vector.
 * @param Mat The transformation matrix.
 * @param Vector Pointer to the input 3D vector (w is assumed to be 1).
 * @remarks Reads only 12 input bytes and writes 16 output bytes without perspective division.
 *          The input may be the base view of the output VxVector4 or a matrix row.
 */
inline void Vx3DMultiplyMatrixVector4(VxVector4 *ResultVector, const VxMatrix &Mat, const VxVector *Vector);

/**
 * @brief Applies the matrix's linear 3x3 transform to a 3D vector.
 * @param ResultVector Pointer to store the resulting rotated vector.
 * @param Mat The rotation matrix.
 * @param Vector Pointer to the input vector to rotate.
 * @remarks Ignores translation and the fourth column, without normalizing. Writes only XYZ and supports in-place use.
 */
inline void Vx3DRotateVector(VxVector *ResultVector, const VxMatrix &Mat, const VxVector *Vector);

/**
 * @brief Rotates an array of 3D vectors by a matrix.
 * @param ResultVector Pointer to the output array of rotated vectors.
 * @param Mat The rotation matrix.
 * @param Vector Pointer to the input array of vectors.
 * @param count The number of vectors to rotate.
 * @param stride The byte offset between consecutive vectors.
 * @remarks Applies only the linear 3x3 transform, without translation, normalization or perspective division.
 *          Reads and writes XYZ (12 bytes); does not read the matrix's translation row.
 *          Source and destination share a signed byte stride, including negative or zero stride.
 *          Each element is read and written before advancing; writes are visible to later reads.
 *          The matrix must remain separate from output records. Nonpositive count performs no access.
 */
inline void Vx3DRotateVectorMany(VxVector *ResultVector, const VxMatrix &Mat, const VxVector *Vector, int count, int stride);

/**
 * @brief Multiplies two affine 3D transformation matrices.
 * @param ResultMat The resulting matrix (MatB * MatA in row-vector notation).
 * @param MatA The first matrix operand.
 * @param MatB The second matrix operand.
 * @remarks Applies MatB then MatA using their 3x3 blocks and translations; the last column is [0,0,0,1].
 * Use `Vx3DMultiplyMatrix4()` when projective terms must be preserved.
 */
inline void Vx3DMultiplyMatrix(VxMatrix &ResultMat, const VxMatrix &MatA, const VxMatrix &MatB);

/**
 * @brief Multiplies two full 4x4 matrices.
 * @param ResultMat The resulting matrix (MatB * MatA in row-vector notation).
 * @param MatA The first matrix operand.
 * @param MatB The second matrix operand.
 * @remarks Applies MatB then MatA, retaining all homogeneous terms. ResultMat may alias either operand.
 */
inline void Vx3DMultiplyMatrix4(VxMatrix &ResultMat, const VxMatrix &MatA, const VxMatrix &MatB);

/**
 * @brief Computes the inverse of the affine 3x3 transform and translation.
 * @param InverseMat The resulting inverse matrix.
 * @param Mat The matrix to invert; its fourth column is ignored.
 * @remarks An exactly singular linear transform leaves InverseMat unchanged. InverseMat may alias Mat.
 */
inline void Vx3DInverseMatrix(VxMatrix &InverseMat, const VxMatrix &Mat);

/**
 * @brief Computes the determinant of the upper-left 3x3 linear transform.
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
// As in the original runtime, an Origin referencing ResultMat[3] observes
// sequential translation writes. Pass a copy for a fixed pivot in that case.
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
 * @brief Interpolates decomposed translation and row scales linearly, and rotation with Slerp.
 * @param step The interpolation factor (0.0 for matrix A, 1.0 for matrix B).
 * @param Res The resulting interpolated matrix.
 * @param A The first matrix.
 * @param B The second matrix.
 */
inline void Vx3DInterpolateMatrix(float step, VxMatrix &Res, const VxMatrix &A, const VxMatrix &B);

/**
 * @brief Compatibility alias for Vx3DInterpolateMatrix, including scale interpolation.
 * @note Both names resolve to the same function in the original runtime.
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
 * @remarks Source and destination have independent unsigned byte strides, including zero.
 *          Caches the descriptors' pointers and strides; the descriptors remain unchanged and may be identical.
 *          Transforms XYZ with implicit w=1, without perspective division; reads and writes 12 bytes per item.
 *          Each item is read and written before advancing. The matrix and descriptors must remain separate from output records.
 *          Nonpositive count performs no access.
 */
inline void Vx3DMultiplyMatrixVectorStrided(VxStridedData *Dest, VxStridedData *Src, const VxMatrix &Mat, int count);

/**
 * @brief Transforms strided 3D points to complete 4D results, using implicit w=1.
 * @param Dest Pointer to the destination strided data structure.
 * @param Src Pointer to the source strided data structure.
 * @param Mat The transformation matrix.
 * @param count The number of vectors to transform.
 * @remarks Reads only XYZ (12 bytes) per input and writes XYZW (16 bytes), without perspective division.
 *          Source and destination have independent unsigned byte strides, including zero; descriptors remain unchanged.
 *          Each point is read and written before advancing. In-place use requires VxVector4 record storage.
 *          The matrix and descriptors must remain separate from output records. Nonpositive count performs no access.
 */
inline void Vx3DMultiplyMatrixVector4Strided(VxStridedData *Dest, VxStridedData *Src, const VxMatrix &Mat, int count);

/**
 * @brief Rotates an array of vectors with a specific stride by a matrix.
 * @param Dest Pointer to the destination strided data structure.
 * @param Src Pointer to the source strided data structure.
 * @param Mat The rotation matrix.
 * @param count The number of vectors to rotate.
 * @remarks Reads and writes only XYZ (12 bytes); uses the upper 3x3, without translation or division.
 *          Source and destination have independent unsigned byte strides, including zero; descriptors remain unchanged.
 *          Each vector is read and written before advancing. Matrix and descriptors must remain separate from output.
 *          Nonpositive count performs no access.
 */
inline void Vx3DRotateVectorStrided(VxStridedData *Dest, VxStridedData *Src, const VxMatrix &Mat, int count);

/**
 * @brief Computes the transpose of a 4x4 matrix.
 * @param Result The resulting transposed matrix.
 * @param A The input matrix.
 * @remarks Transposes all sixteen 32-bit fields, preserving their representations without arithmetic.
 *          Result may be the same matrix as A; the input is captured before any output is written.
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
 * @brief Decomposes the linear part as sign * R(Quat) * R(URot) * diag(Scale) * R(URot)^T.
 * @param A The input transformation matrix.
 * @param Quat The output primary rotation quaternion.
 * @param Pos The output translation vector.
 * @param Scale The stretch eigenvalues, reordered together with URot by Snuggle.
 * @param URot The rotation of the stretch axes.
 * @return The reflection sign (-1 or +1); a singular linear part returns +1.
 */
inline float Vx3DDecomposeMatrixTotal(const VxMatrix &A, VxQuaternion &Quat, VxVector &Pos, VxVector &Scale, VxQuaternion &URot);

/**
 * @brief Decomposes a transformation matrix into components, using pointers for optional outputs.
 * @param A The input transformation matrix.
 * @param Quat Pointer to store the output rotation quaternion (can be NULL).
 * @param Pos Pointer to store the output translation vector (can be NULL).
 * @param Scale Pointer to store the output scaling vector (can be NULL).
 * @param URot Pointer to store the output secondary rotation quaternion (can be NULL).
 * @return The same reflection sign as Vx3DDecomposeMatrixTotal. If Quat, Scale
 * and URot are all NULL, returns +1 without reading the linear part; Pos is
 * still copied when requested. Scale alone uses the same Snuggle ordering.
 */
inline float Vx3DDecomposeMatrixTotalPtr(const VxMatrix &A, VxQuaternion *Quat, VxVector *Pos, VxVector *Scale, VxQuaternion *URot);
/// @}

/**
 * @brief A class representing a 4x4 matrix.
 */
class VxMatrix {
public:
    /// @brief Default constructor. Initializes all elements to zero.
    VxMatrix();

    /**
     * @brief Constructs a matrix from a 2D float array.
     * @param m A 4x4 array of floats to initialize the matrix with.
     */
    VxMatrix(float m[4][4]);

    /**
     * @brief Constructs an identity matrix.
     * @return An independent identity matrix by value.
     */
    static VxMatrix Identity();

    // Matrix construction
    /// @brief Sets all elements of the matrix to zero.
    void Clear();
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
     * @param Fov The horizontal field of view, in radians.
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
    const VxVector4 &operator[](int i) const;
    /// @brief Provides mutable access to a row of the matrix.
    VxVector4 &operator[](int i);

    /// @brief Implicitly converts the matrix to a const void pointer to its data.
    operator const void *() const;
    /// @brief Implicitly converts the matrix to a void pointer to its data.
    operator void *();

    /**
     * @brief Compares two matrices for equality.
     * @remarks This operator compares the contents of the matrices.
     * @return TRUE if the matrices have the same contents, FALSE otherwise.
     */
    XBOOL operator==(const VxMatrix &mat) const;

    /**
     * @brief Compares two matrices for inequality.
     * @remarks This operator compares the contents of the matrices.
     * @return TRUE if the matrices have different contents, FALSE otherwise.
     */
    XBOOL operator!=(const VxMatrix &mat) const;

    /**
     * @brief Multiplies this matrix by another matrix and assigns the result to this matrix.
     * @param mat The matrix to multiply with.
     * @return A reference to this modified matrix.
     * @remarks Uses affine multiplication (`Vx3DMultiplyMatrix`).
     */
    VxMatrix &operator*=(const VxMatrix &mat);

    /**
     * @brief Multiplies this matrix by another matrix.
     * @param iMat The matrix to multiply with.
     * @return The resulting new matrix.
     * @remarks Uses affine multiplication (`Vx3DMultiplyMatrix`).
     */
    VxMatrix operator*(const VxMatrix &iMat) const;

    // Both operand orders transform XYZ as a point with implicit w=1.
    // Use the explicit VxVector4-input function overload to transform actual w.
    /// @brief Returns the three point-transform coordinates.
    friend VxVector operator*(const VxMatrix &m, const VxVector &v);
    /// @brief Returns all four point-transform coordinates, ignoring source w.
    friend VxVector4 operator*(const VxMatrix &m, const VxVector4 &v);
    /// @brief Returns the three point-transform coordinates.
    friend VxVector operator*(const VxVector &v, const VxMatrix &m);
    /// @brief Returns all four point-transform coordinates, ignoring source w.
    friend VxVector4 operator*(const VxVector4 &v, const VxMatrix &m);

protected:
    float m_Data[4][4]; ///< The 16 floating-point values of the matrix.
};

#include "VxMatrix.inl"

#endif // VXMATRIX_H
