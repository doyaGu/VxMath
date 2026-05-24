#pragma once

#include "VxMath.h"

void VxGenerateMipMapKernel(const VxImageDescEx &src_desc, XBYTE *Buffer, bool useSIMD);
XBOOL VxConvertToNormalMapKernel(const VxImageDescEx &image, XDWORD ColorMask, bool useSIMD);
XBOOL VxConvertToBumpMapKernel(const VxImageDescEx &image, bool useSIMD);

XBOOL VxGenerateMipMap16Rgb565SSE2(const VxImageDescEx &src_desc, XBYTE *Buffer);
XBOOL VxGenerateMipMap24Rgb888SSSE3(const VxImageDescEx &src_desc, XBYTE *Buffer);
