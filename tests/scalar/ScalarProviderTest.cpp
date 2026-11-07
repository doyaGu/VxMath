#include "VxFPUControlWord.h"
#include "VxScalar.h"

#include <stdio.h>

// A scalar-only host supplies its environment without the full VxMath library.
XWORD VxGetFPUControlWord() { return 0x037f; }

int main() {
    VxScalarContext arithmetic;
    VxScalarInitContext(arithmetic);
    const VxScalar first = VxScalarLoad(arithmetic, 16777217);
    const VxScalar second = VxScalarLoad(arithmetic, 0.25);
    const VxScalar sum = VxScalarAdd(arithmetic, first, second);
    const unsigned failures = VxScalarStoreDouble(arithmetic, sum) != 16777217.25;
    printf("scalar provider failures=%u value_size=%u context_size=%u\n", failures,
           static_cast<unsigned>(sizeof(VxScalar)), static_cast<unsigned>(sizeof(VxScalarContext)));
    return failures ? 1 : 0;
}
