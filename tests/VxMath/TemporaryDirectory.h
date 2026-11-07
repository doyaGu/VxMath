#ifndef TEMPORARYDIRECTORY_H
#define TEMPORARYDIRECTORY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

// Owns one uniquely reserved directory; callers remove its contents before destruction.
class TemporaryDirectory {
public:
    TemporaryDirectory() {
#if defined(_WIN32)
        char temp[512];
        const DWORD length = GetTempPathA(sizeof(temp), temp);
        if (!length || length >= sizeof(temp) || !GetTempFileNameA(temp, "vxt", 0, m_reservation)) {
            m_reservation[0] = '\0';
            return;
        }
        char directory[sizeof(m_path)];
        const int size = snprintf(directory, sizeof(directory), "%s.dir", m_reservation);
        if (size < 0 || size >= int(sizeof(directory)) || !CreateDirectoryA(directory, nullptr))
            return;
        strcpy(m_path, directory);
#else
        const char *temp = getenv("TMPDIR");
        if (!temp || !*temp)
            temp = "/tmp";
        const int size = snprintf(m_path, sizeof(m_path), "%s/vx-test-XXXXXX", temp);
        if (size < 0 || size >= int(sizeof(m_path)) || !mkdtemp(m_path))
            m_path[0] = '\0';
#endif
    }

    ~TemporaryDirectory() {
#if defined(_WIN32)
        if (*m_path)
            RemoveDirectoryA(m_path);
        // Keep this file occupied until directory cleanup is complete.
        if (*m_reservation)
            DeleteFileA(m_reservation);
#else
        if (*m_path)
            rmdir(m_path);
#endif
    }

    TemporaryDirectory(const TemporaryDirectory &) = delete;
    TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;

    const char *Path() const { return m_path; }
    bool IsValid() const { return *m_path != '\0'; }

private:
    char m_path[512] = {};
#if defined(_WIN32)
    char m_reservation[512] = {};
#endif
};

#endif // TEMPORARYDIRECTORY_H
