#include "VxMath.h"

#include <stdio.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#if defined(__GNUC__)
#include <cpuid.h>
#elif defined(_MSC_VER)
#include <intrin.h>
#endif

#include "cpuinfo.h"

HINSTANCE g_hinstDLL;
CRITICAL_SECTION g_CriticalSection;

VxMatrix g_IdentityMatrix;
int g_QuantizationSamplingFactor;
float g_MSecondsPerCycle;

ProcessorsType g_ProcessorType;
int g_ProcessorFrequency;
XULONG g_ProcessorFeatures;
XULONG g_InstructionSetExtensions;
char g_ProcDescription[64];

typedef struct CpuIdRegs {
    XULONG eax;
    XULONG ebx;
    XULONG ecx;
    XULONG edx;
} CpuIdRegs;

static inline CpuIdRegs cpuid(XULONG eax) {
    CpuIdRegs regs;
#if defined(__GNUC__)
    __cpuid(eax, regs.eax, regs.ebx, regs.ecx, regs.edx);
#else
    int regs_array[4];
    __cpuid(regs_array, (int) eax);
    regs.eax = regs_array[0];
    regs.ebx = regs_array[1];
    regs.ecx = regs_array[2];
    regs.edx = regs_array[3];
#endif
    return regs;
}

void InitVxMath() {
    VxDetectProcessor();
    ::InitializeCriticalSection(&g_CriticalSection);
    g_IdentityMatrix.SetIdentity();
    g_QuantizationSamplingFactor = 15;
}

void VxDetectProcessor() {
    static XBOOL ProcessorDetected = FALSE;
    if (ProcessorDetected)
        return;

    ::OutputDebugString("VxMath: Detecting processor------------------------\n");
    HANDLE hThread = ::GetCurrentThread();
    int priority = ::GetThreadPriority(hThread);
    ::SetThreadPriority(hThread, THREAD_PRIORITY_IDLE);
    ::Sleep(10);
    static LARGE_INTEGER freq;
    ::QueryPerformanceFrequency(&freq);
    DWORD64 timeStamp1 = __rdtsc();
    LARGE_INTEGER freq1;
    ::QueryPerformanceFrequency(&freq1);
    DWORD64 timeStamp2 = __rdtsc();
    {
        int x = 0, y = 0, z = 0;
        for (int i = 0; i < 500000; ++i) {
            x += ++y;
            z += x;
            x -= z;
        }
    }
    LARGE_INTEGER freq2;
    ::QueryPerformanceFrequency(&freq2);
    DWORD64 timeStamp3 = __rdtsc();
    double t1 = (double) (freq2.LowPart - freq1.LowPart) / (double) freq.LowPart;
    double t2 = (double) (timeStamp3 - timeStamp2);
    g_MSecondsPerCycle = (float) (1000.0 * t1 / t2);
    g_ProcessorFrequency = t2 / t1 / 1000000.0;
    ::SetThreadPriority(hThread, priority);

    cpuinfo_initialize();
    switch (cpuinfo_get_uarch(0)->uarch) {
        case cpuinfo_uarch_p5:
            if (!cpuinfo_has_x86_mmx())
                g_ProcessorType = PROC_PENTIUM;
            else
                g_ProcessorType = PROC_PENTIUMMMX;
            break;
        case cpuinfo_uarch_p6:
            g_ProcessorType = PROC_PENTIUM3;
            break;
        case cpuinfo_uarch_willamette:
            g_ProcessorType = PROC_PENTIUM4;
            break;
        case cpuinfo_uarch_k8:
            g_ProcessorType = PROC_ATHLON;
            break;
        default:
            break;
    }

    strncpy(g_ProcDescription, cpuinfo_get_processor(0)->package->name, 64);

    CpuIdRegs regs = cpuid(0);
    g_ProcessorFeatures = regs.edx;

    g_InstructionSetExtensions = ISEX_NONE;
    if (cpuinfo_has_x86_sse())
        g_InstructionSetExtensions |= ISEX_SSE;
    if (cpuinfo_has_x86_sse2())
        g_InstructionSetExtensions |= ISEX_SSE2;
    if (cpuinfo_has_x86_sse3())
        g_InstructionSetExtensions |= ISEX_SSE3;
    if (cpuinfo_has_x86_ssse3())
        g_InstructionSetExtensions |= ISEX_SSSE3;
    if (cpuinfo_has_x86_sse4_1())
        g_InstructionSetExtensions |= ISEX_SSE41;
    if (cpuinfo_has_x86_sse4_2())
        g_InstructionSetExtensions |= ISEX_SSE42;
    if (cpuinfo_has_x86_avx())
        g_InstructionSetExtensions |= ISEX_AVX;
    if (cpuinfo_has_x86_avx2())
        g_InstructionSetExtensions |= ISEX_AVX2;

    if (g_ProcessorFrequency < 1000)
        sprintf(g_ProcDescription, "%s %d Mhz", g_ProcDescription, g_ProcessorFrequency);
    else
        sprintf(g_ProcDescription, "%s %g Ghz", g_ProcDescription, g_ProcessorFrequency / 1000.0);

    ProcessorDetected = TRUE;
}

void InterpolateFloatArray(void *Res, void *array1, void *array2, float factor, int count) {

}

void InterpolateVectorArray(void *Res, void *Inarray1, void *Inarray2, float factor, int count, XULONG StrideRes, XULONG StrideIn) {

}

XBOOL VxTransformBox2D(const VxMatrix &World_ProjectionMat, const VxBbox &box, VxRect *ScreenSize, VxRect *Extents, VXCLIP_FLAGS &OrClipFlags,
                       VXCLIP_FLAGS &AndClipFlags) {
    return 0;
}

void VxProjectBoxZExtents(const VxMatrix &World_ProjectionMat, const VxBbox &box, float &ZhMin, float &ZhMax) {

}

XBOOL VxFillStructure(int Count, void *Dst, XULONG Stride, XULONG SizeSrc, void *Src) {
    return 0;
}

XBOOL VxCopyStructure(int Count, void *Dst, XULONG OutStride, XULONG SizeSrc, void *Src, XULONG InStride) {
    return 0;
}

XBOOL VxIndexedCopy(const VxStridedData &Dst, const VxStridedData &Src, XULONG SizeSrc, int *Indices, int IndexCount) {
    return 0;
}

void VxDoBlit(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {

}

void VxDoBlitUpsideDown(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {

}

void VxDoAlphaBlit(const VxImageDescEx &dst_desc, XBYTE AlphaValue) {

}

void VxDoAlphaBlit(const VxImageDescEx &dst_desc, XBYTE *AlphaValues) {

}

void VxGetBitCounts(const VxImageDescEx &desc, XULONG &Rbits, XULONG &Gbits, XULONG &Bbits, XULONG &Abits) {

}

void VxGetBitShifts(const VxImageDescEx &desc, XULONG &Rshift, XULONG &Gshift, XULONG &Bshift, XULONG &Ashift) {

}

void VxGenerateMipMap(const VxImageDescEx &src_desc, XBYTE *DestBuffer) {

}

void VxResizeImage32(const VxImageDescEx &src_desc, const VxImageDescEx &dst_desc) {

}

XBOOL VxConvertToNormalMap(const VxImageDescEx &image, XULONG ColorMask) {
    return 0;
}

XBOOL VxConvertToBumpMap(const VxImageDescEx &image) {
    return 0;
}

XULONG GetBitCount(XULONG dwMask) {
    return 0;
}

XULONG GetBitShift(XULONG dwMask) {
    return 0;
}

VX_PIXELFORMAT VxImageDesc2PixelFormat(const VxImageDescEx &desc) {
    return _16_RGB555;
}

void VxPixelFormat2ImageDesc(VX_PIXELFORMAT Pf, VxImageDescEx &desc) {

}

const char *VxPixelFormat2String(VX_PIXELFORMAT Pf) {
    return nullptr;
}

void VxBppToMask(VxImageDescEx &desc) {

}

int GetQuantizationSamplingFactor() {
    return 0;
}

void SetQuantizationSamplingFactor(int sf) {

}

XBOOL VxPtInRect(CKRECT *rect, CKPOINT *pt) {
    return 0;
}

char *GetProcessorDescription() {
    return g_ProcDescription;
}

int GetProcessorFrequency() {
    return g_ProcessorFrequency;
}

XULONG GetProcessorFeatures() {
    return g_ProcessorFeatures;
}

void ModifyProcessorFeatures(XULONG Add, XULONG Remove) {
    g_ProcessorFeatures = ~Remove & (Add | g_ProcessorFeatures);
}

ProcessorsType GetProcessorType() {
    return g_ProcessorType;
}

XULONG GetInstructionSetExtensions() {
    return g_InstructionSetExtensions;
}

XBOOL VxComputeBestFitBBox(const XBYTE *Points, const XULONG Stride, const int Count, VxMatrix &BBoxMatrix, const float AdditionalBorder) {
    return 0;
}

BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            g_hinstDLL = hinstDLL;
            InitVxMath();
            break;
        case DLL_PROCESS_DETACH:
            DeleteCriticalSection(&g_CriticalSection);
            break;
        default:
            break;
    }
    return TRUE;
}