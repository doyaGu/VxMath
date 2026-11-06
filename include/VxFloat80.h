#ifndef VXFLOAT80_H
#define VXFLOAT80_H

#include "VxMathDefines.h"

// Register values are independent of the host ABI. Serialize with the byte
// functions: sizeof(VxFloat80) may include alignment padding.
struct VxFloat80 {
    uint64_t Significand;
    uint16_t SignExponent;
};

enum VxFloatPrecision { VXFLOAT_PRECISION_24 = 24, VXFLOAT_PRECISION_53 = 53, VXFLOAT_PRECISION_64 = 64 };
enum VxFloatRounding { VXFLOAT_ROUND_NEAREST, VXFLOAT_ROUND_DOWN, VXFLOAT_ROUND_UP, VXFLOAT_ROUND_TO_ZERO };
enum VxFloatRelation { VXFLOAT_LESS = -1, VXFLOAT_EQUAL, VXFLOAT_GREATER, VXFLOAT_UNORDERED };
enum VxFloatException {
    VXFLOAT_EXCEPTION_INVALID_OPERATION = 0x01,
    VXFLOAT_EXCEPTION_DENORMAL_INPUT = 0x02,
    VXFLOAT_EXCEPTION_DIVISION_BY_ZERO = 0x04,
    VXFLOAT_EXCEPTION_OVERFLOW = 0x08,
    VXFLOAT_EXCEPTION_UNDERFLOW = 0x10,
    VXFLOAT_EXCEPTION_INEXACT_RESULT = 0x20
};

// Arithmetic state is plain data, independent of the numerical backend.
// Operations accumulate masked exceptions; initialize before first use.
struct VxFloatContext {
    VxFloatPrecision Precision;
    VxFloatRounding Rounding;
    unsigned ExceptionFlags;
};

VX_EXPORT void VxFloatInitContext(VxFloatContext &context, VxFloatPrecision precision = VXFLOAT_PRECISION_64,
                                  VxFloatRounding rounding = VXFLOAT_ROUND_NEAREST);
VX_EXPORT void VxFloatInitContext(VxFloatContext &context, XWORD controlWord);
VX_EXPORT VxFloat80 VxFloat80FromBits(uint16_t signExponent, uint64_t significand);
VX_EXPORT VxFloat80 VxFloat80FromBytes(const unsigned char *bytes);
VX_EXPORT void VxFloat80ToBytes(const VxFloat80 &value, unsigned char *bytes);
VX_EXPORT void VxFloatSetPrecision(VxFloatContext &context, VxFloatPrecision precision);
VX_EXPORT void VxFloatSetRounding(VxFloatContext &context, VxFloatRounding rounding);
VX_EXPORT unsigned VxFloatGetExceptionFlags(const VxFloatContext &context);
VX_EXPORT void VxFloatClearExceptionFlags(VxFloatContext &context);
VX_EXPORT VxFloat80 VxFloat80Load(VxFloatContext &context, float value);
VX_EXPORT VxFloat80 VxFloat80Load(VxFloatContext &context, double value);
VX_EXPORT VxFloat80 VxFloat80Load(VxFloatContext &context, int32_t value);
VX_EXPORT float VxFloat80StoreFloat(VxFloatContext &context, const VxFloat80 &value);
VX_EXPORT double VxFloat80StoreDouble(VxFloatContext &context, const VxFloat80 &value);
VX_EXPORT VxFloat80 VxFloat80Add(VxFloatContext &context, const VxFloat80 &first, const VxFloat80 &second);
VX_EXPORT VxFloat80 VxFloat80Subtract(VxFloatContext &context, const VxFloat80 &first, const VxFloat80 &second);
VX_EXPORT VxFloat80 VxFloat80Multiply(VxFloatContext &context, const VxFloat80 &first, const VxFloat80 &second);
VX_EXPORT VxFloat80 VxFloat80Divide(VxFloatContext &context, const VxFloat80 &first, const VxFloat80 &second);
VX_EXPORT VxFloat80 VxFloat80SquareRoot(VxFloatContext &context, const VxFloat80 &value);
VX_EXPORT VxFloat80 VxFloat80Absolute(VxFloatContext &context, const VxFloat80 &value);
VX_EXPORT VxFloat80 VxFloat80Negate(VxFloatContext &context, const VxFloat80 &value);
VX_EXPORT VxFloatRelation VxFloat80Compare(VxFloatContext &context, const VxFloat80 &first, const VxFloat80 &second,
                                           bool quiet = false);
VX_EXPORT VxFloat80 VxFloat80Sine(VxFloatContext &context, const VxFloat80 &value, bool *outOfRange = nullptr);
VX_EXPORT VxFloat80 VxFloat80Cosine(VxFloatContext &context, const VxFloat80 &value, bool *outOfRange = nullptr);
VX_EXPORT VxFloat80 VxFloat80Arctangent(VxFloatContext &context, const VxFloat80 &y, const VxFloat80 &x);
VX_EXPORT VxFloat80 VxFloat80Power2Minus1(VxFloatContext &context, const VxFloat80 &value);
VX_EXPORT VxFloat80 VxFloat80Log2Multiply(VxFloatContext &context, const VxFloat80 &x, const VxFloat80 &y);
VX_EXPORT VxFloat80 VxFloat80RoundInteger(VxFloatContext &context, const VxFloat80 &value);
VX_EXPORT VxFloat80 VxFloat80Scale(VxFloatContext &context, const VxFloat80 &value, const VxFloat80 &exponent);
VX_EXPORT VxFloat80 VxFloat80NaturalLogarithmOfTwo(const VxFloatContext &context);
VX_EXPORT VxFloat80 VxFloat80Logarithm2OfE(const VxFloatContext &context);
VX_EXPORT VxFloat80 VxFloat80Exponential(VxFloatContext &context, const VxFloat80 &value);

#endif
