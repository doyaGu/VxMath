/* Standalone build shim for the unmodified SDL libm subset. */
#include <stdint.h>
#include <SDL3/SDL_endian.h>
#include <SDL3/SDL_assert.h>
#define SDL_uclibc_sin VxLibmSin
#define SDL_uclibc_cos VxLibmCos
#define SDL_uclibc_fabs VxLibmFabs
#define SDL_uclibc_floor VxLibmFloor
#define SDL_uclibc_scalbn VxLibmScalbn
#define SDL_uclibc_scalbln VxLibmScalbln
#define SDL_uclibc_copysign VxLibmCopySign
#define __kernel_sin VxLibmKernelSin
#define __kernel_cos VxLibmKernelCos
#define __ieee754_rem_pio2 VxLibmRemPiOver2
#define __kernel_rem_pio2 VxLibmKernelRemPiOver2
