#include "VxSharedLibrary.h"
#include "VxFPUControlWord.h"

#include <dlfcn.h>
#include <fenv.h>

VxSharedLibrary::VxSharedLibrary() {
    m_LibraryHandle = NULL;
}

void VxSharedLibrary::Attach(INSTANCE_HANDLE LibraryHandle) {
    m_LibraryHandle = LibraryHandle;
}

INSTANCE_HANDLE VxSharedLibrary::Load(const char *LibraryName) {
    fenv_t environment;
    fegetenv(&environment);
    const XWORD controlWord = VxGetFPUControlWord();
    if (m_LibraryHandle) {
        ReleaseLibrary();
    }

    if (LibraryName) {
        m_LibraryHandle = dlopen(LibraryName, RTLD_NOW | RTLD_LOCAL);
    } else {
        m_LibraryHandle = dlopen(NULL, RTLD_NOW | RTLD_LOCAL);
    }
    fesetenv(&environment);
    VxSetFPUControlWord(controlWord);

    return m_LibraryHandle;
}

void VxSharedLibrary::ReleaseLibrary() {
    if (m_LibraryHandle) {
        dlclose(m_LibraryHandle);
        m_LibraryHandle = NULL;
    }
}

void *VxSharedLibrary::GetFunctionPtr(const char *FunctionName) {
    if (m_LibraryHandle && FunctionName) {
        return dlsym(m_LibraryHandle, FunctionName);
    }
    return NULL;
}
