#include "VxMemoryPool.h"

void *VxNewAligned(int size, int align)
{
    XBYTE *ptr = new XBYTE[size + align + 4];
    *(XBYTE **)((~(align - 1) & (*(XDWORD*)&ptr + align + 3)) - 4) = ptr;
    return (void *)(~(align - 1) & (*(XDWORD*)&ptr + align + 3));
}

void VxDeleteAligned(void *ptr)
{
    if (ptr) delete[] *((XBYTE **)ptr - 1);
}