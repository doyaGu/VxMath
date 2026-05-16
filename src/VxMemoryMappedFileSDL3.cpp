/**
 * @file VxMemoryMappedFileSDL3.cpp
 * @brief SDL3 implementation of VxMemoryMappedFile for read-only file access.
 *
 * This implementation uses SDL3's IOStream API for portable file access.
 * Note: SDL3 doesn't have native memory mapping, so we read the entire file
 * into memory for compatibility with the VxMemoryMappedFile interface.
 */

#include "VxMemoryMappedFile.h"

#include <SDL3/SDL_iostream.h>
#include <stdlib.h>
#include <string.h>

VxMemoryMappedFile::VxMemoryMappedFile(const char *pszFileName) {
    m_hFile = NULL;
    m_hFileMapping = NULL;
    m_pMemoryMappedFileBase = NULL;
    m_cbFile = 0;
    m_errCode = VxMMF_NoError;

    if (!pszFileName) {
        m_errCode = VxMMF_FileOpen;
        return;
    }

    // Open file using SDL3's IOStream
    SDL_IOStream *io = SDL_IOFromFile(pszFileName, "rb");
    if (!io) {
        m_errCode = VxMMF_FileOpen;
        return;
    }

    // Get file size
    Sint64 size = SDL_GetIOSize(io);
    if (size < 0) {
        SDL_CloseIO(io);
        m_errCode = VxMMF_FileMapping;
        return;
    }

    m_cbFile = (size_t)size;

    // Allocate memory and read the entire file
    m_pMemoryMappedFileBase = malloc(m_cbFile);
    if (!m_pMemoryMappedFileBase) {
        SDL_CloseIO(io);
        m_errCode = VxMMF_MapView;
        return;
    }

    // Read file contents
    size_t bytesRead = SDL_ReadIO(io, m_pMemoryMappedFileBase, m_cbFile);
    SDL_CloseIO(io);

    if (bytesRead != m_cbFile) {
        free(m_pMemoryMappedFileBase);
        m_pMemoryMappedFileBase = NULL;
        m_cbFile = 0;
        m_errCode = VxMMF_MapView;
        return;
    }

    // Mark as successfully mapped (handles are not used in SDL3 implementation)
    m_hFile = (GENERIC_HANDLE)(intptr_t)1;
    m_hFileMapping = (GENERIC_HANDLE)(intptr_t)1;
}

VxMemoryMappedFile::~VxMemoryMappedFile() {
    if (m_pMemoryMappedFileBase) {
        free(m_pMemoryMappedFileBase);
        m_pMemoryMappedFileBase = NULL;
    }
    m_hFile = NULL;
    m_hFileMapping = NULL;
    m_cbFile = 0;
}

void *VxMemoryMappedFile::GetBase() {
    return m_pMemoryMappedFileBase;
}

size_t VxMemoryMappedFile::GetFileSize() {
    return m_cbFile;
}

XBOOL VxMemoryMappedFile::IsValid() {
    return m_pMemoryMappedFileBase != NULL;
}

VxMMF_Error VxMemoryMappedFile::GetErrorType() {
    return m_errCode;
}
