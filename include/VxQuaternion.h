#ifndef VXQUATERNION_H
#define VXQUATERNION_H

#include "VxVector.h"

/**
 * @enum QuatPart
 * @brief Enumerates the components of a quaternion for indexed access.
 */
enum QuatPart {
    Quat_X, ///< The X component.
    Quat_Y, ///< The Y component.
    Quat_Z, ///< The Z component.
    Quat_W  ///< The W component.
};

struct VxQuaternion;
class VxMatrix;

/** @name Quaternion Utility Functions */
///@{

/**
 * @brief Adjusts a quaternion to account for scaling factors.
 * @param Quat Pointer to the quaternion to adjust.
 * @param Scale Pointer to a vector containing scaling factors.
 * @return The adjusted quaternion.
 */
VX_EXPORT VxQuaternion Vx3DQuaternionSnuggle(VxQuaternion *Quat, VxVector *Scale);

/**
 * @brief Creates a quaternion from a rotation matrix.
 * @param Mat The source rotation matrix.
 * @return The resulting quaternion.
 */
inline VxQuaternion Vx3DQuaternionFromMatrix(const VxMatrix &Mat);

/**
 * @brief Computes the conjugate of a quaternion.
 * @param Quat The quaternion to conjugate.
 * @return The conjugate of the input quaternion.
 */
inline VxQuaternion Vx3DQuaternionConjugate(const VxQuaternion &Quat);

/**
 * @brief Multiplies two quaternions.
 * @param QuatL The left-hand side quaternion.
 * @param QuatR The right-hand side quaternion.
 * @return The result of the multiplication (QuatL * QuatR).
 */
inline VxQuaternion Vx3DQuaternionMultiply(const VxQuaternion &QuatL, const VxQuaternion &QuatR);

/**
 * @brief Divides one quaternion by another.
 * @param P The numerator quaternion.
 * @param Q The denominator quaternion.
 * @return The result of the division (P * Q^-1).
 */
inline VxQuaternion Vx3DQuaternionDivide(const VxQuaternion &P, const VxQuaternion &Q);

/**
 * @brief Performs spherical linear interpolation between two quaternions.
 * @param Theta The interpolation factor, from 0.0 (Quat1) to 1.0 (Quat2).
 * @param Quat1 The starting quaternion.
 * @param Quat2 The ending quaternion.
 * @return The interpolated quaternion.
 */
inline VxQuaternion Slerp(float Theta, const VxQuaternion &Quat1, const VxQuaternion &Quat2);

/**
 * @brief Performs spherical quadrangle interpolation.
 * @param Theta The interpolation factor.
 * @param Quat1 The starting quaternion.
 * @param Quat1Out The outgoing control quaternion for Quat1.
 * @param Quat2In The incoming control quaternion for Quat2.
 * @param Quat2 The ending quaternion.
 * @return The interpolated quaternion.
 */
inline VxQuaternion Squad(float Theta, const VxQuaternion &Quat1, const VxQuaternion &Quat1Out, const VxQuaternion &Quat2In, const VxQuaternion &Quat2);

/**
 * @brief Computes the logarithmic difference between two quaternions, equivalent to Ln(Q^-1 * P).
 * @param P The first quaternion.
 * @param Q The second quaternion.
 * @return The quaternion representing the logarithmic difference.
 */
inline VxQuaternion LnDif(const VxQuaternion &P, const VxQuaternion &Q);

/**
 * @brief Computes the natural logarithm of a quaternion.
 * @param Quat The input quaternion.
 * @return The natural logarithm of the quaternion.
 */
inline VxQuaternion Ln(const VxQuaternion &Quat);

/**
 * @brief Computes the exponential of a quaternion.
 * @param Quat The input quaternion.
 * @return The exponential of the quaternion.
 */
inline VxQuaternion Exp(const VxQuaternion &Quat);
///@}

/**
 * @struct VxQuaternion
 * @brief Represents an orientation in 3D space using a four-component quaternion.
 *
 * @remarks
 * A quaternion is defined by 4 floats (x, y, z, w) and is primarily used to represent
 * an orientation. Its common usage is for smooth interpolation between two orientations
 * through spherical linear interpolation (Slerp). Quaternions can be converted
 * to and from VxMatrix or Euler angles.
 *
 * A VxQuaternion is defined as:
 * @code
 * struct VxQuaternion {
 *     union {
 *         struct { float x, y, z, w; };
 *         float v[4];
 *     };
 * };
 * @endcode
 * Elements can be accessed either by name (x, y, z, w) or through the float array `v`.
 *
 * @see VxMatrix, VxVector
 */
typedef struct VxQuaternion {
    // Members
#if !defined(_MSC_VER)
    float x; ///< The x component of the vector part.
    float y; ///< The y component of the vector part.
    float z; ///< The z component of the vector part.
    union
    {
        float w;     ///< The scalar part of the quaternion.
        float angle; ///< Alias for the scalar part, w.
    };
#else
    union {
        struct {
            VxVector axis; ///< The vector part (x, y, z).
            float angle;   ///< The scalar part (w).
        };
        struct {
            float x; ///< The x component of the vector part.
            float y; ///< The y component of the vector part.
            float z; ///< The z component of the vector part.
            float w; ///< The scalar part of the quaternion.
        };
        float v[4]; ///< Array access to components {x, y, z, w}.
    };
#endif

public:
    /// @brief Default constructor. Initializes to an identity quaternion (0, 0, 0, 1).
    VxQuaternion();

    /**
     * @brief Constructs a quaternion from an axis and an angle.
     * @param Vector The axis of rotation.
     * @param Angle The angle of rotation in radians.
     */
    VxQuaternion(const VxVector &Vector, float Angle);

    /**
     * @brief Constructs a quaternion from four float components.
     * @param X The x component.
     * @param Y The y component.
     * @param Z The z component.
     * @param W The w component.
     */
    VxQuaternion(float X, float Y, float Z, float W);

    /**
     * @brief Creates a quaternion from a matrix.
     * @param Mat The source matrix.
     * @param MatIsUnit If TRUE, assumes the matrix has a unit scale, allowing for a faster conversion.
     * @param RestoreMat Hint to avoid modifying the source matrix if possible.
     */
    inline void FromMatrix(const VxMatrix &Mat, XBOOL MatIsUnit = TRUE, XBOOL RestoreMat = TRUE);

    /// @brief Converts this quaternion to a rotation matrix.
    inline void ToMatrix(VxMatrix &Mat) const;

    /// @brief Multiplies this quaternion by another in-place.
    void Multiply(const VxQuaternion &Quat);

    /// @brief Sets this quaternion from an axis and an angle of rotation.
    inline void FromRotation(const VxVector &Vector, float Angle);

    /// @brief Sets this quaternion from a set of Euler angles.
    inline void FromEulerAngles(float eax, float eay, float eaz);

    /// @brief Converts this quaternion to a set of Euler angles.
    inline void ToEulerAngles(float *eax, float *eay, float *eaz) const;

    /// @brief Normalizes this quaternion to unit length.
    void Normalize();

    /// @brief Provides const access to the components by index.
    const float &operator[](int i) const;
    /// @brief Provides mutable access to the components by index.
    float &operator[](int i);

    /// @brief Component-wise addition of two quaternions.
    VxQuaternion operator+(const VxQuaternion &q) const;
    /// @brief Component-wise subtraction of two quaternions.
    VxQuaternion operator-(const VxQuaternion &q) const;
    /// @brief Multiplies two quaternions.
    VxQuaternion operator*(const VxQuaternion &q) const;
    /// @brief Divides one quaternion by another.
    VxQuaternion operator/(const VxQuaternion &q) const;

    /// @brief Multiplies a quaternion by a scalar.
    friend VxQuaternion operator*(float, const VxQuaternion &);
    /// @brief Multiplies a quaternion by a scalar.
    friend VxQuaternion operator*(const VxQuaternion &, float);

    /// @brief Multiplies this quaternion by a scalar and assigns the result.
    VxQuaternion &operator*=(float s);

    /// @brief Returns the negation of the quaternion.
    VxQuaternion operator-() const;
    /// @brief Unary plus operator, returns a copy of the quaternion.
    VxQuaternion operator+() const;

    /// @brief Checks for bitwise equality.
    friend int operator==(const VxQuaternion &q1, const VxQuaternion &q2);
    /// @brief Checks for bitwise inequality.
    friend int operator!=(const VxQuaternion &q1, const VxQuaternion &q2);

    /// @brief Calculates the squared magnitude of a quaternion.
    friend float Magnitude(const VxQuaternion &q);
    /// @brief Calculates the dot product of two quaternions.
    friend float DotProduct(const VxQuaternion &p, const VxQuaternion &q);
} VxQuaternion;

#include "VxQuaternion.inl"

#endif // VXQUATERNION_H
