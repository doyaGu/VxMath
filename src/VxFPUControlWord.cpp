#include "VxWindowFunctions.h"

#if defined(_MSC_VER) && defined(_M_X64) && !defined(__clang__)
// MSVC x64 uses the matching leaf procedures in VxFPUControlWordWin64.asm.
#if defined(VX_API)
#pragma comment(linker, "/EXPORT:?VxGetFPUControlWord@@YAGXZ")
#pragma comment(linker, "/EXPORT:?VxSetFPUControlWord@@YAXG@Z")
#pragma comment(linker, "/EXPORT:?VxSetBaseFPUControlWord@@YAXXZ")
#endif
#else
XWORD VxGetFPUControlWord() {
#if defined(__i386__) || defined(__x86_64__)
    XWORD controlWord;
    __asm__ __volatile__("fstcw %0" : "=m"(controlWord));
    return controlWord;
#elif defined(_MSC_VER) && defined(_M_IX86)
    XWORD controlWord;
    __asm fstcw controlWord
    return controlWord;
#else
    return 0x027F;
#endif
}

void VxSetFPUControlWord(XWORD Fpu) {
#if defined(__i386__) || defined(__x86_64__)
    __asm__ __volatile__("fldcw %0" : : "m"(Fpu));
#elif defined(_MSC_VER) && defined(_M_IX86)
    __asm fldcw Fpu
#else
    (void)Fpu;
#endif
}

void VxSetBaseFPUControlWord() {
    XWORD controlWord = VxGetFPUControlWord();
    XWORD base = static_cast<XWORD>((controlWord & 0xF0FF) | 0x003F);
    if (base != controlWord)
        VxSetFPUControlWord(base);
}
#endif
