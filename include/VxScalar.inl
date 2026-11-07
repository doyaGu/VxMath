#ifndef VXSCALAR_INL
#define VXSCALAR_INL

#if VXMATH_ENABLE_X87_EMULATION
#include "VxFPUControlWord.h"

inline void VxScalarInitContext(VxScalarContext &context) { VxFloatInitContext(context, VxGetFPUControlWord()); }

inline VxScalar VxScalarLoad(VxScalarContext &context, float value) { return VxFloat80Load(context, value); }

inline VxScalar VxScalarLoad(VxScalarContext &context, double value) { return VxFloat80Load(context, value); }

inline VxScalar VxScalarLoad(VxScalarContext &context, int32_t value) { return VxFloat80Load(context, value); }

inline float VxScalarStore(VxScalarContext &context, const VxScalar &value) {
    return VxFloat80StoreFloat(context, value);
}

inline double VxScalarStoreDouble(VxScalarContext &context, const VxScalar &value) {
    return VxFloat80StoreDouble(context, value);
}

inline VxScalar VxScalarAdd(VxScalarContext &context, const VxScalar &first, const VxScalar &second) {
    return VxFloat80Add(context, first, second);
}

inline VxScalar VxScalarSubtract(VxScalarContext &context, const VxScalar &first, const VxScalar &second) {
    return VxFloat80Subtract(context, first, second);
}

inline VxScalar VxScalarMultiply(VxScalarContext &context, const VxScalar &first, const VxScalar &second) {
    return VxFloat80Multiply(context, first, second);
}

inline VxScalar VxScalarDivide(VxScalarContext &context, const VxScalar &first, const VxScalar &second) {
    return VxFloat80Divide(context, first, second);
}

inline VxScalar VxScalarSquareRoot(VxScalarContext &context, const VxScalar &value) {
    return VxFloat80SquareRoot(context, value);
}

inline VxScalar VxScalarAbsolute(VxScalarContext &context, const VxScalar &value) {
    return VxFloat80Absolute(context, value);
}

inline VxScalar VxScalarNegate(VxScalarContext &context, const VxScalar &value) {
    return VxFloat80Negate(context, value);
}

inline VxScalarRelation VxScalarCompare(VxScalarContext &context, const VxScalar &first, const VxScalar &second) {
    return static_cast<VxScalarRelation>(VxFloat80Compare(context, first, second));
}

#else
#include <math.h>

inline void VxScalarInitContext(VxScalarContext &context) { (void)context; }

inline VxScalar VxScalarLoad(VxScalarContext &context, float value) {
    (void)context;
    return value;
}

inline VxScalar VxScalarLoad(VxScalarContext &context, double value) {
    (void)context;
    return value;
}

inline VxScalar VxScalarLoad(VxScalarContext &context, int32_t value) {
    (void)context;
    return value;
}

inline float VxScalarStore(VxScalarContext &context, const VxScalar &value) {
    (void)context;
    volatile float stored = static_cast<float>(value);
    return stored;
}

inline double VxScalarStoreDouble(VxScalarContext &context, const VxScalar &value) {
    (void)context;
    volatile double stored = value;
    return stored;
}

inline VxScalar VxScalarAdd(VxScalarContext &context, const VxScalar &first, const VxScalar &second) {
    (void)context;
    return first + second;
}

inline VxScalar VxScalarSubtract(VxScalarContext &context, const VxScalar &first, const VxScalar &second) {
    (void)context;
    return first - second;
}

inline VxScalar VxScalarMultiply(VxScalarContext &context, const VxScalar &first, const VxScalar &second) {
    (void)context;
    return first * second;
}

inline VxScalar VxScalarDivide(VxScalarContext &context, const VxScalar &first, const VxScalar &second) {
    (void)context;
    return first / second;
}

inline VxScalar VxScalarSquareRoot(VxScalarContext &context, const VxScalar &value) {
    (void)context;
    return sqrt(value);
}

inline VxScalar VxScalarAbsolute(VxScalarContext &context, const VxScalar &value) {
    (void)context;
    return fabs(value);
}

inline VxScalar VxScalarNegate(VxScalarContext &context, const VxScalar &value) {
    (void)context;
    return -value;
}

inline VxScalarRelation VxScalarCompare(VxScalarContext &context, const VxScalar &first, const VxScalar &second) {
    (void)context;
    if (first < second)
        return VXSCALAR_LESS;
    if (first > second)
        return VXSCALAR_GREATER;
    return first == second ? VXSCALAR_EQUAL : VXSCALAR_UNORDERED;
}

#endif

#endif // VXSCALAR_INL
