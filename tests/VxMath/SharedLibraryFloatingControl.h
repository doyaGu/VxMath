#ifndef SHAREDLIBRARYFLOATINGCONTROL_H
#define SHAREDLIBRARYFLOATINGCONTROL_H

#include <stdint.h>

#if defined(_M_ARM64) || defined(_M_ARM64EC)
#include <intrin.h>

static const uint64_t SharedLibraryRoundingMask = uint64_t(3) << 22;
static const uint64_t SharedLibraryRoundDown = uint64_t(2) << 22;
static const uint64_t SharedLibraryFlushToZero = uint64_t(1) << 24;

static uint64_t SharedLibraryGetFloatingControl() {
    return static_cast<uint64_t>(_ReadStatusReg(ARM64_FPCR));
}

static void SharedLibrarySetFloatingControl(uint64_t control) {
    _WriteStatusReg(ARM64_FPCR, static_cast<int64_t>(control));
}
#else
#include <xmmintrin.h>

static const uint64_t SharedLibraryRoundingMask = 0x6000u;
static const uint64_t SharedLibraryRoundDown = 0x2000u;
static const uint64_t SharedLibraryFlushToZero = 0x8000u;

static uint64_t SharedLibraryGetFloatingControl() {
    return _mm_getcsr();
}

static void SharedLibrarySetFloatingControl(uint64_t control) {
    _mm_setcsr(static_cast<unsigned>(control));
}
#endif

#endif // SHAREDLIBRARYFLOATINGCONTROL_H
