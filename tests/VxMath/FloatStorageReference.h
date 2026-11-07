#ifndef FLOATSTORAGEREFERENCE_H
#define FLOATSTORAGEREFERENCE_H

#include <math.h>
#include <stdint.h>
#include <string.h>

// Independent binary32 storage boundaries for the original packed arithmetic.
// Volatile stores prevent the host from retaining wider intermediate results.
static float ReferenceFloatStore(double value) {
    volatile float stored = static_cast<float>(value);
    return stored;
}

static float ReferenceFloatDot(const float *first, const float *second, int count) {
    float result = ReferenceFloatStore(double(first[0]) * second[0]);
    for (int i = 1; i < count; ++i) {
        const float product = ReferenceFloatStore(double(first[i]) * second[i]);
        result = ReferenceFloatStore(double(result) + product);
    }
    return result;
}

static bool ReferenceFloatEqual(float first, float second) {
    return first == second || (isnan(first) && isnan(second));
}

static float ReferenceStoredSquare(const float *value) {
    return ReferenceFloatStore((double(value[0]) * value[0] + double(value[1]) * value[1]) + double(value[2]) * value[2]);
}

static void ReferenceStoredNormalize(const float *input, float *output) {
    const float inverse = ReferenceFloatStore(1.0 / sqrt(double(ReferenceStoredSquare(input))));
    for (int i = 0; i < 3; ++i)
        output[i] = ReferenceFloatStore(double(input[i]) * inverse);
}

static uint64_t ReferenceFloatFingerprint(const float *values, int count) {
    uint64_t hash = 14695981039346656037ULL;
    for (int i = 0; i < count; ++i) {
        uint32_t bits;
        memcpy(&bits, values + i, sizeof(bits));
        if ((bits & 0x7fffffffu) > 0x7f800000u)
            bits = 0x7fc00000u;
        for (int byte = 0; byte < 4; ++byte)
            hash = (hash ^ ((bits >> (8 * byte)) & 255u)) * 1099511628211ULL;
    }
    return hash;
}

#endif // FLOATSTORAGEREFERENCE_H
