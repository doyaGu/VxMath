#ifndef VXSCALAR_H
#define VXSCALAR_H

#include "VxMathDefines.h"

#include <stdint.h>

#if VXMATH_ENABLE_X87_EMULATION
#include "VxFloat80.h"

typedef VxFloat80 VxScalar;
typedef VxFloatContext VxScalarContext;
#else
typedef double VxScalar;
struct VxScalarContext {};
#endif

enum VxScalarRelation { VXSCALAR_LESS = -1, VXSCALAR_EQUAL, VXSCALAR_GREATER, VXSCALAR_UNORDERED };

// Scalar expressions use VxMath's selected floating-point evaluation. Initialize
// one context per expression sequence and store at the required float/double
// boundary. Values are plain data; their representation is not a serialization
// format. Native scalar operations use double intermediates; this interface does not
// replace the usual C++ float/double promotion rules in arbitrary expressions.
inline void VxScalarInitContext(VxScalarContext &context);

inline VxScalar VxScalarLoad(VxScalarContext &context, float value);
inline VxScalar VxScalarLoad(VxScalarContext &context, double value);
inline VxScalar VxScalarLoad(VxScalarContext &context, int32_t value);

inline float VxScalarStore(VxScalarContext &context, const VxScalar &value);
inline double VxScalarStoreDouble(VxScalarContext &context, const VxScalar &value);

inline VxScalar VxScalarAdd(VxScalarContext &context, const VxScalar &first, const VxScalar &second);

inline VxScalar VxScalarSubtract(VxScalarContext &context, const VxScalar &first, const VxScalar &second);

inline VxScalar VxScalarMultiply(VxScalarContext &context, const VxScalar &first, const VxScalar &second);

inline VxScalar VxScalarDivide(VxScalarContext &context, const VxScalar &first, const VxScalar &second);

inline VxScalar VxScalarSquareRoot(VxScalarContext &context, const VxScalar &value);

inline VxScalar VxScalarAbsolute(VxScalarContext &context, const VxScalar &value);

inline VxScalar VxScalarNegate(VxScalarContext &context, const VxScalar &value);

inline VxScalarRelation VxScalarCompare(VxScalarContext &context, const VxScalar &first, const VxScalar &second);

#include "VxScalar.inl"

#endif // VXSCALAR_H
