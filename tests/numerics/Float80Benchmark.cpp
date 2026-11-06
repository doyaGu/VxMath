#include "VxFloat80.h"
#include "VxMatrix.h"
#include "VxFPUControlWord.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>

int main(int argc, char **argv) {
    const unsigned count = argc > 1 ? static_cast<unsigned>(strtoul(argv[1], nullptr, 10)) : 3000000;
    if (!count || count > UINT_MAX / 16)
        return 2;
    VxFloatContext context;
    VxFloatInitContext(context);
    VxFloat80 value = VxFloat80Load(context, 1);
    const VxFloat80 increment = VxFloat80Divide(context, value, VxFloat80Load(context, 1024));
    const clock_t begin = clock();
    for (unsigned i = 0; i < count; ++i) {
        value = VxFloat80Add(context, value, increment);
        value = VxFloat80Multiply(context, value, increment);
        value = VxFloat80Add(context, value, increment);
    }
    const double seconds = static_cast<double>(clock() - begin) / CLOCKS_PER_SEC;
    printf("operations=%llu seconds=%.6f mop_per_second=%.3f checksum=%04x:%016llx\n",
           static_cast<unsigned long long>(count) * 3, seconds, count * 3.0 / seconds / 1000000.0,
           static_cast<unsigned>(value.SignExponent), static_cast<unsigned long long>(value.Significand));

    VxSetFPUControlWord(0x037f);
    VxMatrix matrix = VxMatrix::Identity();
    matrix[0].Set(.9848077297f, .1736481637f, 0);
    matrix[1].Set(-.1736481637f, .9848077297f, 0);
    matrix[3].Set(17, -9, 4);
    VxVector input(.5f, .75f, 1), output;
    unsigned state = 0x12345678;
    volatile float checksum[3] = {};
    const unsigned pointCount = count * 16;
    const clock_t matrixBegin = clock();
    for (unsigned i = 0; i < pointCount; ++i) {
        state = state * 1664525 + 1013904223;
        input.x = static_cast<float>(state & 1023) * (1.0f / 1024);
        Vx3DMultiplyMatrixVector(&output, matrix, &input);
        checksum[0] = output.x;
        checksum[1] = output.y;
        checksum[2] = output.z;
    }
    const double matrixSeconds = static_cast<double>(clock() - matrixBegin) / CLOCKS_PER_SEC;
    printf("point_transforms=%u seconds=%.6f mop_per_second=%.3f checksum=%.9g,%.9g,%.9g\n", pointCount, matrixSeconds,
           pointCount / matrixSeconds / 1000000.0, static_cast<double>(checksum[0]), static_cast<double>(checksum[1]),
           static_cast<double>(checksum[2]));
    return 0;
}
