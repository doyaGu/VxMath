#include "XString.h"

#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// Substring Ctor
XString::XString(const char *iString, int iLength) : XBaseString() {
    if (!iString || *iString == '\0') {
        m_Buffer = NULL;
        m_Length = 0;
        m_Allocated = 0;
        return;
    }

    const int length = (iLength > 0) ? ClampLength(iLength) : ClampLength(strlen(iString));
    if (length == 0) {
        m_Buffer = NULL;
        m_Length = 0;
        m_Allocated = 0;
        return;
    }

    m_Length = (XWORD) length;
    m_Allocated = (XWORD) (length + 1);
    m_Buffer = new char[m_Allocated];
    strncpy(m_Buffer, iString, m_Length);
    m_Buffer[m_Length] = '\0';
}

// Reserving Ctor
XString::XString(int iLength) : XBaseString() {
    const int reserved = ClampLength(iLength);
    if (reserved <= 0) {
        m_Buffer = NULL;
        m_Length = 0;
        m_Allocated = 0;
        return;
    }

    m_Length = 0;
    m_Allocated = (XWORD) (reserved + 1);
    m_Buffer = new char[m_Allocated];
    memset(m_Buffer, 0, m_Allocated);
}

// Copy Ctor
XString::XString(const XString &iSrc) : XBaseString() {
    Assign(iSrc.m_Buffer, iSrc.m_Length);
}

// Copy Ctor from XBaseString
XString::XString(const XBaseString &iSrc) : XBaseString() {
    Assign(iSrc.m_Buffer, iSrc.m_Length);
}

// Dtor
XString::~XString() {
    delete[] m_Buffer;
}

// operator =
XString &XString::operator=(const XString &iSrc) {
    if (this != &iSrc)
        Assign(iSrc.m_Buffer, iSrc.m_Length);
    return *this;
}

// operator =
XString &XString::operator=(const char *iSrc) {
    if (iSrc) {
        Assign(iSrc, (int)strlen(iSrc));
    } else {
        m_Length = 0;
        if (m_Buffer)
            m_Buffer[0] = '\0';
    }
    return *this;
}

// operator =
XString &XString::operator=(const XBaseString &iSrc) {
    if (this != &iSrc) {
        Assign(iSrc.m_Buffer, iSrc.m_Length);
    }
    return *this;
}

// Create from a string and a length
XString &XString::Create(const char *iString, int iLength) {
    if (iString) {
        const int length = (iLength > 0) ? ClampLength(iLength) : ClampLength(strlen(iString));
        if (length > 0) {
            CheckSize(length);
            m_Length = (XWORD) length;
            strncpy(m_Buffer, iString, m_Length);
            m_Buffer[m_Length] = '\0';
        } else {
            m_Length = 0;
            if (m_Buffer)
                m_Buffer[0] = '\0';
        }
    } else {
        m_Length = 0;
        if (m_Buffer)
            m_Buffer[0] = '\0';
    }
    return *this;
}

// Format the string sprintf style
XString &XString::Format(const char *iFormat, ...) {
    if (!iFormat) {
        m_Length = 0;
        if (m_Buffer)
            m_Buffer[0] = '\0';
        return *this;
    }

    va_list args;
    va_start(args, iFormat);

    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, iFormat, args_copy);
    va_end(args_copy);

    if (len > 0) {
        const int writeLength = ClampLength(len);
        CheckSize(writeLength);
        vsnprintf(m_Buffer, (size_t) writeLength + 1, iFormat, args);
        m_Length = (XWORD) writeLength;
        m_Buffer[m_Length] = '\0';
    } else {
        m_Length = 0;
        if (m_Buffer)
            m_Buffer[0] = '\0';
    }

    va_end(args);
    return *this;
}

// Capitalize all the characters of the string
XString &XString::ToUpper() {
    for (XWORD i = 0; i < m_Length; ++i)
        m_Buffer[i] = (char) toupper((unsigned char) m_Buffer[i]);
    return *this;
}

// Uncapitalize all the characters of the string
XString &XString::ToLower() {
    for (XWORD i = 0; i < m_Length; ++i)
        m_Buffer[i] = (char) tolower((unsigned char) m_Buffer[i]);
    return *this;
}

// Compare the strings (ignore the case).
int XString::ICompare(const XBaseString &iStr) const {
    if (!m_Length)
        return -iStr.m_Length;
    if (!iStr.m_Length)
        return m_Length;

    const char *s1 = m_Buffer;
    const char *s2 = iStr.m_Buffer;
    char c1, c2;

    do {
        c1 = (char) tolower((unsigned char) *s1);
        c2 = (char) tolower((unsigned char) *s2);
        if (c1 != c2)
            return c1 - c2;
        s1++;
        s2++;
    } while (c1 != '\0' && c2 != '\0');

    return c1 - c2;
}

// Remove whitespace from start and end
XString &XString::Trim() {
    if (m_Length == 0)
        return *this;

    // Trim end
    while (m_Length > 0 && isspace((unsigned char) m_Buffer[m_Length - 1])) {
        --m_Length;
    }
    m_Buffer[m_Length] = '\0';

    // Trim start
    if (m_Length > 0) {
        XWORD start = 0;
        while (start < m_Length && isspace((unsigned char) m_Buffer[start])) {
            ++start;
        }
        if (start > 0) {
            m_Length -= start;
            memmove(m_Buffer, &m_Buffer[start], m_Length + 1);
        }
    }
    return *this;
}

XString &XString::Strip() {
    if (m_Length == 0)
        return *this;

    XWORD writePos = 0;
    XBOOL lastWasSpace = FALSE;

    for (XWORD readPos = 0; readPos < m_Length; ++readPos) {
        if (isspace((unsigned char) m_Buffer[readPos])) {
            if (!lastWasSpace) {
                m_Buffer[writePos++] = ' ';
                lastWasSpace = TRUE;
            }
        } else {
            m_Buffer[writePos++] = m_Buffer[readPos];
            lastWasSpace = FALSE;
        }
    }

    m_Length = writePos;
    m_Buffer[m_Length] = '\0';
    return *this;
}

// Find a character in the string
XWORD XString::Find(char iCar, XWORD iStart) const {
    if (m_Length == 0 || iStart >= m_Length)
        return NOTFOUND;

    char *str = strchr(&m_Buffer[iStart], iCar);
    if (str)
        return (XWORD)(str - m_Buffer);
    else
        return NOTFOUND;
}

// Find a string in the string
XWORD XString::Find(const XBaseString &iStr, XWORD iStart) const {
    if (m_Length == 0 || iStr.m_Length == 0 || iStart >= m_Length)
        return NOTFOUND;

    char *str = strstr(&m_Buffer[iStart], iStr.m_Buffer);
    if (str)
        return (XWORD)(str - m_Buffer);
    else
        return NOTFOUND;
}

XWORD XString::IFind(const XBaseString &iStr, XWORD iStart) const {
    if (m_Length == 0 || iStr.m_Length == 0 || iStart >= m_Length)
        return NOTFOUND;

    // No possible match if the needle is longer than the remaining haystack.
    if (iStr.m_Length > m_Length || iStart > (m_Length - iStr.m_Length))
        return NOTFOUND;

    // Manual case-insensitive search.
    for (XWORD i = iStart; i <= (m_Length - iStr.m_Length); ++i) {
        XBOOL match = TRUE;
        for (XWORD j = 0; j < iStr.m_Length; ++j) {
            if (tolower((unsigned char) m_Buffer[i + j]) != tolower((unsigned char) iStr.m_Buffer[j])) {
                match = FALSE;
                break;
            }
        }
        if (match)
            return i;
    }
    return NOTFOUND;
}

XWORD XString::RFind(char iCar, XWORD iStart) const {
    if (m_Length == 0)
        return NOTFOUND;

    XWORD start = (iStart == NOTFOUND) ? m_Length - 1 : iStart;
    if (start >= m_Length)
        start = m_Length - 1;

    for (XWORD i = start + 1; i > 0; --i) {
        if (m_Buffer[i - 1] == iCar)
            return i - 1;
    }
    return NOTFOUND;
}

// Create a substring
XString XString::Substring(XWORD iStart, XWORD iLength) const {
    if (iStart >= m_Length)
        return XString();

    XWORD actualLength = (iLength == 0) ? m_Length - iStart : iLength;
    if (iStart + actualLength > m_Length)
        actualLength = m_Length - iStart;

    return XString(&m_Buffer[iStart], actualLength);
}

// Crop the string
XString &XString::Crop(XWORD iStart, XWORD iLength) {
    if (iStart >= m_Length) {
        m_Length = 0;
        if (m_Buffer)
            m_Buffer[0] = '\0';
        return *this;
    }

    if (iStart + iLength > m_Length)
        iLength = m_Length - iStart;

    if (iStart > 0)
        memmove(m_Buffer, &m_Buffer[iStart], iLength);

    m_Length = iLength;
    m_Buffer[m_Length] = '\0';
    return *this;
}

// Cut part of the string
XString &XString::Cut(XWORD iStart, XWORD iLength) {
    if (iStart >= m_Length)
        return *this;

    if (iStart + iLength > m_Length)
        iLength = m_Length - iStart;

    memmove(&m_Buffer[iStart], &m_Buffer[iStart + iLength], m_Length - (iStart + iLength) + 1);
    m_Length -= iLength;
    return *this;
}

// Replace all occurrences of a character
int XString::Replace(char iSrc, char iDest) {
    int count = 0;
    for (XWORD i = 0; i < m_Length; ++i) {
        if (m_Buffer[i] == iSrc) {
            m_Buffer[i] = iDest;
            ++count;
        }
    }
    return count;
}

int XString::Replace(const XBaseString &iSrc, const XBaseString &iDest) {
    if (iSrc.m_Length == 0 || m_Length == 0)
        return 0;

    int count = 0;
    XWORD pos = 0;

    // Count occurrences first
    while ((pos = Find(iSrc, pos)) != NOTFOUND) {
        count++;
        pos += iSrc.m_Length;
    }

    if (count == 0)
        return 0;

    // Calculate new length
    const size_t maxLength = (size_t) MAX_LENGTH;
    const size_t srcLength = (size_t) m_Length;
    const size_t srcTokenLength = (size_t) iSrc.m_Length;
    const size_t destTokenLength = (size_t) iDest.m_Length;

    size_t newLength = srcLength;
    if (destTokenLength >= srcTokenLength) {
        const size_t growthPerMatch = destTokenLength - srcTokenLength;
        const size_t maxGrowth = maxLength - newLength;
        if (growthPerMatch != 0 &&
            (size_t) count > (maxGrowth / growthPerMatch)) {
            return 0;
        }
        newLength += (size_t) count * growthPerMatch;
    } else {
        newLength -= (size_t) count * (srcTokenLength - destTokenLength);
    }

    if (iSrc.m_Length == iDest.m_Length) {
        // Simple case: same length replacement
        pos = 0;
        while ((pos = Find(iSrc, pos)) != NOTFOUND) {
            memcpy(&m_Buffer[pos], iDest.m_Buffer, iDest.m_Length);
            pos += iSrc.m_Length;
        }
    } else {
        // Complex case: different length replacement
        char *newBuffer = new char[newLength + 1];
        size_t srcPos = 0, destPos = 0;

        pos = 0;
        while ((pos = Find(iSrc, (XWORD) srcPos)) != NOTFOUND) {
            // Copy text before match
            const size_t copyLen = (size_t) pos - srcPos;
            if (copyLen > 0) {
                memcpy(&newBuffer[destPos], &m_Buffer[srcPos], copyLen);
                destPos += copyLen;
            }

            // Copy replacement text
            if (iDest.m_Length > 0) {
                memcpy(&newBuffer[destPos], iDest.m_Buffer, iDest.m_Length);
                destPos += (size_t) iDest.m_Length;
            }

            srcPos = (size_t) pos + (size_t) iSrc.m_Length;
        }

        // Copy remaining text
        if (srcPos < (size_t) m_Length) {
            const size_t remaining = (size_t) m_Length - srcPos;
            memcpy(&newBuffer[destPos], &m_Buffer[srcPos], remaining);
            destPos += remaining;
        }

        newBuffer[newLength] = '\0';

        delete[] m_Buffer;
        m_Buffer = newBuffer;
        m_Length = (XWORD) newLength;
        m_Allocated = (XWORD) (newLength + 1);
    }

    return count;
}

XString &XString::operator<<(const char *iString) {
    if (!iString || *iString == '\0')
        return *this;

    const int remaining = GetRemainingLength();
    if (remaining <= 0)
        return *this;

    const int toCopy = ClampLength(strlen(iString));
    const int clippedToCopy = (toCopy < remaining) ? toCopy : remaining;
    if (clippedToCopy <= 0)
        return *this;

    if (IsPointerInBuffer(iString)) {
        XString temp(iString, clippedToCopy);
        return (*this) << static_cast<const XBaseString &>(temp);
    }

    CheckSize(m_Length + clippedToCopy);
    memcpy(&m_Buffer[m_Length], iString, (size_t) clippedToCopy);
    m_Length = (XWORD) (m_Length + clippedToCopy);
    m_Buffer[m_Length] = '\0';
    return *this;
}

// Concatenation operator
XString &XString::operator<<(const XBaseString &iString) {
    if (iString.m_Length == 0 || !iString.m_Buffer)
        return *this;

    const int remaining = GetRemainingLength();
    if (remaining <= 0)
        return *this;

    const int toCopy = (iString.m_Length < remaining) ? iString.m_Length : remaining;
    if (toCopy <= 0)
        return *this;

    if (&iString == this || IsPointerInBuffer(iString.m_Buffer)) {
        XString temp(iString.m_Buffer, toCopy);
        return (*this) << static_cast<const XBaseString &>(temp);
    }

    CheckSize(m_Length + toCopy);
    memcpy(&m_Buffer[m_Length], iString.m_Buffer, (size_t) toCopy);
    m_Length = (XWORD) (m_Length + toCopy);
    m_Buffer[m_Length] = '\0';
    return *this;
}

// Concatenation operator
XString &XString::operator<<(char iValue) {
    CheckSize(m_Length + 1);
    m_Buffer[m_Length++] = iValue;
    m_Buffer[m_Length] = '\0';
    return *this;
}

// Concatenation operator
XString &XString::operator<<(int iValue) {
    char buf[32];  // Increased buffer size for safety
    if (snprintf(buf, sizeof(buf), "%d", iValue) > 0)
        (*this) << buf;
    return *this;
}

// Concatenation operator
XString &XString::operator<<(unsigned int iValue) {
    char buf[32];  // Increased buffer size for safety
    if (snprintf(buf, sizeof(buf), "%u", iValue) > 0)
        (*this) << buf;
    return *this;
}

// Concatenation operator
XString &XString::operator<<(float iValue) {
    char buf[32];  // Increased buffer size for safety
    if (snprintf(buf, sizeof(buf), "%f", iValue) > 0)  // Use %g for cleaner float output
        (*this) << buf;
    return *this;
}

XString &XString::operator<<(double iValue) {
    char buf[64];
    if (snprintf(buf, sizeof(buf), "%.15g", iValue) > 0)
        (*this) << buf;
    return *this;
}

XString &XString::operator<<(bool iValue) {
    return (*this) << (iValue ? "true" : "false");
}

XString &XString::operator<<(const void *iValue) {
    char buf[32];
    if (snprintf(buf, sizeof(buf), "%p", iValue) > 0)
        (*this) << buf;
    return *this;
}

void XString::Reserve(XWORD iLength) {
    const XWORD requested = (iLength > MAX_LENGTH) ? (XWORD) MAX_LENGTH : iLength;
    const XWORD required = (XWORD) (requested + 1);

    if (required > m_Allocated) {
        m_Allocated = required;
        char *buf = new char[m_Allocated];
        if (m_Length > 0) {
            memcpy(buf, m_Buffer, m_Length + 1);
        } else {
            buf[0] = '\0';
        }
        delete[] m_Buffer;
        m_Buffer = buf;
    }
}

void XString::CheckSize(int iLength) {
    const int length = ClampLength(iLength);
    const int requiredCapacity = length + 1;

    if (requiredCapacity > m_Allocated) {
        // Grow buffer with some extra space to reduce reallocations
        int newCapacity = (m_Allocated > 0) ? m_Allocated : 1;
        const int maxCapacity = (int) MAX_LENGTH + 1;

        while (newCapacity < requiredCapacity && newCapacity < maxCapacity) {
            const int doubled = newCapacity * 2;
            if (doubled <= newCapacity || doubled > maxCapacity)
                newCapacity = maxCapacity;
            else
                newCapacity = doubled;
        }

        if (newCapacity < requiredCapacity)
            newCapacity = requiredCapacity;

        m_Allocated = (XWORD) newCapacity;
        char *buf = new char[m_Allocated];
        if (m_Length > 0) {
            memcpy(buf, m_Buffer, m_Length + 1);
        } else {
            buf[0] = '\0';
        }
        delete[] m_Buffer;
        m_Buffer = buf;
    }
}

// Assign a string to the current string
void XString::Assign(const char *iBuffer, int iLength) {
    const int length = ClampLength(iLength);
    if (length > 0 && iBuffer) {
        const char *source = iBuffer;
        char *tempCopy = NULL;

        if (IsPointerInBuffer(iBuffer)) {
            tempCopy = new char[length];
            memcpy(tempCopy, iBuffer, (size_t) length);
            source = tempCopy;
        }

        CheckSize(length);
        memcpy(m_Buffer, source, (size_t) length);
        m_Length = (XWORD) length;
        m_Buffer[m_Length] = '\0';

        delete[] tempCopy;
    } else {
        m_Length = 0;
        if (m_Buffer) {
            m_Buffer[0] = '\0';
        }
    }
}

int XString::ClampLength(int iLength) const {
    if (iLength <= 0)
        return 0;
    if (iLength > (int) MAX_LENGTH)
        return (int) MAX_LENGTH;
    return iLength;
}

int XString::ClampLength(size_t iLength) const {
    const size_t maxLength = (size_t) MAX_LENGTH;
    return (int) ((iLength > maxLength) ? maxLength : iLength);
}

int XString::GetRemainingLength() const {
    if (m_Length >= MAX_LENGTH)
        return 0;
    return (int) (MAX_LENGTH - m_Length);
}

XBOOL XString::IsPointerInBuffer(const char *iPtr) const {
    if (!iPtr || !m_Buffer || m_Allocated == 0)
        return FALSE;

    const XUINTPTR p = (XUINTPTR) iPtr;
    const XUINTPTR begin = (XUINTPTR) m_Buffer;
    const XUINTPTR end = begin + (XUINTPTR) m_Allocated;
    return p >= begin && p < end;
}
