#include "CKPathSplitter.h"
#include "VxPathEncoding.h"

#include <stdlib.h>
#include <stdio.h>

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
    const char *last = NULL;
    for (const char *p = path; *p; p = VxNextPathCharacter(p))
        if (*p == '/' || *p == '\\')
            last = p;
    return last;
}

static bool HasDrivePrefix(const char *path) {
    return path && path[0] && path[1] == ':';
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
    if (length >= ((size_t) -1) - total) {
        return FALSE;
    }
    total += length;
    return TRUE;
}

static char *CreatePathMakerFileName(const char *drive, const char *dir, const char *name, const char *dot, const char *ext) {
    char drivePrefix[3] = {};
    if (drive[0]) {
        drivePrefix[0] = drive[0];
        drivePrefix[1] = ':';
    }

    const size_t directoryLength = strlen(dir);
    bool hasSeparator = directoryLength && (dir[directoryLength - 1] == '/' || dir[directoryLength - 1] == '\\');
    // Only a possible DBCS trail separator needs a character-boundary scan.
    if (hasSeparator && directoryLength > 1 && static_cast<unsigned char>(dir[directoryLength - 2]) >= 0x80) {
        hasSeparator = VxPathEndsWithSeparator(dir);
    }
    const char *separator = directoryLength && !hasSeparator ? "\\" : "";

    const char *parts[] = {drivePrefix, dir, separator, name, dot, ext};
    size_t lengths[6], length = 0;
    for (int i = 0; i < 6; ++i) {
        const size_t previousLength = length;
        if (!AddPathPartLength(length, parts[i])) {
            return DuplicatePathString("");
        }
        lengths[i] = length - previousLength;
    }

    char *fileName = new char[length + 1];
    size_t pos = 0;
    for (int i = 0; i < 6; ++i) {
        if (lengths[i]) {
            memcpy(fileName + pos, parts[i], lengths[i]);
            pos += lengths[i];
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
    if (lastDot) {
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
