#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "VxBlitEngine.h"
#include "VxMath.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <time.h>
#endif

template <typename T>
class PerfBuffer : public XArray<T> {
public:
    explicit PerfBuffer(size_t count = 0)
        : XArray<T>(static_cast<int>(count)) {
        if (count > 0) {
            this->Resize(static_cast<int>(count));
        }
    }

    PerfBuffer(size_t count, const T &value)
        : XArray<T>(static_cast<int>(count)) {
        if (count > 0) {
            this->Resize(static_cast<int>(count));
            for (int i = 0; i < this->Size(); ++i) {
                (*this)[i] = value;
            }
        }
    }

    T *data() { return this->Begin(); }
    const T *data() const { return this->Begin(); }
    size_t size() const { return static_cast<size_t>(this->Size()); }
};

struct FastRng {
    explicit FastRng(XDWORD seed) : m_State(seed) {}

    XDWORD NextU32() {
        m_State = m_State * 1664525u + 1013904223u;
        return m_State;
    }

    float NextFloat(float minValue, float maxValue) {
        const float unit = static_cast<float>((NextU32() >> 8) * (1.0 / 16777216.0));
        return minValue + (maxValue - minValue) * unit;
    }

private:
    XDWORD m_State;
};

class PerfTimer {
public:
    PerfTimer() { Reset(); }

    void Reset() {
#if defined(_WIN32)
        QueryPerformanceCounter(&m_Start);
#else
        clock_gettime(CLOCK_MONOTONIC, &m_Start);
#endif
    }

    double ElapsedSeconds() const {
#if defined(_WIN32)
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        return static_cast<double>(now.QuadPart - m_Start.QuadPart) /
               static_cast<double>(freq.QuadPart);
#else
        timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        const time_t secDiff = now.tv_sec - m_Start.tv_sec;
        const long nsecDiff = now.tv_nsec - m_Start.tv_nsec;
        return static_cast<double>(secDiff) + static_cast<double>(nsecDiff) * 1.0e-9;
#endif
    }

private:
#if defined(_WIN32)
    LARGE_INTEGER m_Start;
#else
    timespec m_Start;
#endif
};

int ReadPositiveEnv(const char *name, int fallback) {
    const char *value = getenv(name);
    if (!value || !*value) {
        return fallback;
    }
    char *end = nullptr;
    const long parsed = strtol(value, &end, 10);
    if (end == value || parsed <= 0) {
        return fallback;
    }
    return static_cast<int>(parsed);
}

bool EqualsIgnoreCase(const char *a, const char *b) {
    if (a == nullptr || b == nullptr) {
        return false;
    }
    while (*a != '\0' && *b != '\0') {
        char ca = *a;
        char cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
        if (ca != cb) return false;
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

bool ReadBoolEnv(const char *name, bool fallback) {
    const char *value = getenv(name);
    if (!value || !*value) {
        return fallback;
    }
    if (EqualsIgnoreCase(value, "0") ||
        EqualsIgnoreCase(value, "false") ||
        EqualsIgnoreCase(value, "off") ||
        EqualsIgnoreCase(value, "no")) {
        return false;
    }
    if (EqualsIgnoreCase(value, "1") ||
        EqualsIgnoreCase(value, "true") ||
        EqualsIgnoreCase(value, "on") ||
        EqualsIgnoreCase(value, "yes")) {
        return true;
    }
    return fallback;
}

struct ProgramOptions {
    bool singleBackend = false;
    bool sweepBackends = false;
    bool gateImageAuto = false;
    bool imageGateCasesOnly = false;
    bool printHeader = true;
    bool hasRequestedBackend = false;
    int gateRuns = 1;
    int requestedBackend = VX_SIMD_MODE_AUTO;
};

bool ParsePositiveInt(const char *value, int &out) {
    if (value == nullptr || *value == '\0') {
        return false;
    }

    char *end = nullptr;
    const long parsed = strtol(value, &end, 10);
    if (end == value || *end != '\0' || parsed <= 0 || parsed > INT_MAX) {
        return false;
    }

    out = static_cast<int>(parsed);
    return true;
}

bool ParseSIMDBackendMode(const char *value, int &mode) {
    if (value == nullptr || *value == '\0') {
        return false;
    }
    if (EqualsIgnoreCase(value, "auto")) {
        mode = VX_SIMD_MODE_AUTO;
        return true;
    }
    if (EqualsIgnoreCase(value, "none") || EqualsIgnoreCase(value, "scalar")) {
        mode = VX_SIMD_MODE_NONE;
        return true;
    }
    if (EqualsIgnoreCase(value, "sse2")) {
        mode = VX_SIMD_MODE_SSE2;
        return true;
    }
    if (EqualsIgnoreCase(value, "ssse3")) {
        mode = VX_SIMD_MODE_SSSE3;
        return true;
    }
    if (EqualsIgnoreCase(value, "sse4_1") || EqualsIgnoreCase(value, "sse4.1") || EqualsIgnoreCase(value, "sse41")) {
        mode = VX_SIMD_MODE_SSE4_1;
        return true;
    }
    if (EqualsIgnoreCase(value, "avx")) {
        mode = VX_SIMD_MODE_AVX;
        return true;
    }
    if (EqualsIgnoreCase(value, "avx2")) {
        mode = VX_SIMD_MODE_AVX2;
        return true;
    }
    return false;
}

bool StartsWith(const char *str, const char *prefix) {
    if (str == nullptr || prefix == nullptr) {
        return false;
    }
    while (*prefix != '\0') {
        if (*str != *prefix) {
            return false;
        }
        ++str;
        ++prefix;
    }
    return true;
}

bool ParseProgramOptions(int argc, char **argv, ProgramOptions &options) {
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];

        if (strcmp(arg, "--single-backend") == 0) {
            options.singleBackend = true;
            continue;
        }

        if (strcmp(arg, "--sweep-backends") == 0) {
            options.sweepBackends = true;
            continue;
        }

        if (strcmp(arg, "--gate-image-auto") == 0) {
            options.gateImageAuto = true;
            continue;
        }

        if (strcmp(arg, "--image-gate-cases-only") == 0) {
            options.imageGateCasesOnly = true;
            continue;
        }

        if (strcmp(arg, "--gate-runs") == 0) {
            if (i + 1 >= argc || !ParsePositiveInt(argv[++i], options.gateRuns)) {
                return false;
            }
            continue;
        }

        if (StartsWith(arg, "--gate-runs=")) {
            const char *value = arg + sizeof("--gate-runs=") - 1;
            if (!ParsePositiveInt(value, options.gateRuns)) {
                return false;
            }
            continue;
        }

        if (strcmp(arg, "--no-header") == 0) {
            options.printHeader = false;
            continue;
        }

        if (strcmp(arg, "--simd-backend") == 0) {
            if (i + 1 >= argc) {
                return false;
            }
            int mode = VX_SIMD_MODE_AUTO;
            if (!ParseSIMDBackendMode(argv[++i], mode)) {
                return false;
            }
            options.hasRequestedBackend = true;
            options.requestedBackend = mode;
            continue;
        }

        if (StartsWith(arg, "--simd-backend=")) {
            const char *value = arg + sizeof("--simd-backend=") - 1;
            int mode = VX_SIMD_MODE_AUTO;
            if (!ParseSIMDBackendMode(value, mode)) {
                return false;
            }
            options.hasRequestedBackend = true;
            options.requestedBackend = mode;
            continue;
        }
    }
    return true;
}

struct ImagePerfGateCase {
    const char *Name;
    double NoneNs;
    double AutoNs;
};

static const double kImagePerfGateTolerance = 0.03;

static ImagePerfGateCase g_ImagePerfGateCases[] = {
    {"mipmap_argb32_1024", 0.0, 0.0},
    {"mipmap_rgb24_1024", 0.0, 0.0},
    {"mipmap_rgb565_1024", 0.0, 0.0},
    {"bump_rgb24_1024", 0.0, 0.0},
    {"normal_argb32_1024", 0.0, 0.0},
    {"normal_rgb24_1024", 0.0, 0.0}
};

static bool g_ImageGateCasesOnly = false;

void ResetImagePerfGateCases() {
    for (int i = 0; i < (int) (sizeof(g_ImagePerfGateCases) / sizeof(g_ImagePerfGateCases[0])); ++i) {
        g_ImagePerfGateCases[i].NoneNs = 0.0;
        g_ImagePerfGateCases[i].AutoNs = 0.0;
    }
}

int FindImagePerfGateCase(const char *name) {
    for (int i = 0; i < (int) (sizeof(g_ImagePerfGateCases) / sizeof(g_ImagePerfGateCases[0])); ++i) {
        if (strcmp(name, g_ImagePerfGateCases[i].Name) == 0) return i;
    }
    return -1;
}

bool ShouldRunPerfCase(const char *name) {
    return !g_ImageGateCasesOnly || FindImagePerfGateCase(name) >= 0;
}

FILE *OpenPerfPipe(const char *command) {
#if defined(_WIN32)
    return _popen(command, "r");
#else
    return popen(command, "r");
#endif
}

int ClosePerfPipe(FILE *pipe) {
#if defined(_WIN32)
    return _pclose(pipe);
#else
    return pclose(pipe);
#endif
}

bool ReadImagePerfGateRun(const char *selfPath, const char *backend, bool isAuto) {
    char command[1024];
#if defined(_MSC_VER)
    _snprintf_s(
        command,
        sizeof(command),
        _TRUNCATE,
        "\"%s\" --single-backend --no-header --image-gate-cases-only --simd-backend=%s",
        (selfPath && *selfPath) ? selfPath : "VxMathPerf",
        backend
    );
#else
    snprintf(
        command,
        sizeof(command),
        "\"%s\" --single-backend --no-header --image-gate-cases-only --simd-backend=%s",
        (selfPath && *selfPath) ? selfPath : "VxMathPerf",
        backend
    );
#endif

    FILE *pipe = OpenPerfPipe(command);
    if (!pipe) return false;

    char line[512];
    while (fgets(line, sizeof(line), pipe)) {
        char caseName[128];
        char backendName[64];
        double ns = 0.0;
        double mb = 0.0;
        if (sscanf(line, "%127[^,],%63[^,],%lf,%lf", caseName, backendName, &ns, &mb) != 4) {
            continue;
        }

        const int index = FindImagePerfGateCase(caseName);
        if (index < 0) continue;
        if (isAuto) {
            if (g_ImagePerfGateCases[index].AutoNs == 0.0 || ns < g_ImagePerfGateCases[index].AutoNs) {
                g_ImagePerfGateCases[index].AutoNs = ns;
            }
        } else {
            if (g_ImagePerfGateCases[index].NoneNs == 0.0 || ns < g_ImagePerfGateCases[index].NoneNs) {
                g_ImagePerfGateCases[index].NoneNs = ns;
            }
        }
    }

    return ClosePerfPipe(pipe) == 0;
}

int RunImagePerfGate(const char *selfPath, int runs) {
    ResetImagePerfGateCases();
    for (int run = 0; run < runs; ++run) {
        if (!ReadImagePerfGateRun(selfPath, "none", false)) {
            fprintf(stderr, "Failed to run image perf gate NONE backend\n");
            return 1;
        }
        if (!ReadImagePerfGateRun(selfPath, "auto", true)) {
            fprintf(stderr, "Failed to run image perf gate AUTO backend\n");
            return 1;
        }
    }

    int failures = 0;
    printf("case,best_none_ns,best_auto_ns,auto_ratio,runs,status\n");
    for (int i = 0; i < (int) (sizeof(g_ImagePerfGateCases) / sizeof(g_ImagePerfGateCases[0])); ++i) {
        const ImagePerfGateCase &gateCase = g_ImagePerfGateCases[i];
        const double allowedAutoNs = gateCase.NoneNs * (1.0 + kImagePerfGateTolerance);
        const double ratio = (gateCase.NoneNs > 0.0) ? (gateCase.AutoNs / gateCase.NoneNs) : 0.0;
        const bool passed = gateCase.NoneNs > 0.0 && gateCase.AutoNs > 0.0 && gateCase.AutoNs <= allowedAutoNs;
        if (!passed) ++failures;
        printf(
            "%s,%.3f,%.3f,%.5f,%d,%s\n",
            gateCase.Name,
            gateCase.NoneNs,
            gateCase.AutoNs,
            ratio,
            runs,
            passed ? "PASS" : "FAIL"
        );
    }

    return failures == 0 ? 0 : 1;
}

int RunBackendSweep(const char *selfPath) {
    const int requestedModes[] = {
        VX_SIMD_MODE_NONE,
        VX_SIMD_MODE_SSE2,
        VX_SIMD_MODE_SSSE3,
        VX_SIMD_MODE_SSE4_1,
        VX_SIMD_MODE_AVX,
        VX_SIMD_MODE_AVX2,
        VX_SIMD_MODE_AUTO
    };
    const char *path = (selfPath && *selfPath) ? selfPath : "VxMathPerf";

    for (int requestedMode : requestedModes) {
        const char *requestedName = VxGetSIMDBackendName(requestedMode);
        char cmd[2048];
        const int cmdLen = snprintf(
            cmd,
            sizeof(cmd),
            "\"%s\" --single-backend --no-header --simd-backend=%s",
            path,
            requestedName
        );
        if (cmdLen <= 0 || cmdLen >= static_cast<int>(sizeof(cmd))) {
            return -1;
        }
        const int rc = system(cmd);
        if (rc != 0) {
            return rc;
        }
    }
    return 0;
}

#define RUN_CASE(NAME, BACKEND, BYTES_PER_ITER, WARMUP_ITERS, MEASURE_ITERS, ...) \
    do { \
        if (ShouldRunPerfCase((NAME))) { \
            for (int runCaseWarmup = 0; runCaseWarmup < (WARMUP_ITERS); ++runCaseWarmup) { \
                __VA_ARGS__ \
            } \
            PerfTimer runCaseTimer; \
            runCaseTimer.Reset(); \
            for (int runCaseMeasure = 0; runCaseMeasure < (MEASURE_ITERS); ++runCaseMeasure) { \
                __VA_ARGS__ \
            } \
            const double runCaseTotalSeconds = runCaseTimer.ElapsedSeconds(); \
            const double runCaseTotalNs = runCaseTotalSeconds * 1.0e9; \
            const double runCaseNsPerOp = runCaseTotalNs / static_cast<double>(MEASURE_ITERS); \
            const double runCaseTotalMB = static_cast<double>(BYTES_PER_ITER) * static_cast<double>(MEASURE_ITERS) / (1024.0 * 1024.0); \
            const double runCaseMBPerSec = (runCaseTotalSeconds > 0.0) ? (runCaseTotalMB / runCaseTotalSeconds) : 0.0; \
            printf("%s,%s,%.3f,%.3f\n", (NAME), (BACKEND), runCaseNsPerOp, runCaseMBPerSec); \
        } \
    } while (0)

#define RUN_BLIT_CASE(NAME, SRC, DST) \
    RUN_CASE((NAME), backend, ImageBytes(SRC) + ImageBytes(DST), kWarmup, kMeasure, { \
        VxDoBlit((SRC), (DST)); \
    })

#define RUN_UPSIDE_DOWN_CASE(NAME, SRC, DST) \
    RUN_CASE((NAME), backend, ImageBytes(SRC) + ImageBytes(DST), kWarmup, kMeasure, { \
        VxDoBlitUpsideDown((SRC), (DST)); \
    })

VxImageDescEx MakeImageDesc(VX_PIXELFORMAT pf, int width, int height, XBYTE *image) {
    VxImageDescEx desc;
    VxPixelFormat2ImageDesc(pf, desc);
    desc.Width = width;
    desc.Height = height;
    desc.BytesPerLine = width * (desc.BitsPerPixel / 8);
    desc.Image = image;
    return desc;
}

size_t ImageBytes(const VxImageDescEx &desc) {
    return static_cast<size_t>(desc.Width) * static_cast<size_t>(desc.Height) * static_cast<size_t>(desc.BitsPerPixel / 8);
}

void FillRandomBytes(PerfBuffer<XBYTE> &buffer, FastRng &rng) {
    for (size_t i = 0; i < buffer.size(); ++i) {
        buffer[static_cast<int>(i)] = static_cast<XBYTE>(rng.NextU32() & 0xFFu);
    }
}

struct PerfVertex {
    float x;
    float y;
    float z;
    float w;
};

int main(int argc, char **argv) {
    ProgramOptions options;
    if (!ParseProgramOptions(argc, argv, options)) {
        fprintf(stderr, "Usage: %s [--single-backend] [--sweep-backends] [--no-header] "
                        "[--gate-image-auto] [--gate-runs N] "
                        "[--simd-backend <auto|none|sse2|ssse3|sse4_1|avx|avx2>]\n",
                (argc > 0 && argv[0]) ? argv[0] : "VxMathPerf");
        return 2;
    }

    g_ImageGateCasesOnly = options.imageGateCasesOnly;

    if (options.gateImageAuto) {
        return RunImagePerfGate((argc > 0 && argv[0]) ? argv[0] : "VxMathPerf", options.gateRuns);
    }

    const bool shouldSweep = options.sweepBackends ||
                             (!options.singleBackend && ReadBoolEnv("VXMATH_PERF_SWEEP_BACKENDS", false));
    if (shouldSweep) {
        printf("case,backend,ns/op,MB/s\n");
        fflush(stdout);
        return RunBackendSweep(argv[0]);
    }

    const int requestedMode = options.hasRequestedBackend ? options.requestedBackend : VX_SIMD_MODE_AUTO;
    if (!VxSetSIMDOverride(requestedMode)) {
        fprintf(stderr, "Failed to apply --simd-backend=%s\n", VxGetSIMDBackendName(requestedMode));
        return 2;
    }

    const int effectiveMode = VxGetSIMDEffectiveBackend();
    char backendLabel[96];
    if (requestedMode == VX_SIMD_MODE_AUTO) {
        snprintf(backendLabel, sizeof(backendLabel), "%s", VxGetSIMDBackendName(effectiveMode));
    } else {
        snprintf(backendLabel,
                 sizeof(backendLabel),
                 "%s(req=%s)",
                 VxGetSIMDBackendName(effectiveMode),
                 VxGetSIMDBackendName(requestedMode));
    }
    const char *backend = backendLabel;

    const int kWarmup = ReadPositiveEnv("VXMATH_PERF_WARMUP", 6);
    const int kMeasure = ReadPositiveEnv("VXMATH_PERF_MEASURE", 24);

    FastRng rng(1337u);

    if (options.printHeader && ReadBoolEnv("VXMATH_PERF_PRINT_HEADER", true)) {
        printf("case,backend,ns/op,MB/s\n");
        fflush(stdout);
    }

    //==========================================================================
    // Core vector/matrix/quaternion kernels
    //==========================================================================
    {
        constexpr int kCount = 1 << 20;
        PerfBuffer<float> a(kCount), b(kCount), out(kCount);
        for (int i = 0; i < kCount; ++i) {
            a[i] = rng.NextFloat(-100.0f, 100.0f);
            b[i] = rng.NextFloat(-100.0f, 100.0f);
        }
        RUN_CASE("interpolate_float_array", backend, sizeof(float) * static_cast<size_t>(kCount) * 3, kWarmup, kMeasure, {
            InterpolateFloatArray(out.data(), a.data(), b.data(), 0.375f, kCount);
        });
    }

    {
        constexpr int kCount = 1 << 18;
        PerfBuffer<VxVector> a(kCount), b(kCount), out(kCount);
        for (int i = 0; i < kCount; ++i) {
            a[i] = VxVector(rng.NextFloat(-100.0f, 100.0f), rng.NextFloat(-100.0f, 100.0f), rng.NextFloat(-100.0f, 100.0f));
            b[i] = VxVector(rng.NextFloat(-100.0f, 100.0f), rng.NextFloat(-100.0f, 100.0f), rng.NextFloat(-100.0f, 100.0f));
        }
        RUN_CASE("interpolate_vector_array", backend, sizeof(VxVector) * static_cast<size_t>(kCount) * 3, kWarmup, kMeasure, {
            InterpolateVectorArray(out.data(), a.data(), b.data(), 0.625f, kCount, sizeof(VxVector), sizeof(VxVector));
        });
    }

    {
        constexpr int kCount = 1 << 18;
        PerfBuffer<VxVector> in(kCount), out(kCount);
        for (int i = 0; i < kCount; ++i) {
            in[i] = VxVector(rng.NextFloat(-100.0f, 100.0f), rng.NextFloat(-100.0f, 100.0f), rng.NextFloat(-100.0f, 100.0f));
        }
        VxMatrix m;
        Vx3DMatrixFromRotation(m, VxVector(0.3f, 1.0f, 0.2f), 0.8f);
        m[3][0] = 3.0f;
        m[3][1] = -2.0f;
        m[3][2] = 1.0f;
        RUN_CASE("matrix_vector_many", backend, sizeof(VxVector) * static_cast<size_t>(kCount) * 2, kWarmup, kMeasure, {
            Vx3DMultiplyMatrixVectorMany(out.data(), m, in.data(), kCount, sizeof(VxVector));
        });
        RUN_CASE("rotate_vector_many", backend, sizeof(VxVector) * static_cast<size_t>(kCount) * 2, kWarmup, kMeasure, {
            Vx3DRotateVectorMany(out.data(), m, in.data(), kCount, sizeof(VxVector));
        });
    }

    {
        constexpr int kCount = 1 << 18;
        PerfBuffer<VxQuaternion> q1(kCount), q2(kCount), out(kCount);
        for (int i = 0; i < kCount; ++i) {
            q1[i] = VxQuaternion(rng.NextFloat(-100.0f, 100.0f), rng.NextFloat(-100.0f, 100.0f), rng.NextFloat(-100.0f, 100.0f), rng.NextFloat(-100.0f, 100.0f));
            q2[i] = VxQuaternion(rng.NextFloat(-100.0f, 100.0f), rng.NextFloat(-100.0f, 100.0f), rng.NextFloat(-100.0f, 100.0f), rng.NextFloat(-100.0f, 100.0f));
            q1[i].Normalize();
            q2[i].Normalize();
        }

        RUN_CASE("quaternion_multiply", backend, sizeof(VxQuaternion) * static_cast<size_t>(kCount) * 3, kWarmup, kMeasure, {
            for (int i = 0; i < kCount; ++i) {
                out[i] = Vx3DQuaternionMultiply(q1[i], q2[i]);
            }
        });

        RUN_CASE("quaternion_normalize", backend, sizeof(VxQuaternion) * static_cast<size_t>(kCount) * 2, kWarmup, kMeasure, {
            for (int i = 0; i < kCount; ++i) {
                out[i] = q1[i];
                out[i].Normalize();
            }
        });

        RUN_CASE("quaternion_slerp", backend, sizeof(VxQuaternion) * static_cast<size_t>(kCount) * 3, kWarmup, kMeasure, {
            for (int i = 0; i < kCount; ++i) {
                out[i] = Slerp(0.35f, q1[i], q2[i]);
            }
        });
    }

    //==========================================================================
    // Memory/structure helpers
    //==========================================================================
    {
        constexpr int kCount = 1 << 18;
        PerfBuffer<PerfVertex> src(kCount), dst(kCount);
        PerfBuffer<int> indices(kCount);
        for (int i = 0; i < kCount; ++i) {
            src[i].x = rng.NextFloat(-100.0f, 100.0f);
            src[i].y = rng.NextFloat(-100.0f, 100.0f);
            src[i].z = rng.NextFloat(-100.0f, 100.0f);
            src[i].w = rng.NextFloat(-100.0f, 100.0f);
            indices[i] = static_cast<int>(rng.NextU32() % static_cast<uint32_t>(kCount));
        }

        RUN_CASE("fill_structure_vertex", backend, sizeof(PerfVertex) * static_cast<size_t>(kCount), kWarmup, kMeasure, {
            VxFillStructure(kCount, dst.data(), sizeof(PerfVertex), sizeof(PerfVertex), src.data());
        });

        RUN_CASE("copy_structure_vertex", backend, sizeof(PerfVertex) * static_cast<size_t>(kCount) * 2, kWarmup, kMeasure, {
            VxCopyStructure(kCount, dst.data(), sizeof(PerfVertex), sizeof(PerfVertex), src.data(), sizeof(PerfVertex));
        });

        VxStridedData dstStrided(dst.data(), sizeof(PerfVertex));
        VxStridedData srcStrided(src.data(), sizeof(PerfVertex));
        RUN_CASE("indexed_copy_vertex", backend, sizeof(PerfVertex) * static_cast<size_t>(kCount) * 2, kWarmup, kMeasure, {
            VxIndexedCopy(dstStrided, srcStrided, sizeof(PerfVertex), indices.data(), kCount);
        });
    }

    //==========================================================================
    // Geometry primitive hot paths
    //==========================================================================
    {
        constexpr int kCount = 1 << 18;
        PerfBuffer<VxRay> rays(kCount), outRays(kCount);
        PerfBuffer<VxVector> points(kCount), outPoints(kCount);
        PerfBuffer<float> outDistance(kCount);

        for (int i = 0; i < kCount; ++i) {
            const VxVector origin(rng.NextFloat(-100.0f, 100.0f), rng.NextFloat(-100.0f, 100.0f), rng.NextFloat(-100.0f, 100.0f));
            const VxVector dir(rng.NextFloat(-100.0f, 100.0f), rng.NextFloat(-100.0f, 100.0f), rng.NextFloat(-100.0f, 100.0f));
            int dummy = 0;
            rays[i] = VxRay(origin, dir, &dummy);
            points[i] = VxVector(rng.NextFloat(-100.0f, 100.0f), rng.NextFloat(-100.0f, 100.0f), rng.NextFloat(-100.0f, 100.0f));
        }

        VxMatrix transform;
        Vx3DMatrixFromEulerAngles(transform, 0.45f, -0.31f, 0.77f);
        transform[3][0] = 2.0f;
        transform[3][1] = -5.0f;
        transform[3][2] = 1.5f;

        RUN_CASE("ray_square_distance_many", backend, sizeof(VxRay) * static_cast<size_t>(kCount) + sizeof(VxVector) * static_cast<size_t>(kCount),
                kWarmup, kMeasure, {
            for (int i = 0; i < kCount; ++i) {
                outDistance[i] = rays[i].SquareDistance(points[i]);
            }
        });

        RUN_CASE("ray_distance_many", backend, sizeof(VxRay) * static_cast<size_t>(kCount) + sizeof(VxVector) * static_cast<size_t>(kCount),
                kWarmup, kMeasure, {
            for (int i = 0; i < kCount; ++i) {
                outDistance[i] = rays[i].Distance(points[i]);
            }
        });

        RUN_CASE("ray_interpolate_many", backend, sizeof(VxRay) * static_cast<size_t>(kCount) + sizeof(VxVector) * static_cast<size_t>(kCount),
                kWarmup, kMeasure, {
            for (int i = 0; i < kCount; ++i) {
                rays[i].Interpolate(outPoints[i], 0.375f);
            }
        });

        RUN_CASE("ray_transform_many", backend, sizeof(VxRay) * static_cast<size_t>(kCount) * 2, kWarmup, kMeasure, {
            for (int i = 0; i < kCount; ++i) {
                rays[i].Transform(outRays[i], transform);
            }
        });
    }

    {
        constexpr int kCount = 1 << 18;
        PerfBuffer<VxPlane> planes(kCount);
        PerfBuffer<VxVector> points(kCount), nearest(kCount);
        PerfBuffer<float> outClassify(kCount), outDistance(kCount);
        PerfBuffer<VxBbox> boxes(kCount);
        PerfBuffer<VxMatrix> mats(kCount);

        for (int i = 0; i < kCount; ++i) {
            VxVector n(rng.NextFloat(-100.0f, 100.0f), rng.NextFloat(-100.0f, 100.0f), rng.NextFloat(-100.0f, 100.0f));
            VxVector p(rng.NextFloat(-100.0f, 100.0f), rng.NextFloat(-100.0f, 100.0f), rng.NextFloat(-100.0f, 100.0f));
            planes[i].Create(n, p);
            points[i] = VxVector(rng.NextFloat(-100.0f, 100.0f), rng.NextFloat(-100.0f, 100.0f), rng.NextFloat(-100.0f, 100.0f));
            boxes[i] = VxBbox(VxVector(-1.0f, -1.0f, -1.0f), VxVector(1.0f, 1.0f, 1.0f));
            Vx3DMatrixFromEulerAngles(mats[i], 0.01f * (i % 17), 0.02f * (i % 11), 0.03f * (i % 7));
            mats[i][3][0] = rng.NextFloat(-100.0f, 100.0f) * 0.1f;
            mats[i][3][1] = rng.NextFloat(-100.0f, 100.0f) * 0.1f;
            mats[i][3][2] = rng.NextFloat(-100.0f, 100.0f) * 0.1f;
        }

        RUN_CASE("plane_classify_point_many", backend, sizeof(VxPlane) * static_cast<size_t>(kCount) + sizeof(VxVector) * static_cast<size_t>(kCount),
                kWarmup, kMeasure, {
            for (int i = 0; i < kCount; ++i) {
                outClassify[i] = planes[i].Classify(points[i]);
            }
        });

        RUN_CASE("plane_distance_many", backend, sizeof(VxPlane) * static_cast<size_t>(kCount) + sizeof(VxVector) * static_cast<size_t>(kCount),
                kWarmup, kMeasure, {
            for (int i = 0; i < kCount; ++i) {
                outDistance[i] = planes[i].Distance(points[i]);
            }
        });

        RUN_CASE("plane_nearest_point_many", backend, sizeof(VxPlane) * static_cast<size_t>(kCount) + sizeof(VxVector) * static_cast<size_t>(kCount) * 2,
                kWarmup, kMeasure, {
            for (int i = 0; i < kCount; ++i) {
                nearest[i] = planes[i].NearestPoint(points[i]);
            }
        });

        RUN_CASE("plane_classify_obb_many", backend, sizeof(VxPlane) * static_cast<size_t>(kCount) + sizeof(VxBbox) * static_cast<size_t>(kCount),
                kWarmup, kMeasure, {
            for (int i = 0; i < kCount; ++i) {
                outClassify[i] = planes[i].Classify(boxes[i], mats[i]);
            }
        });
    }

    {
        constexpr int kCount = 1 << 18;
        PerfBuffer<VxRect> rects(kCount);
        for (int i = 0; i < kCount; ++i) {
            const float l = rng.NextFloat(-100.0f, 100.0f) * 0.5f;
            const float t = rng.NextFloat(-100.0f, 100.0f) * 0.5f;
            rects[i] = VxRect(l, t, l + 200.0f, t + 100.0f);
        }

        const VxRect srcScreen(0.0f, 0.0f, 1920.0f, 1080.0f);
        const VxRect dstScreen(0.0f, 0.0f, 1280.0f, 720.0f);
        const Vx2DVector srcSize(1920.0f, 1080.0f);
        const Vx2DVector dstSize(1280.0f, 720.0f);

        RUN_CASE("rect_transform_screen_many", backend, sizeof(VxRect) * static_cast<size_t>(kCount), kWarmup, kMeasure, {
            for (int i = 0; i < kCount; ++i) {
                rects[i].Transform(dstScreen, srcScreen);
            }
        });

        RUN_CASE("rect_transform_size_many", backend, sizeof(VxRect) * static_cast<size_t>(kCount), kWarmup, kMeasure, {
            for (int i = 0; i < kCount; ++i) {
                rects[i].Transform(dstSize, srcSize);
            }
        });

        RUN_CASE("rect_to_homogeneous_many", backend, sizeof(VxRect) * static_cast<size_t>(kCount), kWarmup, kMeasure, {
            for (int i = 0; i < kCount; ++i) {
                rects[i].TransformToHomogeneous(dstScreen);
            }
        });

        RUN_CASE("rect_from_homogeneous_many", backend, sizeof(VxRect) * static_cast<size_t>(kCount), kWarmup, kMeasure, {
            for (int i = 0; i < kCount; ++i) {
                rects[i].TransformFromHomogeneous(dstScreen);
            }
        });
    }

    {
        constexpr int kCount = 1 << 16;
        PerfBuffer<VxBbox> srcBoxes(kCount), dstBoxes(kCount);
        PerfBuffer<VxMatrix> transforms(kCount);
        PerfBuffer<VxVector> corners(kCount * 8);
        PerfBuffer<float> classifyOut(kCount);

        for (int i = 0; i < kCount; ++i) {
            srcBoxes[i] = VxBbox(VxVector(-1.0f, -1.0f, -1.0f), VxVector(1.0f, 1.0f, 1.0f));
            Vx3DMatrixFromEulerAngles(transforms[i], 0.02f * (i % 31), 0.03f * (i % 17), 0.04f * (i % 13));
            transforms[i][3][0] = rng.NextFloat(-100.0f, 100.0f);
            transforms[i][3][1] = rng.NextFloat(-100.0f, 100.0f);
            transforms[i][3][2] = rng.NextFloat(-100.0f, 100.0f);
        }

        RUN_CASE("bbox_transform_to_many", backend, sizeof(VxBbox) * static_cast<size_t>(kCount) + sizeof(VxVector) * static_cast<size_t>(kCount) * 8,
                kWarmup, kMeasure, {
            for (int i = 0; i < kCount; ++i) {
                srcBoxes[i].TransformTo(&corners[static_cast<size_t>(i) * 8], transforms[i]);
            }
        });

        RUN_CASE("bbox_transform_from_many", backend, sizeof(VxBbox) * static_cast<size_t>(kCount) * 2, kWarmup, kMeasure, {
            for (int i = 0; i < kCount; ++i) {
                dstBoxes[i].TransformFrom(srcBoxes[i], transforms[i]);
            }
        });

        const VxFrustum frustum(
            VxVector(0.0f, 0.0f, 0.0f),
            VxVector::axisX(),
            VxVector::axisY(),
            VxVector::axisZ(),
            0.5f,
            100.0f,
            1.0f,
            16.0f / 9.0f);
        RUN_CASE("frustum_classify_obb_many", backend, sizeof(VxBbox) * static_cast<size_t>(kCount), kWarmup, kMeasure, {
            for (int i = 0; i < kCount; ++i) {
                classifyOut[i] = frustum.Classify(srcBoxes[i], transforms[i]);
            }
        });
    }

    {
        constexpr int kCount = 1 << 14;
        PerfBuffer<VxFrustum> frusta(kCount);
        PerfBuffer<VxVector> vertices(kCount * 8);
        PerfBuffer<VxMatrix> transforms(kCount);

        for (int i = 0; i < kCount; ++i) {
            frusta[i] = VxFrustum(
                VxVector(rng.NextFloat(-100.0f, 100.0f), rng.NextFloat(-100.0f, 100.0f), rng.NextFloat(-100.0f, 100.0f)),
                VxVector::axisX(),
                VxVector::axisY(),
                VxVector::axisZ(),
                0.3f,
                250.0f,
                0.9f,
                16.0f / 9.0f);
            Vx3DMatrixFromEulerAngles(transforms[i], 0.05f * (i % 9), 0.07f * (i % 11), 0.11f * (i % 13));
            transforms[i][3][0] = rng.NextFloat(-100.0f, 100.0f) * 0.25f;
            transforms[i][3][1] = rng.NextFloat(-100.0f, 100.0f) * 0.25f;
            transforms[i][3][2] = rng.NextFloat(-100.0f, 100.0f) * 0.25f;
        }

        RUN_CASE("frustum_compute_vertices_many", backend, sizeof(VxFrustum) * static_cast<size_t>(kCount) + sizeof(VxVector) * static_cast<size_t>(kCount) * 8,
                kWarmup, kMeasure, {
            for (int i = 0; i < kCount; ++i) {
                frusta[i].ComputeVertices(&vertices[static_cast<size_t>(i) * 8]);
            }
        });

        RUN_CASE("frustum_transform_many", backend, sizeof(VxFrustum) * static_cast<size_t>(kCount), kWarmup, kMeasure, {
            for (int i = 0; i < kCount; ++i) {
                frusta[i].Transform(transforms[i]);
            }
        });
    }

    {
        constexpr int kCount = 1 << 15;
        PerfBuffer<VxMatrix> mats(kCount);
        PerfBuffer<VxBbox> boxes(kCount);
        PerfBuffer<VxRect> extents(kCount);
        PerfBuffer<float> outMin(kCount), outMax(kCount);
        VXCLIP_FLAGS orFlags = static_cast<VXCLIP_FLAGS>(0);
        VXCLIP_FLAGS andFlags = static_cast<VXCLIP_FLAGS>(0);
        const VxRect screen(0.0f, 0.0f, 1920.0f, 1080.0f);

        for (int i = 0; i < kCount; ++i) {
            Vx3DMatrixFromEulerAngles(mats[i], 0.02f * (i % 11), 0.03f * (i % 7), 0.05f * (i % 5));
            mats[i][3][0] = rng.NextFloat(-100.0f, 100.0f) * 0.5f;
            mats[i][3][1] = rng.NextFloat(-100.0f, 100.0f) * 0.5f;
            mats[i][3][2] = 3.0f + (rng.NextFloat(-100.0f, 100.0f) * 0.1f);
            boxes[i] = VxBbox(VxVector(-1.0f, -1.0f, -1.0f), VxVector(1.0f, 1.0f, 1.0f));
            extents[i] = VxRect(0.0f, 0.0f, 0.0f, 0.0f);
        }

        RUN_CASE("transform_box2d_many", backend, sizeof(VxMatrix) * static_cast<size_t>(kCount) + sizeof(VxBbox) * static_cast<size_t>(kCount),
                kWarmup, kMeasure, {
            for (int i = 0; i < kCount; ++i) {
                VxTransformBox2D(mats[i], boxes[i], const_cast<VxRect *>(&screen), &extents[i], orFlags, andFlags);
            }
        });

        RUN_CASE("project_box_z_extents_many", backend, sizeof(VxMatrix) * static_cast<size_t>(kCount) + sizeof(VxBbox) * static_cast<size_t>(kCount),
                kWarmup, kMeasure, {
            for (int i = 0; i < kCount; ++i) {
                VxProjectBoxZExtents(mats[i], boxes[i], outMin[i], outMax[i]);
            }
        });
    }

    //==========================================================================
    // VxBlitEngine exhaustive paths (optimized + scalar fallbacks)
    //==========================================================================
    {
        constexpr int kW = 1920;
        constexpr int kH = 1080;
        constexpr int kWOdd = 1919;
        constexpr int kResizeW = 1281;
        constexpr int kResizeH = 721;
        constexpr int kMipW = 1024;
        constexpr int kMipH = 1024;

        const size_t pixels = static_cast<size_t>(kW) * kH;
        const size_t oddPixels = static_cast<size_t>(kWOdd) * kH;
        const size_t resizePixels = static_cast<size_t>(kResizeW) * kResizeH;
        const size_t mipPixels = static_cast<size_t>(kMipW) * kMipH;
        const size_t mipDstPixels = static_cast<size_t>(kMipW / 2) * (kMipH / 2);

        PerfBuffer<XBYTE> srcARGB(pixels * 4);
        PerfBuffer<XBYTE> dstARGB(pixels * 4);
        PerfBuffer<XBYTE> dstRGB32(pixels * 4);
        PerfBuffer<XBYTE> dstRGB24(pixels * 3);
        PerfBuffer<XBYTE> dst565(pixels * 2);
        PerfBuffer<XBYTE> dst555(pixels * 2);
        PerfBuffer<XBYTE> dst1555(pixels * 2);
        PerfBuffer<XBYTE> dst4444(pixels * 2);
        PerfBuffer<XBYTE> dstABGR(pixels * 4);
        PerfBuffer<XBYTE> dstRGBA(pixels * 4);
        PerfBuffer<XBYTE> dstBGRA(pixels * 4);
        PerfBuffer<XBYTE> pal8Src(pixels);
        PerfBuffer<XBYTE> pal8Dst(pixels);
        PerfBuffer<XBYTE> alphaValues(pixels);

        PerfBuffer<XBYTE> oddARGB(oddPixels * 4);
        PerfBuffer<XBYTE> oddRGB24(oddPixels * 3);
        PerfBuffer<XBYTE> odd565(oddPixels * 2);
        PerfBuffer<XBYTE> oddABGR(oddPixels * 4);

        PerfBuffer<XBYTE> resizeARGB(resizePixels * 4);
        PerfBuffer<XBYTE> resizeRGB24(resizePixels * 3);
        PerfBuffer<XBYTE> resize565(resizePixels * 2);
        PerfBuffer<XBYTE> resize1555(resizePixels * 2);
        PerfBuffer<XBYTE> resize4444(resizePixels * 2);

        PerfBuffer<XBYTE> mipARGBSrc(mipPixels * 4);
        PerfBuffer<XBYTE> mipARGBDst(mipDstPixels * 4);
        PerfBuffer<XBYTE> mipRGB24Src(mipPixels * 3);
        PerfBuffer<XBYTE> mipRGB24Dst(mipDstPixels * 3);
        PerfBuffer<XBYTE> mip565Src(mipPixels * 2);
        PerfBuffer<XBYTE> mip565Dst(mipDstPixels * 2);
        PerfBuffer<XBYTE> bumpRGB24Src(mipPixels * 3);
        PerfBuffer<XBYTE> bumpRGB24Work(mipPixels * 3);
        PerfBuffer<XBYTE> normalARGBSrc(mipPixels * 4);
        PerfBuffer<XBYTE> normalARGBWork(mipPixels * 4);
        PerfBuffer<XBYTE> normalRGB24Src(mipPixels * 3);
        PerfBuffer<XBYTE> normalRGB24Work(mipPixels * 3);

        PerfBuffer<XBYTE> palette(256 * 4);

        FillRandomBytes(srcARGB, rng);
        FillRandomBytes(dstARGB, rng);
        FillRandomBytes(dstRGB32, rng);
        FillRandomBytes(dstRGB24, rng);
        FillRandomBytes(dst565, rng);
        FillRandomBytes(dst555, rng);
        FillRandomBytes(dst1555, rng);
        FillRandomBytes(dst4444, rng);
        FillRandomBytes(dstABGR, rng);
        FillRandomBytes(dstRGBA, rng);
        FillRandomBytes(dstBGRA, rng);
        FillRandomBytes(pal8Src, rng);
        FillRandomBytes(pal8Dst, rng);
        FillRandomBytes(alphaValues, rng);
        FillRandomBytes(oddARGB, rng);
        FillRandomBytes(oddRGB24, rng);
        FillRandomBytes(odd565, rng);
        FillRandomBytes(oddABGR, rng);
        FillRandomBytes(resizeARGB, rng);
        FillRandomBytes(resizeRGB24, rng);
        FillRandomBytes(resize565, rng);
        FillRandomBytes(resize1555, rng);
        FillRandomBytes(resize4444, rng);
        FillRandomBytes(mipARGBSrc, rng);
        FillRandomBytes(mipARGBDst, rng);
        FillRandomBytes(mipRGB24Src, rng);
        FillRandomBytes(mipRGB24Dst, rng);
        FillRandomBytes(mip565Src, rng);
        FillRandomBytes(mip565Dst, rng);
        FillRandomBytes(bumpRGB24Src, rng);
        FillRandomBytes(bumpRGB24Work, rng);
        FillRandomBytes(normalARGBSrc, rng);
        FillRandomBytes(normalARGBWork, rng);
        FillRandomBytes(normalRGB24Src, rng);
        FillRandomBytes(normalRGB24Work, rng);
        FillRandomBytes(palette, rng);

        VxImageDescEx srcARGBDesc = MakeImageDesc(_32_ARGB8888, kW, kH, srcARGB.data());
        VxImageDescEx dstARGBDesc = MakeImageDesc(_32_ARGB8888, kW, kH, dstARGB.data());
        VxImageDescEx dstRGB32Desc = MakeImageDesc(_32_RGB888, kW, kH, dstRGB32.data());
        VxImageDescEx dstRGB24Desc = MakeImageDesc(_24_RGB888, kW, kH, dstRGB24.data());
        VxImageDescEx dst565Desc = MakeImageDesc(_16_RGB565, kW, kH, dst565.data());
        VxImageDescEx dst555Desc = MakeImageDesc(_16_RGB555, kW, kH, dst555.data());
        VxImageDescEx dst1555Desc = MakeImageDesc(_16_ARGB1555, kW, kH, dst1555.data());
        VxImageDescEx dst4444Desc = MakeImageDesc(_16_ARGB4444, kW, kH, dst4444.data());
        VxImageDescEx dstABGRDesc = MakeImageDesc(_32_ABGR8888, kW, kH, dstABGR.data());
        VxImageDescEx dstRGBADesc = MakeImageDesc(_32_RGBA8888, kW, kH, dstRGBA.data());
        VxImageDescEx dstBGRADesc = MakeImageDesc(_32_BGRA8888, kW, kH, dstBGRA.data());

        VxImageDescEx palSrcDesc = MakeImageDesc(_8_RGB332, kW, kH, pal8Src.data());
        palSrcDesc.ColorMapEntries = 256;
        palSrcDesc.BytesPerColorEntry = 4;
        palSrcDesc.ColorMap = palette.data();
        VxImageDescEx pal8DstDesc = MakeImageDesc(_8_RGB332, kW, kH, pal8Dst.data());

        VxImageDescEx oddARGBDesc = MakeImageDesc(_32_ARGB8888, kWOdd, kH, oddARGB.data());
        VxImageDescEx oddRGB24Desc = MakeImageDesc(_24_RGB888, kWOdd, kH, oddRGB24.data());
        VxImageDescEx odd565Desc = MakeImageDesc(_16_RGB565, kWOdd, kH, odd565.data());
        VxImageDescEx oddABGRDesc = MakeImageDesc(_32_ABGR8888, kWOdd, kH, oddABGR.data());

        VxImageDescEx resizeARGBDesc = MakeImageDesc(_32_ARGB8888, kResizeW, kResizeH, resizeARGB.data());
        VxImageDescEx resizeRGB24Desc = MakeImageDesc(_24_RGB888, kResizeW, kResizeH, resizeRGB24.data());
        VxImageDescEx resize565Desc = MakeImageDesc(_16_RGB565, kResizeW, kResizeH, resize565.data());
        VxImageDescEx resize1555Desc = MakeImageDesc(_16_ARGB1555, kResizeW, kResizeH, resize1555.data());
        VxImageDescEx resize4444Desc = MakeImageDesc(_16_ARGB4444, kResizeW, kResizeH, resize4444.data());

        VxImageDescEx mipARGBSrcDesc = MakeImageDesc(_32_ARGB8888, kMipW, kMipH, mipARGBSrc.data());
        VxImageDescEx mipRGB24SrcDesc = MakeImageDesc(_24_RGB888, kMipW, kMipH, mipRGB24Src.data());
        VxImageDescEx mip565SrcDesc = MakeImageDesc(_16_RGB565, kMipW, kMipH, mip565Src.data());
        VxImageDescEx bumpRGB24Desc = MakeImageDesc(_24_RGB888, kMipW, kMipH, bumpRGB24Work.data());
        VxImageDescEx normalARGBDesc = MakeImageDesc(_32_ARGB8888, kMipW, kMipH, normalARGBWork.data());
        VxImageDescEx normalRGB24Desc = MakeImageDesc(_24_RGB888, kMipW, kMipH, normalRGB24Work.data());

        // Specific x86 conversion table coverage.
        RUN_BLIT_CASE("blit_argb_to_rgb32_1080p", srcARGBDesc, dstRGB32Desc);
        RUN_BLIT_CASE("blit_rgb32_to_argb_1080p", dstRGB32Desc, dstARGBDesc);
        RUN_BLIT_CASE("blit_argb_to_rgb24_1080p", srcARGBDesc, dstRGB24Desc);
        RUN_BLIT_CASE("blit_rgb24_to_argb_1080p", dstRGB24Desc, dstARGBDesc);
        RUN_BLIT_CASE("blit_argb_to_565_1080p", srcARGBDesc, dst565Desc);
        RUN_BLIT_CASE("blit_argb_to_555_1080p", srcARGBDesc, dst555Desc);
        RUN_BLIT_CASE("blit_argb_to_1555_1080p", srcARGBDesc, dst1555Desc);
        RUN_BLIT_CASE("blit_argb_to_4444_1080p", srcARGBDesc, dst4444Desc);
        RUN_BLIT_CASE("blit_565_to_argb_1080p", dst565Desc, dstARGBDesc);
        RUN_BLIT_CASE("blit_555_to_argb_1080p", dst555Desc, dstARGBDesc);
        RUN_BLIT_CASE("blit_1555_to_argb_1080p", dst1555Desc, dstARGBDesc);
        RUN_BLIT_CASE("blit_4444_to_argb_1080p", dst4444Desc, dstARGBDesc);
        RUN_BLIT_CASE("blit_argb_to_abgr_1080p", srcARGBDesc, dstABGRDesc);
        RUN_BLIT_CASE("blit_abgr_to_argb_1080p", dstABGRDesc, dstARGBDesc);
        RUN_BLIT_CASE("blit_argb_to_rgba_1080p", srcARGBDesc, dstRGBADesc);
        RUN_BLIT_CASE("blit_rgba_to_argb_1080p", dstRGBADesc, dstARGBDesc);
        RUN_BLIT_CASE("blit_argb_to_bgra_1080p", srcARGBDesc, dstBGRADesc);
        RUN_BLIT_CASE("blit_bgra_to_argb_1080p", dstBGRADesc, dstARGBDesc);

        // Paletted conversion coverage.
        RUN_BLIT_CASE("blit_pal8_to_pal8_1080p", palSrcDesc, pal8DstDesc);
        RUN_BLIT_CASE("blit_pal8_to_565_1080p", palSrcDesc, dst565Desc);
        RUN_BLIT_CASE("blit_pal8_to_24_1080p", palSrcDesc, dstRGB24Desc);
        RUN_BLIT_CASE("blit_pal8_to_32argb_1080p", palSrcDesc, dstARGBDesc);
        RUN_BLIT_CASE("blit_pal8_to_1555alpha_1080p", palSrcDesc, dst1555Desc);
        RUN_BLIT_CASE("blit_pal8_to_4444alpha_1080p", palSrcDesc, dst4444Desc);

        // Odd width tails for vectorized paths.
        RUN_BLIT_CASE("blit_argb_to_rgb24_odd_width", oddARGBDesc, oddRGB24Desc);
        RUN_BLIT_CASE("blit_rgb24_to_argb_odd_width", oddRGB24Desc, oddARGBDesc);
        RUN_BLIT_CASE("blit_argb_to_565_odd_width", oddARGBDesc, odd565Desc);
        RUN_BLIT_CASE("blit_argb_to_abgr_odd_width", oddARGBDesc, oddABGRDesc);

        // Upside-down path.
        RUN_UPSIDE_DOWN_CASE("blit_upsidedown_argb_to_argb_1080p", srcARGBDesc, dstARGBDesc);

        // Alpha APIs (constant and per-pixel).
        VxImageDescEx alpha8Desc = MakeImageDesc(_8_ARGB2222, kW, kH, pal8Dst.data());
        RUN_CASE("alpha_const_8_1080p", backend, ImageBytes(alpha8Desc), kWarmup, kMeasure, {
            VxDoAlphaBlit(alpha8Desc, static_cast<XBYTE>(0x7Fu));
        });
        RUN_CASE("alpha_array_8_1080p", backend, ImageBytes(alpha8Desc) + alphaValues.size(), kWarmup, kMeasure, {
            VxDoAlphaBlit(alpha8Desc, alphaValues.data());
        });
        RUN_CASE("alpha_const_16_1555_1080p", backend, ImageBytes(dst1555Desc), kWarmup, kMeasure, {
            VxDoAlphaBlit(dst1555Desc, static_cast<XBYTE>(0xA5u));
        });
        RUN_CASE("alpha_array_16_1555_1080p", backend, ImageBytes(dst1555Desc) + alphaValues.size(), kWarmup, kMeasure, {
            VxDoAlphaBlit(dst1555Desc, alphaValues.data());
        });
        // Backward-compatible aliases for historical perf dashboards.
        RUN_CASE("alpha_const_16_1080p", backend, ImageBytes(dst1555Desc), kWarmup, kMeasure, {
            VxDoAlphaBlit(dst1555Desc, static_cast<XBYTE>(0xA5u));
        });
        RUN_CASE("alpha_array_16_1080p", backend, ImageBytes(dst1555Desc) + alphaValues.size(), kWarmup, kMeasure, {
            VxDoAlphaBlit(dst1555Desc, alphaValues.data());
        });
        RUN_CASE("alpha_const_16_4444_1080p", backend, ImageBytes(dst4444Desc), kWarmup, kMeasure, {
            VxDoAlphaBlit(dst4444Desc, static_cast<XBYTE>(0xA5u));
        });
        RUN_CASE("alpha_array_16_4444_1080p", backend, ImageBytes(dst4444Desc) + alphaValues.size(), kWarmup, kMeasure, {
            VxDoAlphaBlit(dst4444Desc, alphaValues.data());
        });
        RUN_CASE("alpha_const_32_1080p", backend, ImageBytes(dstARGBDesc), kWarmup, kMeasure, {
            VxDoAlphaBlit(dstARGBDesc, static_cast<XBYTE>(0xD3u));
        });
        RUN_CASE("alpha_array_32_1080p", backend, ImageBytes(dstARGBDesc) + alphaValues.size(), kWarmup, kMeasure, {
            VxDoAlphaBlit(dstARGBDesc, alphaValues.data());
        });

        // Hot image operations.
        RUN_CASE("clear_alpha_1080p", backend, ImageBytes(dstARGBDesc), kWarmup, kMeasure, {
            TheBlitter.ClearAlpha(dstARGBDesc);
        });
        RUN_CASE("set_full_alpha_1080p", backend, ImageBytes(dstARGBDesc), kWarmup, kMeasure, {
            TheBlitter.SetFullAlpha(dstARGBDesc);
        });
        RUN_CASE("invert_colors_1080p", backend, ImageBytes(dstARGBDesc), kWarmup, kMeasure, {
            TheBlitter.InvertColors(dstARGBDesc);
        });
        RUN_CASE("grayscale_1080p", backend, ImageBytes(dstARGBDesc), kWarmup, kMeasure, {
            TheBlitter.ConvertToGrayscale(dstARGBDesc);
        });
        RUN_CASE("premultiply_1080p", backend, ImageBytes(dstARGBDesc), kWarmup, kMeasure, {
            TheBlitter.PremultiplyAlpha(dstARGBDesc);
        });
        RUN_CASE("unpremultiply_1080p", backend, ImageBytes(dstARGBDesc), kWarmup, kMeasure, {
            TheBlitter.UnpremultiplyAlpha(dstARGBDesc);
        });
        RUN_CASE("swap_red_blue_1080p", backend, ImageBytes(dstARGBDesc), kWarmup, kMeasure, {
            TheBlitter.SwapRedBlue(dstARGBDesc);
        });
        RUN_CASE("multiply_blend_1080p", backend, ImageBytes(srcARGBDesc) + ImageBytes(dstARGBDesc), kWarmup, kMeasure, {
            TheBlitter.MultiplyBlend(srcARGBDesc, dstARGBDesc);
        });

        // Mipmap and bump-map image kernels.
        RUN_CASE("mipmap_argb32_1024", backend, ImageBytes(mipARGBSrcDesc) + mipARGBDst.size(), kWarmup, kMeasure, {
            VxGenerateMipMap(mipARGBSrcDesc, mipARGBDst.data());
        });
        RUN_CASE("mipmap_rgb24_1024", backend, ImageBytes(mipRGB24SrcDesc) + mipRGB24Dst.size(), kWarmup, kMeasure, {
            VxGenerateMipMap(mipRGB24SrcDesc, mipRGB24Dst.data());
        });
        RUN_CASE("mipmap_rgb565_1024", backend, ImageBytes(mip565SrcDesc) + mip565Dst.size(), kWarmup, kMeasure, {
            VxGenerateMipMap(mip565SrcDesc, mip565Dst.data());
        });
        RUN_CASE("bump_rgb24_1024", backend, bumpRGB24Src.size() + bumpRGB24Work.size(), kWarmup, kMeasure, {
            memcpy(bumpRGB24Work.data(), bumpRGB24Src.data(), bumpRGB24Src.size());
            VxConvertToBumpMap(bumpRGB24Desc);
        });
        RUN_CASE("normal_argb32_1024", backend, normalARGBSrc.size() + normalARGBWork.size(), kWarmup, kMeasure, {
            memcpy(normalARGBWork.data(), normalARGBSrc.data(), normalARGBSrc.size());
            VxConvertToNormalMap(normalARGBDesc, 0xFFFFFFFFu);
        });
        RUN_CASE("normal_rgb24_1024", backend, normalRGB24Src.size() + normalRGB24Work.size(), kWarmup, kMeasure, {
            memcpy(normalRGB24Work.data(), normalRGB24Src.data(), normalRGB24Src.size());
            VxConvertToNormalMap(normalRGB24Desc, 0xFFFFFFFFu);
        });

        // Fill paths.
        RUN_CASE("fill32_odd_width", backend, ImageBytes(oddARGBDesc), kWarmup, kMeasure, {
            TheBlitter.FillImage(oddARGBDesc, 0x7FA0C0E0u);
        });
        RUN_CASE("fill16_1080p", backend, ImageBytes(dst565Desc), kWarmup, kMeasure, {
            TheBlitter.FillImage(dst565Desc, 0xA53Fu);
        });
        RUN_CASE("fill8_1080p", backend, ImageBytes(alpha8Desc), kWarmup, kMeasure, {
            TheBlitter.FillImage(alpha8Desc, 0x5Au);
        });
        RUN_CASE("fill24_1080p", backend, ImageBytes(dstRGB24Desc), kWarmup, kMeasure, {
            TheBlitter.FillImage(dstRGB24Desc, 0x00ABCDEFu);
        });

        // Resize paths.
        RUN_CASE("resize32_1080p_to_721p", backend, ImageBytes(srcARGBDesc) + ImageBytes(resizeARGBDesc), kWarmup, kMeasure, {
            VxResizeImage32(srcARGBDesc, resizeARGBDesc);
        });
        RUN_CASE("resize24_1080p_to_721p", backend, ImageBytes(dstRGB24Desc) + ImageBytes(resizeRGB24Desc), kWarmup, kMeasure, {
            TheBlitter.ResizeImage(dstRGB24Desc, resizeRGB24Desc);
        });
        RUN_CASE("resize565_nearest_1080p_to_721p", backend, ImageBytes(dst565Desc) + ImageBytes(resize565Desc), kWarmup, kMeasure, {
            TheBlitter.ResizeImage(dst565Desc, resize565Desc);
        });
        RUN_CASE("resize1555_nearest_1080p_to_721p", backend, ImageBytes(dst1555Desc) + ImageBytes(resize1555Desc), kWarmup, kMeasure, {
            TheBlitter.ResizeImage(dst1555Desc, resize1555Desc);
        });
        RUN_CASE("resize4444_nearest_1080p_to_721p", backend, ImageBytes(dst4444Desc) + ImageBytes(resize4444Desc), kWarmup, kMeasure, {
            TheBlitter.ResizeImage(dst4444Desc, resize4444Desc);
        });
    }

    return 0;
}


