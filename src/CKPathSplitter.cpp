#include "CKPathSplitter.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

static void CopyBounded(char *dst, size_t dstSize, const char *src, size_t count) {
    if (!dst || dstSize == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }

    if (count >= dstSize) {
        count = dstSize - 1;
    }
    if (count > 0) {
        memcpy(dst, src, count);
    }
    dst[count] = '\0';
}

static const char *FindLastPathSeparator(const char *path) {
    const char *lastSlash = strrchr(path, '/');
    const char *lastBackslash = strrchr(path, '\\');

    if (!lastSlash) {
        return lastBackslash;
    }
    if (!lastBackslash) {
        return lastSlash;
    }
    return (lastSlash > lastBackslash) ? lastSlash : lastBackslash;
}

static bool HasDrivePrefix(const char *path) {
    return path && isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':';
}

static char *DuplicatePathString(const char *text) {
    const size_t length = text ? strlen(text) : 0;
    char *copy = new char[length + 1];
    if (length > 0) {
        memcpy(copy, text, length);
    }
    copy[length] = '\0';
    return copy;
}

static XBOOL AddPathPartLength(size_t &total, const char *part) {
    if (!part) {
        return TRUE;
    }

    const size_t length = strlen(part);
    if (length > ((size_t) -1) - total) {
        return FALSE;
    }
    total += length;
    return TRUE;
}

static char *CreatePathMakerFileName(const char *drive, const char *dir, const char *name, const char *dot, const char *ext) {
    size_t length = 0;
    if (!AddPathPartLength(length, drive) ||
        !AddPathPartLength(length, dir) ||
        !AddPathPartLength(length, name) ||
        !AddPathPartLength(length, dot) ||
        !AddPathPartLength(length, ext)) {
        return DuplicatePathString("");
    }

    char *fileName = new char[length + 1];
    size_t pos = 0;
    const char *parts[] = {drive, dir, name, dot, ext};
    for (int i = 0; i < 5; ++i) {
        const char *part = parts[i];
        if (!part) {
            continue;
        }
        const size_t partLength = strlen(part);
        if (partLength > 0) {
            memcpy(fileName + pos, part, partLength);
            pos += partLength;
        }
    }
    fileName[pos] = '\0';
    return fileName;
}

CKPathSplitter::CKPathSplitter(const char *file) : m_Drive(), m_Dir(), m_Filename(), m_Ext() {
    if (!file || file[0] == '\0') {
        return;
    }

    const char *path = file;

    if (HasDrivePrefix(path)) {
        CopyBounded(m_Drive, sizeof(m_Drive), path, 2);
        path += 2;
    }

    const char *lastSep = FindLastPathSeparator(path);
    const char *baseName = path;

    if (lastSep) {
        CopyBounded(m_Dir, sizeof(m_Dir), path, static_cast<size_t>((lastSep - path) + 1));
        baseName = lastSep + 1;
    }

    if (*baseName == '\0') {
        return;
    }

    const char *lastDot = strrchr(baseName, '.');
    if (lastDot && lastDot != baseName) {
        CopyBounded(m_Filename, sizeof(m_Filename), baseName, static_cast<size_t>(lastDot - baseName));
        CopyBounded(m_Ext, sizeof(m_Ext), lastDot, strlen(lastDot));
    } else {
        CopyBounded(m_Filename, sizeof(m_Filename), baseName, strlen(baseName));
    }
}

CKPathSplitter::~CKPathSplitter() {}

const char *CKPathSplitter::GetDrive() const {
    return m_Drive;
}

const char *CKPathSplitter::GetDir() const {
    return m_Dir;
}

const char *CKPathSplitter::GetName() const {
    return m_Filename;
}

const char *CKPathSplitter::GetExtension() const {
    return m_Ext;
}

CKPathMaker::CKPathMaker(const char *Drive, const char *Directory, const char *Fname, const char *Extension) : m_FileName(NULL) {
    const char *drive = Drive ? Drive : "";
    const char *dir = Directory ? Directory : "";
    const char *name = Fname ? Fname : "";
    const char *ext = Extension ? Extension : "";
    const char *dot = (ext[0] != '\0' && ext[0] != '.') ? "." : "";

    m_FileName = CreatePathMakerFileName(drive, dir, name, dot, ext);
}

CKPathMaker::CKPathMaker(const CKPathMaker &other) : m_FileName(DuplicatePathString(other.GetFileName())) {
}

CKPathMaker &CKPathMaker::operator=(const CKPathMaker &other) {
    if (this != &other) {
        char *copy = DuplicatePathString(other.GetFileName());
        delete[] m_FileName;
        m_FileName = copy;
    }
    return *this;
}

CKPathMaker::~CKPathMaker() {
    delete[] m_FileName;
}

const char *CKPathMaker::GetFileName() const {
    return m_FileName ? m_FileName : "";
}
