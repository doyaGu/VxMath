#include "VxFloat80.h"

#include "vx_softfloat.h"
#include <assert.h>
#include <string.h>

static softfloat_status_t VxFloatStatus(const VxFloatContext &context) {
    softfloat_status_t status = {};
    status.softfloat_exceptionMasks = softfloat_all_exceptions_mask;
    status.extF80_roundingPrecision = context.Precision == VXFLOAT_PRECISION_24   ? 32
                                      : context.Precision == VXFLOAT_PRECISION_53 ? 64
                                                                                  : 80;
    status.softfloat_roundingMode = static_cast<uint8_t>(context.Rounding);
    status.softfloat_exceptionFlags = context.ExceptionFlags;
    return status;
}

static extFloat80_t VxFloatInput(const VxFloat80 &value) {
    extFloat80_t result;
    result.signif = value.Significand;
    result.signExp = value.SignExponent;
    return result;
}
static VxFloat80 VxFloatOutput(extFloat80_t value) {
    return VxFloat80FromBits(value.signExp, value.signif);
}

VxFloat80 VxFloat80FromBits(uint16_t signExponent, uint64_t significand) {
    const VxFloat80 result = {significand, signExponent};
    return result;
}
VxFloat80 VxFloat80FromBytes(const unsigned char *bytes) {
    uint64_t significand = 0;
    for (unsigned i = 0; i < 8; ++i)
        significand |= static_cast<uint64_t>(bytes[i]) << (i * 8);
    return VxFloat80FromBits(static_cast<uint16_t>(bytes[8] | (bytes[9] << 8)), significand);
}
void VxFloat80ToBytes(const VxFloat80 &value, unsigned char *bytes) {
    for (unsigned i = 0; i < 8; ++i)
        bytes[i] = static_cast<unsigned char>(value.Significand >> (i * 8));
    bytes[8] = static_cast<unsigned char>(value.SignExponent);
    bytes[9] = static_cast<unsigned char>(value.SignExponent >> 8);
}

void VxFloatInitContext(VxFloatContext &context, VxFloatPrecision precision, VxFloatRounding rounding) {
    context.ExceptionFlags = 0;
    VxFloatSetPrecision(context, precision);
    VxFloatSetRounding(context, rounding);
}
void VxFloatInitContext(VxFloatContext &context, XWORD controlWord) {
    const unsigned precision = controlWord & 0x0300;
    VxFloatInitContext(context,
                       precision == 0        ? VXFLOAT_PRECISION_24
                       : precision == 0x0200 ? VXFLOAT_PRECISION_53
                                             : VXFLOAT_PRECISION_64,
                       static_cast<VxFloatRounding>((controlWord >> 10) & 3));
}

void VxFloatSetPrecision(VxFloatContext &context, VxFloatPrecision precision) {
    assert(precision == VXFLOAT_PRECISION_24 || precision == VXFLOAT_PRECISION_53 || precision == VXFLOAT_PRECISION_64);
    context.Precision = precision;
}

void VxFloatSetRounding(VxFloatContext &context, VxFloatRounding rounding) {
    assert(rounding >= VXFLOAT_ROUND_NEAREST && rounding <= VXFLOAT_ROUND_TO_ZERO);
    context.Rounding = rounding;
}

unsigned VxFloatGetExceptionFlags(const VxFloatContext &context) {
    return context.ExceptionFlags;
}

void VxFloatClearExceptionFlags(VxFloatContext &context) {
    context.ExceptionFlags = 0;
}

VxFloat80 VxFloat80Load(VxFloatContext &context, float value) {
    softfloat_status_t status = VxFloatStatus(context);
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    const VxFloat80 output = VxFloatOutput(f32_to_extF80(bits, &status));
    context.ExceptionFlags = status.softfloat_exceptionFlags & 0x3f;
    return output;
}

VxFloat80 VxFloat80Load(VxFloatContext &context, double value) {
    softfloat_status_t status = VxFloatStatus(context);
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    const VxFloat80 output = VxFloatOutput(f64_to_extF80(bits, &status));
    context.ExceptionFlags = status.softfloat_exceptionFlags & 0x3f;
    return output;
}

VxFloat80 VxFloat80Load(VxFloatContext &, int32_t value) {
    return VxFloatOutput(i32_to_extF80(value));
}

float VxFloat80StoreFloat(VxFloatContext &context, const VxFloat80 &value) {
    softfloat_status_t status = VxFloatStatus(context);
    const uint32_t bits = extF80_to_f32(VxFloatInput(value), &status);
    float result;
    memcpy(&result, &bits, sizeof(result));
    context.ExceptionFlags = status.softfloat_exceptionFlags & 0x3f;
    return result;
}

double VxFloat80StoreDouble(VxFloatContext &context, const VxFloat80 &value) {
    softfloat_status_t status = VxFloatStatus(context);
    const uint64_t bits = extF80_to_f64(VxFloatInput(value), &status);
    double result;
    memcpy(&result, &bits, sizeof(result));
    context.ExceptionFlags = status.softfloat_exceptionFlags & 0x3f;
    return result;
}

VxFloat80 VxFloat80Add(VxFloatContext &context, const VxFloat80 &first, const VxFloat80 &second) {
    softfloat_status_t status = VxFloatStatus(context);
    const VxFloat80 output = VxFloatOutput(extF80_add(VxFloatInput(first), VxFloatInput(second), &status));
    context.ExceptionFlags = status.softfloat_exceptionFlags & 0x3f;
    return output;
}

VxFloat80 VxFloat80Subtract(VxFloatContext &context, const VxFloat80 &first, const VxFloat80 &second) {
    softfloat_status_t status = VxFloatStatus(context);
    const VxFloat80 output = VxFloatOutput(extF80_sub(VxFloatInput(first), VxFloatInput(second), &status));
    context.ExceptionFlags = status.softfloat_exceptionFlags & 0x3f;
    return output;
}

VxFloat80 VxFloat80Multiply(VxFloatContext &context, const VxFloat80 &first, const VxFloat80 &second) {
    softfloat_status_t status = VxFloatStatus(context);
    const VxFloat80 output = VxFloatOutput(extF80_mul(VxFloatInput(first), VxFloatInput(second), &status));
    context.ExceptionFlags = status.softfloat_exceptionFlags & 0x3f;
    return output;
}

VxFloat80 VxFloat80Divide(VxFloatContext &context, const VxFloat80 &first, const VxFloat80 &second) {
    softfloat_status_t status = VxFloatStatus(context);
    const VxFloat80 output = VxFloatOutput(extF80_div(VxFloatInput(first), VxFloatInput(second), &status));
    context.ExceptionFlags = status.softfloat_exceptionFlags & 0x3f;
    return output;
}

VxFloat80 VxFloat80SquareRoot(VxFloatContext &context, const VxFloat80 &value) {
    softfloat_status_t status = VxFloatStatus(context);
    const VxFloat80 output = VxFloatOutput(extF80_sqrt(VxFloatInput(value), &status));
    context.ExceptionFlags = status.softfloat_exceptionFlags & 0x3f;
    return output;
}

VxFloat80 VxFloat80Absolute(VxFloatContext &, const VxFloat80 &value) {
    return VxFloat80FromBits(value.SignExponent & 0x7fff, value.Significand);
}

VxFloat80 VxFloat80Negate(VxFloatContext &, const VxFloat80 &value) {
    return VxFloat80FromBits(value.SignExponent ^ 0x8000, value.Significand);
}

VxFloatRelation VxFloat80Compare(VxFloatContext &context, const VxFloat80 &first, const VxFloat80 &second, bool quiet) {
    softfloat_status_t status = VxFloatStatus(context);
    const VxFloatRelation output =
        static_cast<VxFloatRelation>(extF80_compare(VxFloatInput(first), VxFloatInput(second), quiet, &status));
    context.ExceptionFlags = status.softfloat_exceptionFlags & 0x3f;
    return output;
}

VxFloat80 VxFloat80Sine(VxFloatContext &context, const VxFloat80 &value, bool *outOfRange) {
    softfloat_status_t status = VxFloatStatus(context);
    extFloat80_t result = VxFloatInput(value);
    const int range = fsin(result, status);
    if (outOfRange)
        *outOfRange = range != 0;
    const VxFloat80 output = VxFloatOutput(result);
    context.ExceptionFlags = status.softfloat_exceptionFlags & 0x3f;
    return output;
}

VxFloat80 VxFloat80Cosine(VxFloatContext &context, const VxFloat80 &value, bool *outOfRange) {
    softfloat_status_t status = VxFloatStatus(context);
    extFloat80_t result = VxFloatInput(value);
    const int range = fcos(result, status);
    if (outOfRange)
        *outOfRange = range != 0;
    const VxFloat80 output = VxFloatOutput(result);
    context.ExceptionFlags = status.softfloat_exceptionFlags & 0x3f;
    return output;
}

VxFloat80 VxFloat80Arctangent(VxFloatContext &context, const VxFloat80 &y, const VxFloat80 &x) {
    softfloat_status_t status = VxFloatStatus(context);
    const VxFloat80 output = VxFloatOutput(fpatan(VxFloatInput(x), VxFloatInput(y), status));
    context.ExceptionFlags = status.softfloat_exceptionFlags & 0x3f;
    return output;
}

VxFloat80 VxFloat80Power2Minus1(VxFloatContext &context, const VxFloat80 &value) {
    softfloat_status_t status = VxFloatStatus(context);
    const VxFloat80 output = VxFloatOutput(f2xm1(VxFloatInput(value), status));
    context.ExceptionFlags = status.softfloat_exceptionFlags & 0x3f;
    return output;
}

VxFloat80 VxFloat80Log2Multiply(VxFloatContext &context, const VxFloat80 &x, const VxFloat80 &y) {
    softfloat_status_t status = VxFloatStatus(context);
    const VxFloat80 output = VxFloatOutput(fyl2x(VxFloatInput(x), VxFloatInput(y), status));
    context.ExceptionFlags = status.softfloat_exceptionFlags & 0x3f;
    return output;
}

VxFloat80 VxFloat80RoundInteger(VxFloatContext &context, const VxFloat80 &value) {
    softfloat_status_t status = VxFloatStatus(context);
    const VxFloat80 output = VxFloatOutput(extF80_roundToInt(VxFloatInput(value), &status));
    context.ExceptionFlags = status.softfloat_exceptionFlags & 0x3f;
    return output;
}

VxFloat80 VxFloat80Scale(VxFloatContext &context, const VxFloat80 &value, const VxFloat80 &exponent) {
    softfloat_status_t status = VxFloatStatus(context);
    const VxFloat80 output = VxFloatOutput(extF80_scale(VxFloatInput(value), VxFloatInput(exponent), &status));
    context.ExceptionFlags = status.softfloat_exceptionFlags & 0x3f;
    return output;
}

VxFloat80 VxFloat80NaturalLogarithmOfTwo(const VxFloatContext &context) {
    const unsigned mode = context.Rounding;
    const uint64_t significand =
        UINT64_C(0xb17217f7d1cf79ac) - (mode == VXFLOAT_ROUND_DOWN || mode == VXFLOAT_ROUND_TO_ZERO);
    return VxFloat80FromBits(0x3ffe, significand);
}

VxFloat80 VxFloat80Logarithm2OfE(const VxFloatContext &context) {
    const unsigned mode = context.Rounding;
    const uint64_t significand =
        UINT64_C(0xb8aa3b295c17f0bc) - (mode == VXFLOAT_ROUND_DOWN || mode == VXFLOAT_ROUND_TO_ZERO);
    return VxFloat80FromBits(0x3fff, significand);
}

VxFloat80 VxFloat80Exponential(VxFloatContext &context, const VxFloat80 &value) {
    const VxFloat80 exponent = VxFloat80Multiply(context, value, VxFloat80Logarithm2OfE(context));
    const VxFloat80 integral = VxFloat80RoundInteger(context, exponent);
    return VxFloat80Scale(context,
                          VxFloat80Add(context,
                                       VxFloat80Power2Minus1(context, VxFloat80Subtract(context, exponent, integral)),
                                       VxFloat80Load(context, 1.0)),
                          integral);
}
