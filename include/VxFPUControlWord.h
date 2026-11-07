#ifndef VXFPUCONTROLWORD_H
#define VXFPUCONTROLWORD_H

#include "VxMathDefines.h"

// Get/set the control used by native callers and software extended arithmetic.
VX_EXPORT XWORD VxGetFPUControlWord();
VX_EXPORT void VxSetFPUControlWord(XWORD Fpu);
VX_EXPORT void VxSetBaseFPUControlWord();

// Initialize the current thread's default arithmetic environment before
// constructing runtime objects and after initializing external render drivers.
VX_EXPORT void VxInitializeMathEnvironment();

#endif
