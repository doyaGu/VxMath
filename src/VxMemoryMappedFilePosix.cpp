#include "VxMemoryMappedFile.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

VxMemoryMappedFile::VxMemoryMappedFile(char *pszFileName)
    : m_hFile(NULL),
      m_hFileMapping(NULL),
      m_pMemoryMappedFileBase(NULL),
      m_cbFile(0),
      m_errCode(VxMMF_FileOpen) {
    if (!pszFileName) {
        m_errCode = VxMMF_FileOpen;
        return;
    }

    int fd = open(pszFileName, O_RDONLY);
    if (fd < 0) {
        m_errCode = VxMMF_FileOpen;
        return;
    }

    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        close(fd);
        m_errCode = VxMMF_FileOpen;
        return;
    }

    m_cbFile = static_cast<size_t>(st.st_size);
    if (m_cbFile == 0) {
        close(fd);
        m_errCode = VxMMF_FileMapping;
        return;
    }

    void *mapped = mmap(NULL, m_cbFile, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) {
        close(fd);
        m_errCode = VxMMF_MapView;
        return;
    }

    int *ownedFd = new int(fd);
    m_hFile = static_cast<GENERIC_HANDLE>(ownedFd);
    m_hFileMapping = NULL;
    m_pMemoryMappedFileBase = mapped;
    m_errCode = VxMMF_NoError;
}

VxMemoryMappedFile::~VxMemoryMappedFile() {
    if (m_pMemoryMappedFileBase) {
        munmap(m_pMemoryMappedFileBase, m_cbFile);
        m_pMemoryMappedFileBase = NULL;
    }

    if (m_hFile) {
        int *ownedFd = static_cast<int *>(m_hFile);
        close(*ownedFd);
        delete ownedFd;
        m_hFile = NULL;
    }

    m_hFileMapping = NULL;
    m_cbFile = 0;
    m_errCode = VxMMF_FileOpen;
}

void *VxMemoryMappedFile::GetBase() {
    return m_pMemoryMappedFileBase;
}

size_t VxMemoryMappedFile::GetFileSize() {
    return m_cbFile;
}

XBOOL VxMemoryMappedFile::IsValid() {
    return m_errCode == VxMMF_NoError;
}

VxMMF_Error VxMemoryMappedFile::GetErrorType() {
    return m_errCode;
}
