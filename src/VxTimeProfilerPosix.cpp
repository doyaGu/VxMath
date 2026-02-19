#include "VxTimeProfiler.h"

#include <string.h>
#include <time.h>

static uint64_t VxMonotonicNowNs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ull + static_cast<uint64_t>(ts.tv_nsec);
}

VxTimeProfiler &VxTimeProfiler::operator=(const VxTimeProfiler &t) {
    if (&t != this) {
        memcpy(Times, t.Times, sizeof(Times));
    }
    return *this;
}

void VxTimeProfiler::Reset() {
    const uint64_t now = VxMonotonicNowNs();
    memcpy(&Times[0], &now, sizeof(now));
    const uint64_t zero = 0;
    memcpy(&Times[2], &zero, sizeof(zero));
}

float VxTimeProfiler::Current() {
    uint64_t start = 0;
    memcpy(&start, &Times[0], sizeof(start));

    const uint64_t elapsedNs = VxMonotonicNowNs() - start;
    memcpy(&Times[2], &elapsedNs, sizeof(elapsedNs));

    return static_cast<float>(elapsedNs / 1000000.0);
}
