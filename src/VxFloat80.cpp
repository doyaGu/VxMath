#include "VxFloat80.h"
#include <libx87/x87.hxx>
#include <assert.h>

static X87_Float80 VxFloatInput(const VxFloat80 &value) {
    return X87_Float80::from_bits(value.SignExponent, value.Significand);
}
static VxFloat80 VxFloatOutput(const X87_Float80 &value) {
    return VxFloat80FromBits(value.get_sign_exponent(), value.get_significand());
}

// Bridge VxMath's plain context to the shared numerical implementation.
class VxFloatArithmetic : public X87_Arithmetic {
    VxFloatContext &context;
public:
    explicit VxFloatArithmetic(VxFloatContext &state)
        : X87_Arithmetic(static_cast<Precision>(state.Precision), static_cast<Rounding>(state.Rounding)),
          context(state) { raise_exception_flags(state.ExceptionFlags); }
    ~VxFloatArithmetic() { context.ExceptionFlags = get_exception_flags(); }
};

VxFloat80 VxFloat80FromBits(uint16_t signExponent, uint64_t significand) {
    const VxFloat80 result = {significand, signExponent};
    return result;
}
VxFloat80 VxFloat80FromBytes(const unsigned char *bytes) {
    return VxFloatOutput(X87_Float80::from_bytes(bytes));
}
void VxFloat80ToBytes(const VxFloat80 &value, unsigned char *bytes) {
    VxFloatInput(value).to_bytes(bytes);
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
    VxFloatArithmetic fp(context);
    return VxFloatOutput(fp.load(value));
}
VxFloat80 VxFloat80Load(VxFloatContext &context, double value) {
    VxFloatArithmetic fp(context);
    return VxFloatOutput(fp.load(value));
}
VxFloat80 VxFloat80Load(VxFloatContext &context, int32_t value) {
    VxFloatArithmetic fp(context);
    return VxFloatOutput(fp.load(value));
}
float VxFloat80StoreFloat(VxFloatContext &context, const VxFloat80 &value) {
    VxFloatArithmetic fp(context);
    return fp.store_float(VxFloatInput(value));
}
double VxFloat80StoreDouble(VxFloatContext &context, const VxFloat80 &value) {
    VxFloatArithmetic fp(context);
    return fp.store_double(VxFloatInput(value));
}
VxFloat80 VxFloat80Add(VxFloatContext &context, const VxFloat80 &first, const VxFloat80 &second) {
    VxFloatArithmetic fp(context);
    return VxFloatOutput(fp.add(VxFloatInput(first), VxFloatInput(second)));
}
VxFloat80 VxFloat80Subtract(VxFloatContext &context, const VxFloat80 &first, const VxFloat80 &second) {
    VxFloatArithmetic fp(context);
    return VxFloatOutput(fp.subtract(VxFloatInput(first), VxFloatInput(second)));
}
VxFloat80 VxFloat80Multiply(VxFloatContext &context, const VxFloat80 &first, const VxFloat80 &second) {
    VxFloatArithmetic fp(context);
    return VxFloatOutput(fp.multiply(VxFloatInput(first), VxFloatInput(second)));
}
VxFloat80 VxFloat80Divide(VxFloatContext &context, const VxFloat80 &first, const VxFloat80 &second) {
    VxFloatArithmetic fp(context);
    return VxFloatOutput(fp.divide(VxFloatInput(first), VxFloatInput(second)));
}
VxFloat80 VxFloat80Arctangent(VxFloatContext &context, const VxFloat80 &first, const VxFloat80 &second) {
    VxFloatArithmetic fp(context);
    return VxFloatOutput(fp.arctangent(VxFloatInput(first), VxFloatInput(second)));
}
VxFloat80 VxFloat80Log2Multiply(VxFloatContext &context, const VxFloat80 &first, const VxFloat80 &second) {
    VxFloatArithmetic fp(context);
    return VxFloatOutput(fp.log2_multiply(VxFloatInput(first), VxFloatInput(second)));
}
VxFloat80 VxFloat80Scale(VxFloatContext &context, const VxFloat80 &first, const VxFloat80 &second) {
    VxFloatArithmetic fp(context);
    return VxFloatOutput(fp.scale(VxFloatInput(first), VxFloatInput(second)));
}
VxFloat80 VxFloat80SquareRoot(VxFloatContext &context, const VxFloat80 &value) {
    VxFloatArithmetic fp(context);
    return VxFloatOutput(fp.square_root(VxFloatInput(value)));
}
VxFloat80 VxFloat80Absolute(VxFloatContext &context, const VxFloat80 &value) {
    VxFloatArithmetic fp(context);
    return VxFloatOutput(fp.absolute(VxFloatInput(value)));
}
VxFloat80 VxFloat80Negate(VxFloatContext &context, const VxFloat80 &value) {
    VxFloatArithmetic fp(context);
    return VxFloatOutput(fp.negate(VxFloatInput(value)));
}
VxFloat80 VxFloat80Power2Minus1(VxFloatContext &context, const VxFloat80 &value) {
    VxFloatArithmetic fp(context);
    return VxFloatOutput(fp.power2_minus1(VxFloatInput(value)));
}
VxFloat80 VxFloat80RoundInteger(VxFloatContext &context, const VxFloat80 &value) {
    VxFloatArithmetic fp(context);
    return VxFloatOutput(fp.round_integer(VxFloatInput(value)));
}
VxFloat80 VxFloat80Exponential(VxFloatContext &context, const VxFloat80 &value) {
    VxFloatArithmetic fp(context);
    return VxFloatOutput(fp.exponential(VxFloatInput(value)));
}
VxFloatRelation VxFloat80Compare(VxFloatContext &context, const VxFloat80 &first, const VxFloat80 &second, bool quiet) {
    VxFloatArithmetic fp(context);
    return static_cast<VxFloatRelation>(fp.compare(VxFloatInput(first), VxFloatInput(second), quiet));
}
VxFloat80 VxFloat80Sine(VxFloatContext &context, const VxFloat80 &value, bool *outOfRange) {
    VxFloatArithmetic fp(context);
    return VxFloatOutput(fp.sine(VxFloatInput(value), outOfRange));
}
VxFloat80 VxFloat80Cosine(VxFloatContext &context, const VxFloat80 &value, bool *outOfRange) {
    VxFloatArithmetic fp(context);
    return VxFloatOutput(fp.cosine(VxFloatInput(value), outOfRange));
}
VxFloat80 VxFloat80NaturalLogarithmOfTwo(const VxFloatContext &context) {
    X87_Arithmetic fp(static_cast<X87_Arithmetic::Precision>(context.Precision),
        static_cast<X87_Arithmetic::Rounding>(context.Rounding));
    return VxFloatOutput(fp.natural_logarithm_of_two());
}
VxFloat80 VxFloat80Logarithm2OfE(const VxFloatContext &context) {
    X87_Arithmetic fp(static_cast<X87_Arithmetic::Precision>(context.Precision),
        static_cast<X87_Arithmetic::Rounding>(context.Rounding));
    return VxFloatOutput(fp.logarithm2_of_e());
}
