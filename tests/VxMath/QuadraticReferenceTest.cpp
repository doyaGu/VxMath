#include <cmath>
#include <cfloat>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#ifndef VX_QUADRATIC_NATIVE_PROBE
#ifndef VX_LIB
#define VX_LIB 1
#endif
#ifndef VX_QUADRATIC_IMPLEMENTATION
#define VX_QUADRATIC_IMPLEMENTATION "../../src/VxIntersect_Sphere.cpp"
#endif
#include VX_QUADRATIC_IMPLEMENTATION
#include <gtest/gtest.h>
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

typedef int (*QFunction)(float, float, float, float *, float *);
struct QResults { int samples = 0, errors = 0; unsigned digest = 2166136261u; };
static void QObserve(QResults &r, int result, float first, float second, bool good) {
    unsigned words[2];
    std::memcpy(words, &first, sizeof(first));
    std::memcpy(words + 1, &second, sizeof(second));
    r.digest = (r.digest ^ unsigned(result)) * 16777619u;
    for (unsigned word : words) r.digest = (r.digest ^ word) * 16777619u;
    ++r.samples;
    if (!good) ++r.errors;
}
static bool QNear(float actual, double expected) {
    // A relative bound is essential: an absolute epsilon would accept a lost
    // 2^-120 root as zero. All generated nonzero roots are normal float values.
    return std::isfinite(actual) && (expected == 0 ? actual == 0 :
        std::abs(double(actual) - expected) <= std::abs(expected) * 4e-6);
}
static bool QResidual(float a, float b, float c, float root) {
    const double x = root;
    const double aa = double(a) * x * x, bb = double(b) * x;
    const double scale = std::abs(aa) + std::abs(bb) + std::abs(double(c));
    return std::isfinite(root) && std::abs(aa + bb + c) <= scale * 4e-6;
}
static bool QRoots(float a, float b, float c, float first, float second, double expected1, double expected2) {
    return QNear(first, expected1) && QNear(second, expected2) &&
        QResidual(a, b, c, first) && QResidual(a, b, c, second) &&
        std::abs(double(first) + second + double(b) / a) <=
            (std::abs(double(first)) + std::abs(double(second))) * 8e-6 &&
        std::abs(double(first) * second - double(c) / a) <=
            (std::abs(double(c) / a) + DBL_MIN) * 8e-6;
}
static QResults QFactored(QFunction fn) {
    QResults r;
    // Construct the polynomial from known integer roots rather than copying
    // the production quadratic formula into the oracle.
    for (int a : {-3, -1, 1, 3}) for (int low = -8; low <= 8; ++low)
        for (int high = low; high <= 8; ++high) {
            const float b = float(-a * (low + high)), c = float(a * low * high);
            float first = 123, second = -456;
            const int result = fn(float(a), b, c, &first, &second);
            QObserve(r, result, first, second, result == 1 && QRoots(float(a), b, c, first, second,
                a > 0 ? high : low, a > 0 ? low : high));
        }
    return r;
}
static QResults QFailureAndLinear(QFunction fn) {
    QResults r;
    for (float a : {-4.0f, -1.0f, 1.0f, 4.0f}) {
        float first = 123, second = -456;
        const int result = fn(a, 0, a, &first, &second);
        QObserve(r, result, first, second, result == 0 && first == 123 && second == -456);
    }
    for (float a : {0.0f, -0.0f}) for (float b : {-4.0f, -1.0f, 0.0f, 1.0f, 4.0f})
        for (float c : {-8.0f, 0.0f, 8.0f}) {
            float first = 123, second = -456;
            const int result = fn(a, b, c, &first, &second);
            const double root = b == 0 ? 0 : -double(c) / b;
            QObserve(r, result, first, second, result == int(b != 0) && QNear(first, root) && QNear(second, root));
        }
    return r;
}
static QResults QScales(QFunction fn) {
    QResults r;
    for (int exponent : {-120, -100, -40, 0, 40, 100, 120}) for (int sign : {-1, 1}) {
        const float a = std::ldexp(float(sign), exponent), b = -3 * a, c = 2 * a;
        float first = 123, second = -456;
        const int result = fn(a, b, c, &first, &second);
        QObserve(r, result, first, second, result == 1 && QRoots(a, b, c, first, second,
            sign > 0 ? 2 : 1, sign > 0 ? 1 : 2));
    }
    return r;
}
static QResults QCancellation(QFunction fn) {
    QResults r;
    for (int exponent : {20, 40, 60, 100, 120}) for (int signA : {-1, 1}) for (int signB : {-1, 1}) {
        const float a = float(signA), b = std::ldexp(float(signB), exponent), c = a;
        // With |b| >= 2^20 the real roots round to these powers of two in
        // float. Their sum/product and polynomial residual provide separate
        // checks, including the small root that the naive formula cancels.
        const double largeRoot = -double(b) / a, smallRoot = 1.0 / largeRoot;
        const double firstExpected = signB > 0 ? smallRoot : largeRoot;
        const double secondExpected = signB > 0 ? largeRoot : smallRoot;
        float first = 123, second = -456;
        const int result = fn(a, b, c, &first, &second);
        QObserve(r, result, first, second, result == 1 && QRoots(a, b, c, first, second, firstExpected, secondExpected));
    }
    return r;
}
static QResults QOutputOrder(QFunction fn) {
    QResults r;
    for (float a : {-1.0f, 1.0f}) for (float b : {-0.0f, 0.0f}) {
        float first = 123, second = -456;
        const int result = fn(a, b, -4 * a, &first, &second);
        QObserve(r, result, first, second, result == 1 && QRoots(a, b, -4 * a, first, second,
            a > 0 ? 2 : -2, a > 0 ? -2 : 2));
    }
    return r;
}
#if defined(_WIN32)
static QResults QFootprint(QFunction fn) {
    QResults r;
    SYSTEM_INFO system; GetSystemInfo(&system);
    unsigned char *pages = static_cast<unsigned char *>(VirtualAlloc(nullptr, system.dwPageSize * 4,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!pages) { QObserve(r, 0, 0, 0, false); return r; }
    DWORD old;
    const bool protectedPages = VirtualProtect(pages + system.dwPageSize, system.dwPageSize, PAGE_NOACCESS, &old) &&
        VirtualProtect(pages + system.dwPageSize * 3, system.dwPageSize, PAGE_NOACCESS, &old);
    if (protectedPages) {
        float *first = reinterpret_cast<float *>(pages + system.dwPageSize - sizeof(float));
        float *second = reinterpret_cast<float *>(pages + system.dwPageSize * 3 - sizeof(float));
        *first = 123; *second = -456;
        int result = fn(1, -3, 2, first, second);
        QObserve(r, result, *first, *second, result == 1 && *first == 2 && *second == 1);
        *first = 123; *second = -456;
        result = fn(1, 0, 1, first, second);
        QObserve(r, result, *first, *second, result == 0 && *first == 123 && *second == -456);
    } else QObserve(r, 0, 0, 0, false);
    VirtualFree(pages, 0, MEM_RELEASE);
    return r;
}
#endif
#ifndef VX_QUADRATIC_NATIVE_PROBE
static void QCheck(QResults result) {
    EXPECT_EQ(result.errors, 0);
    ::testing::Test::RecordProperty("samples", result.samples);
    ::testing::Test::RecordProperty("digest", std::to_string(result.digest));
}
TEST(QuadraticReferenceTest, FactoredRootsAndOriginalSlotOrder) { QCheck(QFactored(QuadraticFormula)); }
TEST(QuadraticReferenceTest, FailurePreservesOutputsAndLinearDegeneracy) { QCheck(QFailureAndLinear(QuadraticFormula)); }
TEST(QuadraticReferenceTest, CoefficientScaleEquivalence) { QCheck(QScales(QuadraticFormula)); }
TEST(QuadraticReferenceTest, SmallRootSurvivesCancellation) { QCheck(QCancellation(QuadraticFormula)); }
TEST(QuadraticReferenceTest, NegativeLeadingCoefficientAndSignedZeroOrder) { QCheck(QOutputOrder(QuadraticFormula)); }
#if defined(_WIN32)
TEST(QuadraticReferenceTest, ExactFourByteOutputFootprints) { QCheck(QFootprint(QuadraticFormula)); }
#endif
#endif
