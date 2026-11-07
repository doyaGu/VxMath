#include "VxScalar.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

// This executable deliberately links no VxMath or numerical library.
int main()
{
    VxScalarContext arithmetic;
    VxScalarInitContext(arithmetic);
    const VxScalar one = VxScalarLoad(arithmetic, 1);
    const VxScalar nine = VxScalarLoad(arithmetic, 9.0f);
    const VxScalar root = VxScalarSquareRoot(arithmetic, nine);
    unsigned failures = VxScalarStore(arithmetic, root) != 3.0f;
    failures += VxScalarStoreDouble(arithmetic, VxScalarDivide(arithmetic, nine, root)) != 3.0;
    const VxScalar precise = VxScalarLoad(arithmetic, 16777217.0);
    failures += VxScalarStoreDouble(arithmetic, precise) != 16777217.0;
    failures += VxScalarStore(arithmetic, precise) != 16777216.0f;
    failures += VxScalarCompare(arithmetic, one, root) != VXSCALAR_LESS;
    const float negative_zero = VxScalarStore(arithmetic,
        VxScalarNegate(arithmetic, VxScalarLoad(arithmetic, 0)));
    uint32_t bits;
    memcpy(&bits, &negative_zero, sizeof(bits));
    failures += bits != 0x80000000u;
    printf("native scalar isolation failures: %u\n", failures);
    return failures ? 1 : 0;
}
