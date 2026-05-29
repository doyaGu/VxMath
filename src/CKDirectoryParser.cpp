#include "CKDirectoryParser.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <dirent.h>
#include <fnmatch.h>
#include <sys/stat.h>
#endif

static char *DuplicateCString(const char *value) {
    const char *text = value ? value : "";
    const size_t len = strlen(text);
    char *copy = new char[len + 1];
    memcpy(copy, text, len + 1);
    return copy;
}

static char *DuplicateDirectoryName(const char *dir) {
    char *copy = DuplicateCString(dir);
    size_t len = strlen(copy);
    if (len > 0 && (copy[len - 1] == '\\' || copy[len - 1] == '/')) {
        copy[len - 1] = '\0';
    }
    return copy;
}

static char NativePathSeparator() {
#if defined(_WIN32)
    return '\\';
#else
    return '/';
#endif
}

static XBOOL IsPathSeparator(char c) {
    return (c == '\\' || c == '/') ? TRUE : FALSE;
}

static char *JoinDirectoryPath(const char *dir, const char *leaf) {
    const char *directory = dir ? dir : "";
    const char *name = leaf ? leaf : "";

    size_t directoryLength = strlen(directory);
    while (directoryLength > 0 && IsPathSeparator(directory[directoryLength - 1]))
        --directoryLength;

    const size_t nameLength = strlen(name);
    const XBOOL needSeparator = (directoryLength > 0 && nameLength > 0 && !IsPathSeparator(name[0]));
    const size_t resultLength = directoryLength + (needSeparator ? 1 : 0) + nameLength;

    char *result = new char[resultLength + 1];
    size_t offset = 0;
    if (directoryLength > 0) {
        memcpy(result, directory, directoryLength);
        offset = directoryLength;
    }
    if (needSeparator)
        result[offset++] = NativePathSeparator();
    if (nameLength > 0)
        memcpy(result + offset, name, nameLength);
    result[resultLength] = '\0';
    return result;
}

static const char *StoreFullFileName(char *&storage, char *filename) {
    delete[] storage;
    storage = filename;
    return storage;
}

#if !defined(_WIN32)
static bool IsDirectory(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

static bool MatchMask(const char *name, const char *mask) {
    if (!mask || !*mask) {
        return true;
    }
    if (strcmp(mask, "*.*") == 0) {
        mask = "*";
    }
    int flags = 0;
#ifdef FNM_CASEFOLD
    flags |= FNM_CASEFOLD;
#endif
    return fnmatch(mask, name ? name : "", flags) == 0;
}
#endif

CKDirectoryParser::CKDirectoryParser(const char *dir, const char *fileMask, XBOOL recurse) {
    m_FindData = NULL;
    m_StartDir = NULL;
    m_FullFileName = NULL;
    m_FileMask = NULL;
    m_SubParser = NULL;
#if defined(_WIN32)
    m_hFile = -1;
#else
    m_hFile = 0;
#endif
    m_State = 0;
    Reset(dir, fileMask, recurse);
}

CKDirectoryParser::~CKDirectoryParser() {
    Clean();

    delete[] m_StartDir;
    delete[] m_FullFileName;
    delete[] m_FileMask;
}

const char *CKDirectoryParser::GetNextFile() {
#if defined(_WIN32)
    // Handle non-recursive mode or first phase of recursive mode (files in current directory)
    if ((m_State & 2) == 0) {
        // Use a loop to skip directories instead of recursive calls
        while (true) {
            if (m_hFile == -1) {
                char *search = JoinDirectoryPath(m_StartDir, m_FileMask);
                m_hFile = _findfirst(search, (_finddata_t*)m_FindData);
                delete[] search;
                if (m_hFile == -1) {
                    // No files found, move to subdirectory search if recursive
                    if ((m_State & 1) != 0) {
                        m_State |= 2;
                    } else {
                        return NULL;
                    }
                    break; // Exit file search loop
                }
            } else {
                if (_findnext(m_hFile, (_finddata_t*)m_FindData) != 0) {
                    // No more files, close handle and move to subdirectory search if recursive
                    _findclose(m_hFile);
                    m_hFile = -1;
                    if ((m_State & 1) != 0) {
                        m_State |= 2;
                    } else {
                        return NULL;
                    }
                    break; // Exit file search loop
                }
            }

            // Check if current item is a file (not directory)
            if ((((_finddata_t*)m_FindData)->attrib & _A_SUBDIR) == 0) {
                char *fullFileName = JoinDirectoryPath(m_StartDir, ((_finddata_t*)m_FindData)->name);
                return StoreFullFileName(m_FullFileName, fullFileName);
            }
            // If it's a directory, continue the loop to find next item
        }
    }

    // Handle recursive subdirectory search
    if ((m_State & 2) != 0) {
        // Check if we have an active subparser
        if (m_SubParser) {
            const char *ret = m_SubParser->GetNextFile();
            if (ret) {
                return ret;
            } else {
                // Subparser is exhausted, clean it up
                delete m_SubParser;
                m_SubParser = NULL;
            }
        }

        // Look for next subdirectory
        while (true) {
            if (m_hFile == -1) {
                char *search = JoinDirectoryPath(m_StartDir, "*.*");
                m_hFile = _findfirst(search, (_finddata_t*)m_FindData);
                delete[] search;
                if (m_hFile == -1) {
                    return NULL; // No subdirectories found
                }
            } else {
                if (_findnext(m_hFile, (_finddata_t*)m_FindData) != 0) {
                    // No more subdirectories
                    _findclose(m_hFile);
                    m_hFile = -1;
                    return NULL;
                }
            }

            // Check if current item is a valid subdirectory (not . or ..)
            if (strcmp(((_finddata_t*)m_FindData)->name, ".") != 0 &&
                strcmp(((_finddata_t*)m_FindData)->name, "..") != 0 &&
                (((_finddata_t*)m_FindData)->attrib & _A_SUBDIR) != 0) {

                char *dir = JoinDirectoryPath(m_StartDir, ((_finddata_t*)m_FindData)->name);
                m_SubParser = new CKDirectoryParser(dir, m_FileMask, TRUE);
                delete[] dir;

                const char *ret = m_SubParser->GetNextFile();
                if (ret) {
                    return ret;
                } else {
                    // This subdirectory has no files, clean up and continue looking
                    delete m_SubParser;
                    m_SubParser = NULL;
                }
            }
            // Continue looking for next subdirectory
        }
    }

    return NULL;
#else
    if ((m_State & 2) == 0) {
        while (true) {
            DIR *dir = reinterpret_cast<DIR *>(m_hFile);
            if (!dir) {
                dir = opendir(m_StartDir ? m_StartDir : ".");
                if (!dir) {
                    if ((m_State & 1) != 0) {
                        m_State |= 2;
                    } else {
                        return NULL;
                    }
                    break;
                }
                m_hFile = reinterpret_cast<intptr_t>(dir);
            }

            struct dirent *entry = readdir(dir);
            if (!entry) {
                closedir(dir);
                m_hFile = 0;
                if ((m_State & 1) != 0) {
                    m_State |= 2;
                } else {
                    return NULL;
                }
                break;
            }

            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            char *path = JoinDirectoryPath(m_StartDir, entry->d_name);
            if (IsDirectory(path)) {
                delete[] path;
                continue;
            }
            if (!MatchMask(entry->d_name, m_FileMask)) {
                delete[] path;
                continue;
            }

            return StoreFullFileName(m_FullFileName, path);
        }
    }

    if ((m_State & 2) != 0) {
        if (m_SubParser) {
            const char *ret = m_SubParser->GetNextFile();
            if (ret) {
                return ret;
            }
            delete m_SubParser;
            m_SubParser = NULL;
        }

        while (true) {
            DIR *dir = reinterpret_cast<DIR *>(m_hFile);
            if (!dir) {
                dir = opendir(m_StartDir ? m_StartDir : ".");
                if (!dir) {
                    return NULL;
                }
                m_hFile = reinterpret_cast<intptr_t>(dir);
            }

            struct dirent *entry = readdir(dir);
            if (!entry) {
                closedir(dir);
                m_hFile = 0;
                return NULL;
            }

            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            char *path = JoinDirectoryPath(m_StartDir, entry->d_name);
            if (IsDirectory(path)) {
                m_SubParser = new CKDirectoryParser(path, m_FileMask, TRUE);
                delete[] path;
                const char *ret = m_SubParser->GetNextFile();
                if (ret) {
                    return ret;
                }
                delete m_SubParser;
                m_SubParser = NULL;
            } else {
                delete[] path;
            }
        }
    }

    return NULL;
#endif
}

void CKDirectoryParser::Reset(const char *dir, const char *fileMask, XBOOL recurse) {
#if defined(_WIN32)
    // Clean up existing resources
    Clean();

    // Preserve old values if new ones are NULL
    char* oldStartDir = m_StartDir;
    char* oldFileMask = m_FileMask;

    // Always clean up the full filename buffer
    delete[] m_FullFileName;

    // Allocate new resources
    m_FindData = new _finddata_t;
    m_FullFileName = DuplicateCString("");

    // Set directory
    if (dir) {
        delete[] oldStartDir; // Clean up old directory
        m_StartDir = DuplicateDirectoryName(dir);
    } else {
        // Keep the old directory if NULL is passed
        m_StartDir = oldStartDir;
    }

    // Set file mask
    if (fileMask) {
        delete[] oldFileMask; // Clean up old mask
        m_FileMask = DuplicateCString(fileMask);
    } else {
        // Keep the old file mask if NULL is passed
        m_FileMask = oldFileMask;
    }

    // Initialize state
    m_State = recurse ? 1 : 0;
    m_hFile = -1;
    m_SubParser = NULL;
#else
    Clean();

    char *oldStartDir = m_StartDir;
    char *oldFileMask = m_FileMask;

    delete[] m_FullFileName;

    m_FullFileName = DuplicateCString("");
    m_FindData = NULL;

    if (dir) {
        delete[] oldStartDir;
        m_StartDir = DuplicateDirectoryName(dir);
    } else {
        m_StartDir = oldStartDir;
    }

    if (fileMask) {
        delete[] oldFileMask;
        m_FileMask = DuplicateCString(fileMask);
    } else {
        m_FileMask = oldFileMask;
    }

    m_State = recurse ? 1 : 0;
    m_hFile = 0;
    m_SubParser = NULL;
#endif
}

void CKDirectoryParser::Clean() {
#if defined(_WIN32)
    if (m_hFile != -1) {
        _findclose(m_hFile);
        m_hFile = -1;
    }

    if (m_SubParser) {
        delete m_SubParser;
        m_SubParser = NULL;
    }

    if (m_FindData) {
        delete (_finddata_t*)m_FindData;
        m_FindData = NULL;
    }
#else
    if (m_hFile) {
        closedir(reinterpret_cast<DIR *>(m_hFile));
        m_hFile = 0;
    }

    if (m_SubParser) {
        delete m_SubParser;
        m_SubParser = NULL;
    }

    m_FindData = NULL;
#endif
}
