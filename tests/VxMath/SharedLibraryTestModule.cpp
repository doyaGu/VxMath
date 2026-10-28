#include <fenv.h>
#include <float.h>
#include <xmmintrin.h>
#include <windows.h>

#ifndef VX_LIBRARY_TEST_VALUE
#define VX_LIBRARY_TEST_VALUE 17
#endif

extern "C" int SharedLibraryTestValue() { return VX_LIBRARY_TEST_VALUE; }

extern "C" const char *SharedLibraryNextName(int choice) {
    static const char *names[] = {"kernel32.dll", "VxMissingBorrowedLibrary.dll", ""};
    return names[choice];
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
#if defined(VX_LIBRARY_DIRTY_DETACH)
    if (reason == DLL_PROCESS_DETACH) {
        fesetround(FE_DOWNWARD);
        feraiseexcept(FE_INVALID);
#if defined(_M_IX86)
        unsigned ignored;
        __control87_2(_PC_24, _MCW_PC, &ignored, nullptr);
#endif
        _mm_setcsr(_mm_getcsr() ^ 0x8000u);
    }
#endif
    if (reason == DLL_PROCESS_ATTACH) {
#if defined(VX_LIBRARY_DIRTY_FP)
        fesetround(FE_TOWARDZERO);
#if defined(_M_IX86)
        unsigned ignored;
        __control87_2(_PC_24, _MCW_PC, &ignored, nullptr);
#endif
        _mm_setcsr(_mm_getcsr() & ~0x8000u);
#endif
#if defined(VX_LIBRARY_REJECT_LOAD)
        return FALSE;
#endif
    }
    return TRUE;
}
