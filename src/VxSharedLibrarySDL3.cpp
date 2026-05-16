/**
 * @file VxSharedLibrarySDL3.cpp
 * @brief SDL3 implementation of VxSharedLibrary for loading shared libraries.
 *
 * This implementation uses SDL3's shared object API for portable
 * dynamic library loading across all platforms.
 */

#include "VxSharedLibrary.h"

#include <SDL3/SDL_loadso.h>

VxSharedLibrary::VxSharedLibrary() {
    m_LibraryHandle = NULL;
}

void VxSharedLibrary::Attach(INSTANCE_HANDLE LibraryHandle) {
    m_LibraryHandle = LibraryHandle;
}

INSTANCE_HANDLE VxSharedLibrary::Load(const char *LibraryName) {
    m_LibraryHandle = (INSTANCE_HANDLE)SDL_LoadObject(LibraryName);
    return m_LibraryHandle;
}

void VxSharedLibrary::ReleaseLibrary() {
    if (m_LibraryHandle) {
        SDL_UnloadObject((SDL_SharedObject *)m_LibraryHandle);
        m_LibraryHandle = NULL;
    }
}

void *VxSharedLibrary::GetFunctionPtr(const char *FunctionName) {
    if (!m_LibraryHandle)
        return NULL;
    return (void *)SDL_LoadFunction((SDL_SharedObject *)m_LibraryHandle, FunctionName);
}
