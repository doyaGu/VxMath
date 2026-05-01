#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <chrono>
#include <thread>
#endif

#include "VxMath.h"

#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
#define VX_HAS_X86_CPUID 1
#else
#define VX_HAS_X86_CPUID 0
#endif

#if VX_HAS_X86_CPUID && defined(__GNUC__)
#include <cpuid.h>
#include <x86intrin.h>
#elif VX_HAS_X86_CPUID && defined(_MSC_VER)
#include <intrin.h>
#endif

// Global variables
float g_MSecondsPerCycle;
ProcessorsType g_ProcessorType;
int g_ProcessorFrequency;
XDWORD g_ProcessorFeatures;
XDWORD g_InstructionSetExtensions;
char g_ProcDescription[64];

typedef struct CpuIdRegs {
    XDWORD eax;
    XDWORD ebx;
    XDWORD ecx;
    XDWORD edx;
} CpuIdRegs;

struct X86RuntimeState {
    bool avx;
    bool avx512;
    bool amx;
};

static bool HasBit(XDWORD value, int bit) {
    return (value & (static_cast<XDWORD>(1) << bit)) != 0;
}

static CpuIdRegs cpuid(XDWORD eax) {
    CpuIdRegs regs;
#if VX_HAS_X86_CPUID && defined(__GNUC__)
    __cpuid(eax, regs.eax, regs.ebx, regs.ecx, regs.edx);
#elif VX_HAS_X86_CPUID
    int regsArray[4];
    __cpuid(regsArray, (int) eax);
    regs.eax = regsArray[0];
    regs.ebx = regsArray[1];
    regs.ecx = regsArray[2];
    regs.edx = regsArray[3];
#else
    regs.eax = regs.ebx = regs.ecx = regs.edx = 0;
#endif
    return regs;
}

static inline CpuIdRegs cpuidex(XDWORD eax, XDWORD ecx) {
    CpuIdRegs regs;
#if VX_HAS_X86_CPUID && defined(__GNUC__)
    __cpuid_count(eax, ecx, regs.eax, regs.ebx, regs.ecx, regs.edx);
#elif VX_HAS_X86_CPUID
    int regsArray[4];
    __cpuidex(regsArray, (int) eax, (int) ecx);
    regs.eax = regsArray[0];
    regs.ebx = regsArray[1];
    regs.ecx = regsArray[2];
    regs.edx = regsArray[3];
#else
    (void) eax;
    (void) ecx;
    regs.eax = regs.ebx = regs.ecx = regs.edx = 0;
#endif
    return regs;
}

#if VX_HAS_X86_CPUID
static uint64_t ReadXCR0() {
#if defined(_MSC_VER)
    return static_cast<uint64_t>(_xgetbv(0));
#elif defined(__GNUC__) || defined(__clang__)
    uint32_t eax = 0;
    uint32_t edx = 0;
    __asm__ volatile(".byte 0x0f, 0x01, 0xd0" : "=a"(eax), "=d"(edx) : "c"(0));
    return (static_cast<uint64_t>(edx) << 32) | eax;
#else
    return 0;
#endif
}
#endif

static X86RuntimeState GetX86RuntimeState(const CpuIdRegs &regs1) {
    X86RuntimeState state = {};
#if VX_HAS_X86_CPUID
    const bool osxsave = HasBit(regs1.ecx, 27);
    const uint64_t xcr0 = osxsave ? ReadXCR0() : 0;
    state.avx = osxsave && ((xcr0 & 0x6) == 0x6);
    state.avx512 = osxsave && ((xcr0 & 0xE6) == 0xE6);
    state.amx = osxsave && ((xcr0 & ((1ull << 17) | (1ull << 18))) == ((1ull << 17) | (1ull << 18)));
#else
    (void) regs1;
#endif
    return state;
}

static void AddInstructionSetFlag(bool supported, XDWORD flag) {
    if (supported) {
        g_InstructionSetExtensions |= flag;
    }
}

static void GetProcessorNameFromCpuid(char *name, size_t maxLen) {
#if !VX_HAS_X86_CPUID
#if defined(__aarch64__) || defined(_M_ARM64)
    strncpy(name, "ARM64", maxLen - 1);
#elif defined(__arm__) || defined(_M_ARM)
    strncpy(name, "ARM32", maxLen - 1);
#elif defined(__riscv)
    strncpy(name, "RISC-V", maxLen - 1);
#elif defined(__powerpc__) || defined(__ppc__) || defined(_M_PPC)
    strncpy(name, "PowerPC", maxLen - 1);
#elif defined(__mips__)
    strncpy(name, "MIPS", maxLen - 1);
#else
    strncpy(name, "Unknown Processor", maxLen - 1);
#endif
    name[maxLen - 1] = '\0';
    return;
#else
    CpuIdRegs regs;
    char brandString[49] = {0}; // 48 chars + null terminator

    // Check if extended functions are available
    regs = cpuid(0x80000000);
    if (regs.eax < 0x80000004) {
        strncpy(name, "Unknown Processor", maxLen - 1);
        name[maxLen - 1] = '\0';
        return;
    }

    // Get brand string from CPUID 0x80000002-0x80000004
    regs = cpuid(0x80000002);
    memcpy(&brandString[0], &regs.eax, 4);
    memcpy(&brandString[4], &regs.ebx, 4);
    memcpy(&brandString[8], &regs.ecx, 4);
    memcpy(&brandString[12], &regs.edx, 4);

    regs = cpuid(0x80000003);
    memcpy(&brandString[16], &regs.eax, 4);
    memcpy(&brandString[20], &regs.ebx, 4);
    memcpy(&brandString[24], &regs.ecx, 4);
    memcpy(&brandString[28], &regs.edx, 4);

    regs = cpuid(0x80000004);
    memcpy(&brandString[32], &regs.eax, 4);
    memcpy(&brandString[36], &regs.ebx, 4);
    memcpy(&brandString[40], &regs.ecx, 4);
    memcpy(&brandString[44], &regs.edx, 4);

    // Trim leading spaces
    char *trimmed = brandString;
    while (*trimmed == ' ') trimmed++;

    strncpy(name, trimmed, maxLen - 1);
    name[maxLen - 1] = '\0';
#endif
}

static ProcessorsType DetermineProcessorType() {
#if defined(__x86_64__) || defined(_M_X64)
    return PROC_PENTIUM4;
#elif defined(__i386__) || defined(_M_IX86)
    return PROC_PENTIUM4;
#elif defined(__aarch64__) || defined(_M_ARM64)
    return PROC_PPC_ARM;
#elif defined(__arm__) || defined(_M_ARM)
    return PROC_PPC_ARM;
#elif defined(__mips__)
    return PROC_PPC_MIPS;
#elif defined(__powerpc__) || defined(__ppc__) || defined(_M_PPC)
    return PROC_PPC_G4;
#elif defined(__riscv)
    return PROC_UNKNOWN;
#else
    return PROC_UNKNOWN;
#endif
}

static void DetectInstructionSets() {
    g_InstructionSetExtensions = ISEX_NONE;
#if !VX_HAS_X86_CPUID
    return;
#endif

    CpuIdRegs regs1 = cpuid(1);
    const X86RuntimeState runtimeState = GetX86RuntimeState(regs1);

    AddInstructionSetFlag(HasBit(regs1.edx, 25), ISEX_SSE);
    AddInstructionSetFlag(HasBit(regs1.edx, 26), ISEX_SSE2);
    AddInstructionSetFlag(HasBit(regs1.ecx, 0), ISEX_SSE3);
    AddInstructionSetFlag(HasBit(regs1.ecx, 1), ISEX_PCLMULQDQ);
    AddInstructionSetFlag(HasBit(regs1.ecx, 9), ISEX_SSSE3);
    AddInstructionSetFlag(HasBit(regs1.ecx, 12) && runtimeState.avx, ISEX_FMA3);
    AddInstructionSetFlag(HasBit(regs1.ecx, 19), ISEX_SSE41);
    AddInstructionSetFlag(HasBit(regs1.ecx, 20), ISEX_SSE42);
    AddInstructionSetFlag(HasBit(regs1.ecx, 22), ISEX_MOVBE);
    AddInstructionSetFlag(HasBit(regs1.ecx, 23), ISEX_POPCNT);
    AddInstructionSetFlag(HasBit(regs1.ecx, 25), ISEX_AES);
    AddInstructionSetFlag(HasBit(regs1.ecx, 28) && runtimeState.avx, ISEX_AVX);
    AddInstructionSetFlag(HasBit(regs1.ecx, 29) && runtimeState.avx, ISEX_F16C);
    AddInstructionSetFlag(HasBit(regs1.ecx, 30), ISEX_RDRAND);

    // Check extended features from CPUID function 7
    CpuIdRegs regs0 = cpuid(0);
    if (regs0.eax >= 7) {
        CpuIdRegs regs7 = cpuidex(7, 0);

        AddInstructionSetFlag(HasBit(regs7.ebx, 3), ISEX_BMI1);
        AddInstructionSetFlag(HasBit(regs7.ebx, 5) && runtimeState.avx, ISEX_AVX2);
        AddInstructionSetFlag(HasBit(regs7.ebx, 8), ISEX_BMI2);
        AddInstructionSetFlag(HasBit(regs7.ebx, 16) && runtimeState.avx512, ISEX_AVX512F);
        AddInstructionSetFlag(HasBit(regs7.ebx, 17) && runtimeState.avx512, ISEX_AVX512DQ);
        AddInstructionSetFlag(HasBit(regs7.ebx, 18), ISEX_RDSEED);
        AddInstructionSetFlag(HasBit(regs7.ebx, 28) && runtimeState.avx512, ISEX_AVX512CD);
        AddInstructionSetFlag(HasBit(regs7.ebx, 29), ISEX_SHA);
        AddInstructionSetFlag(HasBit(regs7.ebx, 30) && runtimeState.avx512, ISEX_AVX512BW);
        AddInstructionSetFlag(HasBit(regs7.ebx, 31) && runtimeState.avx512, ISEX_AVX512VL);
        AddInstructionSetFlag(HasBit(regs7.ecx, 11) && runtimeState.avx512, ISEX_AVX512VNNI);

        // AVXVNNI support (bit 4 in EAX from CPUID 7, subleaf 1)
        CpuIdRegs regs7_1 = cpuidex(7, 1);
        AddInstructionSetFlag(HasBit(regs7_1.eax, 4) && runtimeState.avx, ISEX_AVXVNNI);
        AddInstructionSetFlag(HasBit(regs7.edx, 24) && HasBit(regs7.edx, 25) && runtimeState.amx, ISEX_AMX);
    }

    // Check extended CPUID function 0x80000001 for AMD-specific features
    CpuIdRegs regs80000000 = cpuid(0x80000000);
    if (regs80000000.eax >= 0x80000001) {
        CpuIdRegs regs80000001 = cpuid(0x80000001);
        AddInstructionSetFlag(HasBit(regs80000001.ecx, 5), ISEX_LZCNT);
    }
}

#if defined(_WIN32)
static void MeasureProcessorTiming() {
    HANDLE hThread = ::GetCurrentThread();
    int priority = ::GetThreadPriority(hThread);
    ::SetThreadPriority(hThread, THREAD_PRIORITY_IDLE);
    ::Sleep(10);
#if VX_HAS_X86_CPUID
    static LARGE_INTEGER freq;
    ::QueryPerformanceFrequency(&freq);
    LARGE_INTEGER freq1;
    ::QueryPerformanceCounter(&freq1);
    DWORD64 timeStamp1 = __rdtsc();
    ::Sleep(50);
    DWORD64 timeStamp2 = __rdtsc();
    LARGE_INTEGER freq2;
    ::QueryPerformanceCounter(&freq2);
    double t1 = (double) (freq2.QuadPart - freq1.QuadPart) / (double) freq.QuadPart;
    double t2 = (double) (timeStamp2 - timeStamp1);
    if (t1 > 0.0 && t2 > 0.0) {
        g_MSecondsPerCycle = (float) (1000.0 * t1 / t2);
        g_ProcessorFrequency = (int) (t2 / t1 / 1000000.0);
    } else {
        g_MSecondsPerCycle = (float) (1000.0 / (double) freq.QuadPart);
        g_ProcessorFrequency = (int) ((double) freq.QuadPart / 1000000.0);
    }
#else
    LARGE_INTEGER freq;
    ::QueryPerformanceFrequency(&freq);
    g_MSecondsPerCycle = (float) (1000.0 / (double) freq.QuadPart);
    g_ProcessorFrequency = (int) ((double) freq.QuadPart / 1000000.0);
#endif
    ::SetThreadPriority(hThread, priority);
}
#else
static void MeasureProcessorTiming() {
    auto start = std::chrono::steady_clock::now();
#if VX_HAS_X86_CPUID
    uint64_t timeStamp1 = __rdtsc();
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
#if VX_HAS_X86_CPUID
    uint64_t timeStamp2 = __rdtsc();
#endif
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    const double t1 = elapsed.count();
#if VX_HAS_X86_CPUID
    const double t2 = (double) (timeStamp2 - timeStamp1);
#else
    const double t2 = 0.0;
#endif
    if (t1 > 0.0 && t2 > 0.0) {
        g_MSecondsPerCycle = (float) (1000.0 * t1 / t2);
        g_ProcessorFrequency = (int) (t2 / t1 / 1000000.0);
    } else {
        g_MSecondsPerCycle = 0.0f;
        g_ProcessorFrequency = (int) (1.0 / t1 / 1000000.0);
        if (g_ProcessorFrequency <= 0) {
            g_ProcessorFrequency = 1000;
        }
    }
}
#endif

static void FormatProcessorDescription() {
    char processorName[sizeof(g_ProcDescription)];
    strncpy(processorName, g_ProcDescription, sizeof(processorName) - 1);
    processorName[sizeof(processorName) - 1] = '\0';

    if (g_ProcessorFrequency < 1000) {
        snprintf(g_ProcDescription, sizeof(g_ProcDescription) - 1, "%s %d MHz", processorName, g_ProcessorFrequency);
    } else {
        snprintf(g_ProcDescription, sizeof(g_ProcDescription) - 1, "%s %.2f GHz", processorName,
                 g_ProcessorFrequency / 1000.0);
    }
    g_ProcDescription[sizeof(g_ProcDescription) - 1] = '\0';
}

static bool InitializeProcessorDetection() {
#if defined(_WIN32)
    ::OutputDebugString("VxMath: Detecting processor------------------------\n");
#else
    fprintf(stderr, "VxMath: Detecting processor------------------------\n");
#endif

    MeasureProcessorTiming();

    g_ProcessorType = DetermineProcessorType();
    GetProcessorNameFromCpuid(g_ProcDescription, sizeof(g_ProcDescription));

    CpuIdRegs regs = cpuid(1);
    g_ProcessorFeatures = regs.edx; // Standard x86 features
    g_ProcessorFeatures |= PROC_HASFPU;

    DetectInstructionSets();
    FormatProcessorDescription();

    return true;
}

void VxDetectProcessor() {
    static const bool initialized = InitializeProcessorDetection();

    (void) initialized;
}

const char *GetProcessorDescription() {
    return g_ProcDescription;
}

int GetProcessorFrequency() {
    return g_ProcessorFrequency;
}

XDWORD GetProcessorFeatures() {
    return g_ProcessorFeatures;
}

void ModifyProcessorFeatures(XDWORD Add, XDWORD Remove) {
    g_ProcessorFeatures = (g_ProcessorFeatures | Add) & ~Remove;
}

ProcessorsType GetProcessorType() {
    return g_ProcessorType;
}

XDWORD GetInstructionSetExtensions() {
    return g_InstructionSetExtensions;
}
