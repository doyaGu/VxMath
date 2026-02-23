#ifndef VXVECTOR_H
#define VXVECTOR_H

#include "VxMathDefines.h"
#include "XUtil.h"

// Forward declarations
struct VxCompressedVector;
struct VxCompressedVectorOld;
class VxMatrix;

/** @name Trigonometry Utility Functions */
///@{

/**
 * @brief Converts radians to a custom angular unit (likely for table lookup).
 * @param val Angle in radians.
 * @return The angle in the custom unit.
 */
VX_EXPORT int radToAngle(float val);

/**
 * @brief Computes sine from a custom angular unit (likely using a lookup table).
 * @param angle Angle in the custom unit.
 * @return The sine of the angle.
 */
VX_EXPORT float Tsin(int angle);

/**
 * @brief Computes cosine from a custom angular unit (likely using a lookup table).
 * @param angle Angle in the custom unit.
 * @return The cosine of the angle.
 */
VX_EXPORT float Tcos(int angle);
///@}

/**
 * @brief Represents a point or direction in 3D space with x, y, and z components.
 *
 * @remarks
 * A VxVector is a fundamental class for 3D mathematics. Its elements can be
 * accessed either via named members (x, y, z) or through array-style indexing.
 *
 * @code
 * VxVector v(1.0f, 2.0f, 3.0f);
 * float x_val = v.x;    // Direct member access
 * float y_val = v[1];   // Array-style access
 * @endcode
 *
 * @see VxVector4, VxCompressedVector
 */
struct VxVector {
#if !defined(_MSC_VER)
    float x; ///< The x-component of the vector.
    float y; ///< The y-component of the vector.
    float z; ///< The z-component of the vector.
#else
    union {
        struct {
            float x; ///< The x-component of the vector.
            float y; ///< The y-component of the vector.
            float z; ///< The z-component of the vector.
        };
        float v[3]; ///< Array access to components {x, y, z}.
    };
#endif

    /** @name Constructors */
    ///@{

    /// @brief Default constructor. Initializes the vector to (0, 0, 0).
    VxVector();

    /**
     * @brief Uniform constructor. Initializes all components to the same value.
     * @param f The value to assign to all three components.
     */
    VxVector(float f);

    /**
     * @brief Component-wise constructor.
     * @param _x The value for the x-component.
     * @param _y The value for the y-component.
     * @param _z The value for the z-component.
     */
    VxVector(float _x, float _y, float _z);

    /**
     * @brief Array constructor.
     * @param f A pointer to a 3-element float array [x, y, z].
     */
    VxVector(const float f[3]);
    ///@}

    /// @brief Converts each component of the vector to its absolute value.
    void Absolute();

    /**
     * @brief Returns the squared magnitude (length) of the vector.
     * @return The square of the vector's length (x*x + y*y + z*z).
     * @remarks This is more efficient than Magnitude() as it avoids a square root operation.
     * @see Magnitude()
     */
    float SquareMagnitude() const { return x * x + y * y + z * z; }

    /**
     * @brief Returns the magnitude (length) of the vector.
     * @return The length of the vector (sqrt(x*x + y*y + z*z)).
     * @warning Calling this on a null vector (0,0,0) will result in NaN (Not aNumber).
     * @see SquareMagnitude()
     */
    float Magnitude() const { return sqrtf(SquareMagnitude()); }

    /** @name Element Access */
    ///@{

    /**
     * @brief Provides const array-style access to the vector's components.
     * @param i The component index (0 for x, 1 for y, 2 for z).
     * @return A const reference to the specified component.
     */
    const float &operator[](int i) const;

    /**
     * @brief Provides array-style access to the vector's components.
     * @param i The component index (0 for x, 1 for y, 2 for z).
     * @return A reference to the specified component.
     */
    float &operator[](int i);

    /**
     * @brief Sets the three components of the vector.
     * @param X The new x-component.
     * @param Y The new y-component.
     * @param Z The new z-component.
     */
    void Set(float X, float Y, float Z);
    ///@}

    /** @name Assignment Operators */
    ///@{

    /// @brief Performs component-wise addition and assignment.
    VxVector &operator+=(const VxVector &v);
    /// @brief Performs component-wise subtraction and assignment.
    VxVector &operator-=(const VxVector &v);
    /// @brief Performs component-wise multiplication and assignment.
    VxVector &operator*=(const VxVector &v);
    /// @brief Performs component-wise division and assignment.
    VxVector &operator/=(const VxVector &v);
    /// @brief Performs scalar multiplication and assignment.
    VxVector &operator*=(float s);
    /// @brief Performs scalar division and assignment.
    VxVector &operator/=(float s);

    /**
     * @brief Assigns the value of a compressed vector to this vector.
     * @param v The compressed vector.
     * @return A reference to this modified vector.
     */
    VX_EXPORT VxVector &operator=(const VxCompressedVector &v);
    ///@}

    /**
     * @brief Computes the dot product with another vector.
     * @param iV The other vector.
     * @return The scalar dot product.
     */
    float Dot(const VxVector &iV) const { return x * iV.x + y * iV.y + z * iV.z; }

    /**
     * @brief Adds a scalar value to all components.
     * @param s The scalar value to add.
     * @return A new vector with the result.
     */
    VxVector operator+(float s) const { return VxVector(x + s, y + s, z + s); }

    /**
     * @brief Subtracts a scalar value from all components.
     * @param s The scalar value to subtract.
     * @return A new vector with the result.
     */
    VxVector operator-(float s) const { return VxVector(x - s, y - s, z - s); }

    /** @name Unary Operators */
    ///@{
    friend const VxVector operator+(const VxVector &v);
    friend const VxVector operator-(const VxVector &v);
    ///@}

    /** @name Binary Operators */
    ///@{
    friend const VxVector operator+(const VxVector &v1, const VxVector &v2);
    friend const VxVector operator-(const VxVector &v1, const VxVector &v2);
    friend const VxVector operator*(const VxVector &v, float s);
    friend const VxVector operator*(float s, const VxVector &v);
    friend const VxVector operator/(const VxVector &v, float s);
    friend const VxVector operator*(const VxVector &v1, const VxVector &v2);
    friend const VxVector operator/(const VxVector &v1, const VxVector &v2);
    friend int operator<(const VxVector &v1, const VxVector &v2);
    friend int operator<=(const VxVector &v1, const VxVector &v2);
    friend int operator==(const VxVector &v1, const VxVector &v2);
    friend int operator!=(const VxVector &v1, const VxVector &v2);
    friend float Min(const VxVector &v);
    friend float Max(const VxVector &v);
    friend const VxVector Minimize(const VxVector &v1, const VxVector &v2);
    friend const VxVector Maximize(const VxVector &v1, const VxVector &v2);
    friend const VxVector Interpolate(float step, const VxVector &v1, const VxVector &v2);
    ///@}

    /**
     * @brief Normalizes the vector, making its length equal to 1.
     * @warning The result is undefined for a null vector (0,0,0).
     */
    inline void Normalize() {
        const float magSq = SquareMagnitude();
        if (magSq > EPSILON) {
            const float invMag = 1.0f / sqrtf(magSq);
            x *= invMag;
            y *= invMag;
            z *= invMag;
        }
    }

    /**
     * @brief Rotates the vector by applying a transformation matrix.
     * @param M The rotation matrix to apply.
     */
    void Rotate(const VxMatrix &M);

    /** @name Predefined Vectors */
    ///@{
    VX_EXPORT static const VxVector &axisX(); ///< Returns a unit vector along the X-axis (1,0,0).
    VX_EXPORT static const VxVector &axisY(); ///< Returns a unit vector along the Y-axis (0,1,0).
    VX_EXPORT static const VxVector &axisZ(); ///< Returns a unit vector along the Z-axis (0,0,1).
    VX_EXPORT static const VxVector &axis0(); ///< Returns a zero vector (0,0,0).
    VX_EXPORT static const VxVector &axis1(); ///< Returns a unit vector (1,1,1).
    ///@}

private:
    /** @name Static Vector Constants */
    ///@{
    const static VxVector m_AxisX; ///< Unit vector along the X-axis.
    const static VxVector m_AxisY; ///< Unit vector along the Y-axis.
    const static VxVector m_AxisZ; ///< Unit vector along the Z-axis.
    const static VxVector m_Axis0; ///< Zero vector.
    const static VxVector m_Axis1; ///< Unit vector (1,1,1).
    ///@}
};

/** @name Vector Standalone Functions */
///@{

/**
 * @brief Returns the squared magnitude (length) of a vector.
 * @param v The vector.
 * @return The square of the vector's length (x*x + y*y + z*z).
 * @remarks This is more efficient than Magnitude() as it avoids a square root operation.
 * @see Magnitude()
 */
inline float SquareMagnitude(const VxVector &v) { return v.x * v.x + v.y * v.y + v.z * v.z; }

/**
 * @brief Returns the magnitude (length) of a vector.
 * @param v The vector.
 * @return The length of the vector (sqrt(x*x + y*y + z*z)).
 * @warning Calling this on a null vector (0,0,0) will result in NaN (Not aNumber).
 * @see SquareMagnitude()
 */
inline float Magnitude(const VxVector &v) { return sqrtf(SquareMagnitude(v)); }

/**
 * @brief Returns the inverse of the squared magnitude of a vector.
 * @param v The vector.
 * @return 1.0f / (x*x + y*y + z*z).
 * @warning The result is undefined for a null vector (0,0,0).
 * @see SquareMagnitude()
 */
inline float InvSquareMagnitude(const VxVector &v) { return 1.0f / SquareMagnitude(v); }

/**
 * @brief Returns the inverse of the magnitude of a vector.
 * @param v The vector.
 * @return 1.0f / sqrt(x*x + y*y + z*z).
 * @warning Calling this on a null vector (0,0,0) will result in NaN (Not aNumber).
 * @see Magnitude()
 */
inline float InvMagnitude(const VxVector &v) { return 1.0f / Magnitude(v); }

/**
 * @brief Returns a normalized copy of a vector.
 * @param v The vector to normalize.
 * @return A new unit vector pointing in the same direction as v.
 * @warning Returns a vector with NaN components for a null input vector (0,0,0).
 * @remarks This free function may be more precise than the member function `VxVector::Normalize()`.
 * @see VxVector::Normalize(), Magnitude()
 */
inline const VxVector Normalize(const VxVector &v) { return v * InvMagnitude(v); }
inline const VxVector Normalize(const VxVector *vect) { return Normalize(*vect); }

/**
 * @brief Calculates the dot product of two vectors.
 * @param v1 The first vector.
 * @param v2 The second vector.
 * @return The scalar dot product.
 * @see VxVector::Dot()
 */
inline float DotProduct(const VxVector &v1, const VxVector &v2) {
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

/**
 * @brief Calculates the cross product of two vectors.
 * @param Vect1 The first vector.
 * @param Vect2 The second vector.
 * @return A new vector perpendicular to both input vectors (Vect1 x Vect2).
 */
inline const VxVector CrossProduct(const VxVector &Vect1, const VxVector &Vect2) {
    return VxVector(Vect1.y * Vect2.z - Vect1.z * Vect2.y,
                    Vect1.z * Vect2.x - Vect1.x * Vect2.z, Vect1.x * Vect2.y - Vect1.y * Vect2.x);
}

/**
 * @brief Reflects an incident vector about a surface normal.
 * @param v1 The incident vector (should point towards the surface).
 * @param Norm The normal of the reflection plane (should be a unit vector).
 * @return The reflected vector, calculated as R = v1 - 2 * (v1 . N) * N.
 * @remarks For a correct reflection, the incident vector should point towards the surface.
 * @see DotProduct()
 */
inline const VxVector Reflect(const VxVector &v1, const VxVector &Norm) {
    float dp2 = 2.0f * (DotProduct(v1, Norm));
    return VxVector(v1.x - dp2 * Norm.x, v1.y - dp2 * Norm.y, v1.z - dp2 * Norm.z);
}

/// @brief Rotates a vector using a transformation matrix.
inline const VxVector Rotate(const VxMatrix &mat, const VxVector &pt);
/// @brief Rotates a vector around an axis by a specified angle.
inline const VxVector Rotate(const VxVector &v1, const VxVector &v2, float angle) {
    const float axisMagSq = v2.x * v2.x + v2.y * v2.y + v2.z * v2.z;
    const float axisInvMag = 1.0f / sqrtf(axisMagSq);
    const float nx = v2.x * axisInvMag;
    const float ny = v2.y * axisInvMag;
    const float nz = v2.z * axisInvMag;

    const float sinAngle = sinf(angle);
    const float cosAngle = cosf(angle);
    const float oneMinusCos = 1.0f - cosAngle;

    const float nx2 = nx * nx, ny2 = ny * ny, nz2 = nz * nz;
    const float nx_ny_omc = nx * ny * oneMinusCos;
    const float nx_nz_omc = nx * nz * oneMinusCos;
    const float ny_nz_omc = ny * nz * oneMinusCos;
    const float sin_nx = sinAngle * nx;
    const float sin_ny = sinAngle * ny;
    const float sin_nz = sinAngle * nz;

    return VxVector(
        v1.x * (cosAngle + oneMinusCos * nx2) + v1.y * (nx_ny_omc + sin_nz) + v1.z * (nx_nz_omc - sin_ny),
        v1.x * (nx_ny_omc - sin_nz) + v1.y * (cosAngle + oneMinusCos * ny2) + v1.z * (ny_nz_omc + sin_nx),
        v1.x * (nx_nz_omc + sin_ny) + v1.y * (ny_nz_omc - sin_nx) + v1.z * (cosAngle + oneMinusCos * nz2)
    );
}
/// @brief Rotates a vector around an axis by a specified angle (pointer version).
inline const VxVector Rotate(const VxVector *v1, const VxVector *v2, float angle) {
    return Rotate(*v1, *v2, angle);
}

///@}

/**
 * @struct VxCompressedVector
 * @brief A memory-efficient representation of a unit vector using two 16-bit polar angles.
 *
 * @remarks
 * This structure reduces the memory usage for a unit vector from 12 bytes (three floats)
 * to 4 bytes (two shorts). It is primarily used for storing normal vectors where precision
 * is less critical than memory footprint.
 *
 * @see VxVector, VxCompressedVectorOld
 */
typedef struct VxCompressedVector {
    short int xa; ///< The polar angle for the x-axis rotation.
    short int ya; ///< The polar angle for the y-axis rotation.

    /// @brief Default constructor. Initializes angles to 0.
    VxCompressedVector() { xa = ya = 0; }
    /// @brief Constructs and sets the compressed vector from three float components.
    VxCompressedVector(float _x, float _y, float _z) { Set(_x, _y, _z); }

    /// @brief Sets the compressed vector from a 3D vector's components.
    void Set(float X, float Y, float Z);

    /// @brief Performs spherical linear interpolation between two compressed vectors.
    void Slerp(float step, VxCompressedVector &v1, VxCompressedVector &v2);

    /// @brief Assigns a VxVector to this compressed vector, performing the conversion.
    VxCompressedVector &operator=(const VxVector &v);
    /// @brief Assigns an old format compressed vector to this one.
    VxCompressedVector &operator=(const VxCompressedVectorOld &v);
} VxCompressedVector;

/**
 * @struct VxCompressedVectorOld
 * @brief A legacy version of VxCompressedVector using 32-bit integers for angles.
 * @deprecated Prefer `VxCompressedVector` for new development.
 */
typedef struct VxCompressedVectorOld {
    int xa; ///< The polar angle for the x-axis rotation.
    int ya; ///< The polar angle for the y-axis rotation.

    /// @brief Default constructor. Initializes angles to 0.
    VxCompressedVectorOld() { xa = ya = 0; }
    /// @brief Constructs and sets the compressed vector from three float components.
    VxCompressedVectorOld(float _x, float _y, float _z) { Set(_x, _y, _z); }

    /// @brief Sets the compressed vector from a 3D vector's components.
    void Set(float X, float Y, float Z);
    /// @brief Performs spherical linear interpolation between two old compressed vectors.
    void Slerp(float step, VxCompressedVectorOld &v1, VxCompressedVectorOld &v2);

    /// @brief Assigns a VxVector to this compressed vector.
    VxCompressedVectorOld &operator=(const VxVector &v);
    /// @brief Assigns a modern compressed vector to this old format one.
    VxCompressedVectorOld &operator=(const VxCompressedVector &v);
} VxCompressedVectorOld;

/**
 * @class VxVector4
 * @brief A 4D vector class, extending VxVector with a 'w' component.
 *
 * @remarks
 * VxVector4 is suitable for homogeneous coordinates used in 3D transformations,
 * particularly with projection matrices where the 'w' component is essential
 * for perspective division.
 *
 * @see VxVector
 */
class VxVector4 : public VxVector {
public:
    float w; ///< The w-component of the vector.

    /// @brief Default constructor. Initializes to (0,0,0,0).
    VxVector4() { x = y = z = w = 0.0f; }
    /// @brief Uniform constructor. Initializes all four components to the same value.
    VxVector4(float f) { x = y = z = w = f; }
    /// @brief Component-wise constructor.
    VxVector4(float _x, float _y, float _z, float _w) {
        x = _x; y = _y; z = _z; w = _w;
    }
    /// @brief Array constructor.
    VxVector4(const float f[4]) {
        x = f[0]; y = f[1]; z = f[2]; w = f[3];
    }

    /**
     * @brief Assignment from a 3D vector. The 'w' component is not modified.
     * @param v The 3D vector to assign from.
     * @return A reference to this modified vector.
     */
    VxVector4 &operator=(const VxVector &v) {
        x = v.x; y = v.y; z = v.z;
        return *this;
    }

    // =====================================
    // Access grants
    // =====================================
    /// @brief Provides const array-style access to components.
    const float &operator[](int i) const;
    /// @brief Provides mutable array-style access to components.
    float &operator[](int i);

    /**
     * @brief Implicit conversion to a float pointer for direct memory access.
     * @return A pointer to the first component (x).
     */
#if !defined(_MSC_VER)
    operator float *() const { return (float *)&x; }
#else
    operator float *() const { return (float *) &v[0]; }
#endif

    // Initialization
    /// @brief Sets all four components of the vector.
    void Set(float X, float Y, float Z, float W);
    /// @brief Sets the x, y, and z components, leaving w unmodified.
    void Set(float X, float Y, float Z);
    /// @brief Computes the 3D dot product (ignoring the w component).
    float Dot(const VxVector4 &iV) const { return x * iV.x + y * iV.y + z * iV.z; }

    /// @brief Component-wise addition and assignment.
    VxVector4 &operator+=(const VxVector4 &v);
    /// @brief Component-wise subtraction and assignment.
    VxVector4 &operator-=(const VxVector4 &v);
    /// @brief Component-wise multiplication and assignment.
    VxVector4 &operator*=(const VxVector4 &v);
    /// @brief Component-wise division and assignment.
    VxVector4 &operator/=(const VxVector4 &v);

    /// @brief Component-wise addition and assignment with a 3D vector.
    VxVector4 &operator+=(const VxVector &v);
    /// @brief Component-wise subtraction and assignment with a 3D vector.
    VxVector4 &operator-=(const VxVector &v);
    /// @brief Component-wise multiplication and assignment with a 3D vector.
    VxVector4 &operator*=(const VxVector &v);
    /// @brief Component-wise division and assignment with a 3D vector.
    VxVector4 &operator/=(const VxVector &v);

    /// @brief Scalar multiplication and assignment.
    VxVector4 &operator*=(float s);
    /// @brief Scalar division and assignment.
    VxVector4 &operator/=(float s);
    /// @brief Adds a scalar to all components.
    VxVector4 operator+(float s) const { return VxVector4(x + s, y + s, z + s, w + s); }
    /// @brief Subtracts a scalar from all components.
    VxVector4 operator-(float s) const { return VxVector4(x - s, y - s, z - s, w - s); }

    // =====================================
    // Unary operators
    // =====================================
    friend const VxVector4 operator+(const VxVector4 &v);
    friend const VxVector4 operator-(const VxVector4 &v);

    // =====================================
    // Binary operators
    // =====================================
    friend const VxVector4 operator+(const VxVector4 &v1, const VxVector4 &v2);
    friend const VxVector4 operator-(const VxVector4 &v1, const VxVector4 &v2);
    friend const VxVector4 operator*(const VxVector4 &v, float s);
    friend const VxVector4 operator*(float s, const VxVector4 &v);
    friend const VxVector4 operator/(const VxVector4 &v, float s);
    friend const VxVector4 operator*(const VxVector4 &v1, const VxVector4 &v2);
    friend const VxVector4 operator/(const VxVector4 &v1, const VxVector4 &v2);

    // Bitwise equality
    friend int operator==(const VxVector4 &v1, const VxVector4 &v2);
    friend int operator!=(const VxVector4 &v1, const VxVector4 &v2);
};

// =================================================================
// VxVector Inline Implementations
// =================================================================

inline VxVector::VxVector() : x(0), y(0), z(0) {}
inline VxVector::VxVector(float f) : x(f), y(f), z(f) {}
inline VxVector::VxVector(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
inline VxVector::VxVector(const float f[3]) : x(f[0]), y(f[1]), z(f[2]) {}
inline void VxVector::Set(float _x, float _y, float _z) { x = _x; y = _y; z = _z; }
inline const float &VxVector::operator[](int i) const { return *((&x) + i); }
inline float &VxVector::operator[](int i) { return *((&x) + i); }
inline VxVector &VxVector::operator+=(const VxVector &v) { x += v.x; y += v.y; z += v.z; return *this; }
inline VxVector &VxVector::operator-=(const VxVector &v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
inline VxVector &VxVector::operator*=(const VxVector &v) { x *= v.x; y *= v.y; z *= v.z; return *this; }
inline VxVector &VxVector::operator/=(const VxVector &v) { x /= v.x; y /= v.y; z /= v.z; return *this; }
inline VxVector &VxVector::operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
inline VxVector &VxVector::operator/=(float s) { float temp = 1.0f / s; x *= temp; y *= temp; z *= temp; return *this; }

inline const VxVector operator+(const VxVector &v) { return v; }
inline const VxVector operator-(const VxVector &v) { return VxVector(-v.x, -v.y, -v.z); }
inline const VxVector operator+(const VxVector &v1, const VxVector &v2) { return VxVector(v1.x + v2.x, v1.y + v2.y, v1.z + v2.z); }
inline const VxVector operator-(const VxVector &v1, const VxVector &v2) { return VxVector(v1.x - v2.x, v1.y - v2.y, v1.z - v2.z); }
inline const VxVector operator*(const VxVector &v1, const VxVector &v2) { return VxVector(v1.x * v2.x, v1.y * v2.y, v1.z * v2.z); }
inline const VxVector operator/(const VxVector &v1, const VxVector &v2) { return VxVector(v1.x / v2.x, v1.y / v2.y, v1.z / v2.z); }
inline int operator<(const VxVector &v1, const VxVector &v2) { return v1[0] < v2[0] && v1[1] < v2[1] && v1[2] < v2[2]; }
inline int operator<=(const VxVector &v1, const VxVector &v2) { return v1[0] <= v2[0] && v1[1] <= v2[1] && v1[2] <= v2[2]; }
inline const VxVector operator*(const VxVector &v, float s) { return VxVector(s * v.x, s * v.y, s * v.z); }
inline const VxVector operator*(float s, const VxVector &v) { return VxVector(s * v.x, s * v.y, s * v.z); }
inline const VxVector operator/(const VxVector &v, float s) { float temp = 1.0f / s; return VxVector(v.x * temp, v.y * temp, v.z * temp); }
inline int operator==(const VxVector &v1, const VxVector &v2) { return ((v1.x == v2.x) && (v1.y == v2.y) && (v1.z == v2.z)); }
inline int operator!=(const VxVector &v1, const VxVector &v2) { return !(v1 == v2); }

/**
 * @brief Returns a vector with each component set to its absolute value.
 * @param v The input vector.
 * @return A new vector with absolute values of the components of v.
 */
inline const VxVector Absolute(const VxVector &v) { return VxVector(XAbs(v.x), XAbs(v.y), XAbs(v.z)); }
inline void VxVector::Absolute() { x = XAbs(x); y = XAbs(y); z = XAbs(z); }

/**
 * @brief Calculates the minimum value among the components of a vector.
 * @param v The vector.
 * @return The minimum component value of v.
 */
inline float Min(const VxVector &v) { return XMin(v.x, v.y, v.z); }

/**
 * @brief Calculates the maximum value among the components of a vector.
 * @param v The vector.
 * @return The maximum component value of v.
 */
inline float Max(const VxVector &v) {
    float ret = v.x;
    if (ret < v.y) ret = v.y;
    if (ret < v.z) ret = v.z;
    return ret;
}

/**
 * @brief Constructs a vector from the component-wise minimum of two vectors.
 * @param v1 The first vector.
 * @param v2 The second vector.
 * @return A vector where each component is the minimum of the corresponding components of v1 and v2.
 */
inline const VxVector Minimize(const VxVector &v1, const VxVector &v2) {
    return VxVector(XMin(v1[0], v2[0]), XMin(v1[1], v2[1]), XMin(v1[2], v2[2]));
}

/**
 * @brief Constructs a vector from the component-wise maximum of two vectors.
 * @param v1 The first vector.
 * @param v2 The second vector.
 * @return A vector where each component is the maximum of the corresponding components of v1 and v2.
 */
inline const VxVector Maximize(const VxVector &v1, const VxVector &v2) {
    return VxVector(XMax(v1[0], v2[0]), XMax(v1[1], v2[1]), XMax(v1[2], v2[2]));
}

/**
 * @brief Performs a linear interpolation between two vectors.
 * @param step The interpolation factor, clamped between 0.0 (v1) and 1.0 (v2).
 * @param v1 The starting vector.
 * @param v2 The ending vector.
 * @return The interpolated vector.
 */
inline const VxVector Interpolate(float step, const VxVector &v1, const VxVector &v2) {
    return VxVector(v1.x + (v2.x - v1.x) * step,
                    v1.y + (v2.y - v1.y) * step,
                    v1.z + (v2.z - v1.z) * step);
}

// =================================================================
// VxVector4 Inline Implementations
// =================================================================

inline VxVector4 &VxVector4::operator+=(const VxVector4 &v) { x += v.x; y += v.y; z += v.z; w += v.w; return *this; }
inline VxVector4 &VxVector4::operator-=(const VxVector4 &v) { x -= v.x; y -= v.y; z -= v.z; w -= v.w; return *this; }
inline VxVector4 &VxVector4::operator*=(const VxVector4 &v) { x *= v.x; y *= v.y; z *= v.z; w *= v.w; return *this; }
inline VxVector4 &VxVector4::operator/=(const VxVector4 &v) { x /= v.x; y /= v.y; z /= v.z; w /= v.w; return *this; }
inline VxVector4 &VxVector4::operator+=(const VxVector &v) { x += v.x; y += v.y; z += v.z; return *this; }
inline VxVector4 &VxVector4::operator-=(const VxVector &v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
inline VxVector4 &VxVector4::operator*=(const VxVector &v) { x *= v.x; y *= v.y; z *= v.z; return *this; }
inline VxVector4 &VxVector4::operator/=(const VxVector &v) { x /= v.x; y /= v.y; z /= v.z; return *this; }
inline VxVector4 &VxVector4::operator*=(float s) { x *= s; y *= s; z *= s; w *= s; return *this; }
inline VxVector4 &VxVector4::operator/=(float s) { float temp = 1.0f / s; x *= temp; y *= temp; z *= temp; w *= temp; return *this; }
inline const VxVector4 operator+(const VxVector4 &v) { return v; }
inline const VxVector4 operator-(const VxVector4 &v) { return VxVector4(-v.x, -v.y, -v.z, -v.w); }
inline const VxVector4 operator+(const VxVector4 &v1, const VxVector4 &v2) { return VxVector4(v1.x + v2.x, v1.y + v2.y, v1.z + v2.z, v1.w + v2.w); }
inline const VxVector4 operator-(const VxVector4 &v1, const VxVector4 &v2) { return VxVector4(v1.x - v2.x, v1.y - v2.y, v1.z - v2.z, v1.w - v2.w); }
inline const VxVector4 operator*(const VxVector4 &v1, const VxVector4 &v2) { return VxVector4(v1.x * v2.x, v1.y * v2.y, v1.z * v2.z, v1.w * v2.w); }
inline const VxVector4 operator/(const VxVector4 &v1, const VxVector4 &v2) { return VxVector4(v1.x / v2.x, v1.y / v2.y, v1.z / v2.z, v1.w / v2.w); }
inline const VxVector4 operator*(const VxVector4 &v, float s) { return VxVector4(s * v.x, s * v.y, s * v.z, s * v.w); }
inline const VxVector4 operator*(float s, const VxVector4 &v) { return VxVector4(s * v.x, s * v.y, s * v.z, s * v.w); }
inline const VxVector4 operator/(const VxVector4 &v, float s) { float invs = 1.0f / s; return VxVector4(invs * v.x, invs * v.y, invs * v.z, invs * v.w); }
inline int operator==(const VxVector4 &v1, const VxVector4 &v2) { return ((v1.x == v2.x) && (v1.y == v2.y) && (v1.z == v2.z) && (v1.w == v2.w)); }
inline int operator!=(const VxVector4 &v1, const VxVector4 &v2) { return !(v1 == v2); }
inline void VxVector4::Set(float _x, float _y, float _z, float _w) { x = _x; y = _y; z = _z; w = _w; }
inline void VxVector4::Set(float _x, float _y, float _z) { x = _x; y = _y; z = _z; }
inline const float &VxVector4::operator[](int i) const { return *((&x) + i); }
inline float &VxVector4::operator[](int i) { return *((&x) + i); }

// =================================================================
// VxCompressedVector Inline Implementations
// =================================================================

/**
 * @brief Performs spherical linear interpolation between two compressed vectors.
 * @param step The interpolation factor (0 for v1, 1 for v2).
 * @param v1 The starting vector.
 * @param v2 The ending vector.
 */
inline void VxCompressedVectorOld::Slerp(float step, VxCompressedVectorOld &v1, VxCompressedVectorOld &v2) {
    int v1y = ((int) v1.ya + 16384) & 16383;
    int v2y = ((int) v2.ya + 16384) & 16383;
    v2y = (v2y - v1y);
    if (v2y > 8192) v2y = 16384 - v2y;
    else if (v2y < -8192) v2y = 16384 + v2y;
    xa = (int) ((float) v1.xa + (float) (v2.xa - v1.xa) * step);
    ya = (int) ((float) v1y + (float) v2y * step);
}

inline VxCompressedVectorOld &VxCompressedVectorOld::operator=(const VxVector &v) {
    Set(v.x, v.y, v.z);
    return *this;
}

/**
 * @brief Sets the compressed vector from 3 float components.
 * @param X The x-component.
 * @param Y The y-component.
 * @param Z The z-component.
 */
inline void VxCompressedVectorOld::Set(float X, float Y, float Z) {
    xa = -radToAngle((float) asin(Y));
    ya = radToAngle((float) atan2(X, Z));
}

inline void VxCompressedVector::Slerp(float step, VxCompressedVector &v1, VxCompressedVector &v2) {
    int coef = (int) (65536.0f * step);
    int v1y = ((int) v1.ya + 16384) & 16383;
    int v2y = ((int) v2.ya + 16384) & 16383;
    v2y = (v2y - v1y);
    if (v2y > 8192) v2y = 16384 - v2y;
    else if (v2y < -8192) v2y = 16384 + v2y;
    xa = (short int) ((int) v1.xa + (((v2.xa - v1.xa) * coef) >> 16));
    ya = (short int) (v1y + ((v2y * coef) >> 16));
}

inline VxCompressedVector &VxCompressedVector::operator=(const VxVector &v) {
    Set(v.x, v.y, v.z);
    return *this;
}

inline void VxCompressedVector::Set(float X, float Y, float Z) {
    xa = (short int) -radToAngle((float) asin(Y));
    ya = (short int) radToAngle((float) atan2(X, Z));
}

inline VxCompressedVectorOld &VxCompressedVectorOld::operator=(const VxCompressedVector &v) {
    xa = (int) v.xa;
    ya = (int) v.ya;
    return *this;
}

inline VxCompressedVector &VxCompressedVector::operator=(const VxCompressedVectorOld &v) {
    xa = (short int) v.xa;
    ya = (short int) v.ya;
    return *this;
}

#endif // VXVECTOR_H
