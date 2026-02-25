#include <cstdio>
#include <cstring>

#include <gtest/gtest.h>

#include "VxMath.h"

namespace {

bool EqualsIgnoreCase(const char *a, const char *b) {
    if (a == nullptr || b == nullptr) {
        return false;
    }
    while (*a != '\0' && *b != '\0') {
        char ca = *a;
        char cb = *b;
        if (ca >= 'A' && ca <= 'Z') {
            ca = static_cast<char>(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = static_cast<char>(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return false;
        }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

bool ParseBackendMode(const char *value, int &mode) {
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

bool ParseBackendArg(int argc, char **argv, bool &hasMode, int &mode) {
    hasMode = false;
    mode = VX_SIMD_MODE_AUTO;
    if (argc <= 1 || argv == nullptr) {
        return true;
    }

    static const char *kArg = "--simd-backend";
    const size_t nameLen = std::strlen(kArg);
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        const char *value = nullptr;
        if (std::strcmp(arg, kArg) == 0) {
            if (i + 1 >= argc) {
                return false;
            }
            value = argv[++i];
        } else if (std::strncmp(arg, kArg, nameLen) == 0 && arg[nameLen] == '=') {
            value = arg + nameLen + 1;
        } else {
            continue;
        }

        if (!ParseBackendMode(value, mode)) {
            return false;
        }
        hasMode = true;
    }
    return true;
}

} // namespace

int main(int argc, char **argv) {
    bool hasMode = false;
    int mode = VX_SIMD_MODE_AUTO;
    if (!ParseBackendArg(argc, argv, hasMode, mode)) {
        std::fprintf(stderr, "Invalid --simd-backend argument\n");
        return 2;
    }

    if (hasMode) {
        if (!VxSetSIMDOverride(mode)) {
            std::fprintf(stderr, "Failed to apply --simd-backend=%s\n", VxGetSIMDBackendName(mode));
            return 2;
        }
    }

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
