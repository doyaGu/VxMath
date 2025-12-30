#include "VxMemory.h"

#include "VxMathDefines.h"

// Basic memory allocation functions
void *mynew(unsigned int n) {
    void *ptr = operator new(n);
    return ptr;
}

void mydelete(void *a) {
    if (a) {
        operator delete(a);
    }
}

// Array allocation functions
void *mynewarray(unsigned int n) {
    void *ptr = operator new[](n);
    return ptr;
}

void mydeletearray(void *a) {
    if (a) {
        operator delete[](a);
    }
}

void *VxNewAligned(int size, int align) {
    XBYTE *ptr = new XBYTE[size + align + 4];
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t aligned = (~static_cast<uintptr_t>(align - 1) & (addr + align + 3));
    *(reinterpret_cast<XBYTE**>(aligned - 4)) = ptr;
    return reinterpret_cast<void*>(aligned);
}

void VxDeleteAligned(void *ptr) {
    if (ptr) delete[] *((XBYTE **) ptr - 1);
}
