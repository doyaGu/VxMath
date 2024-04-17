#include "XString.h"

#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

// Substring Ctor
XString::XString(const char *iString, const int iLength) : XBaseString()
{
    if (!iString || *iString == '\0')
    {
        m_Buffer = NULL;
        m_Length = 0;
        return;
    }

    m_Length = (iLength > 0) ? iLength : strlen(iString);
    m_Allocated = m_Length + 1;
    m_Buffer = new char[m_Allocated];
    strncpy(m_Buffer, iString, m_Length);
}

// Reserving Ctor
XString::XString(const int iLength) : XBaseString()
{
    if (iLength == 0)
    {
        m_Buffer = NULL;
        m_Length = 0;
        return;
    }

    m_Length = iLength - 1;
    m_Allocated = iLength;
    m_Buffer = new char[iLength];
    memset(m_Buffer, 0, m_Allocated);
}

// Copy Ctor
XString::XString(const XString &iSrc) : XBaseString()
{
    if (iSrc.m_Length == 0)
    {
        m_Buffer = NULL;
        m_Length = 0;
        return;
    }

    m_Length = iSrc.m_Length;
    m_Allocated = iSrc.m_Length + 1;
    m_Buffer = new char[m_Allocated];
    memcpy(m_Buffer, iSrc.m_Buffer, m_Allocated);
}

// Copy Ctor
XString::XString(const XBaseString &iSrc) : XBaseString()
{
    if (iSrc.m_Length == 0)
    {
        m_Buffer = NULL;
        m_Length = 0;
        return;
    }

    m_Length = iSrc.m_Length;
    m_Allocated = iSrc.m_Length + 1;
    m_Buffer = new char[m_Allocated];
    memcpy(m_Buffer, iSrc.m_Buffer, m_Allocated);
}

// Dtor
XString::~XString()
{
    delete[] m_Buffer;
}

// operator =
XString &XString::operator=(const XString &iSrc)
{
    if (this != &iSrc)
        Assign(iSrc.m_Buffer, iSrc.m_Length);
    return *this;
}

// operator =
XString &XString::operator=(const char *iSrc)
{
    if (iSrc)
    {
        Assign(iSrc, strlen(iSrc));
    }
    else
    {
        m_Length = 0;
        if (m_Buffer)
            m_Buffer[0] = '\0';
    }
    return *this;
}

// operator =
XString &XString::operator=(const XBaseString &iSrc)
{
    if (this != &iSrc)
    {
        Assign(iSrc.m_Buffer, iSrc.m_Length);
    }
    return *this;
}

// Create from a string and a length
XString &XString::Create(const char *iString, const int iLength)
{
    CheckSize(iLength);
    m_Length = 0;
    if (iString)
    {
        m_Length = iLength;
        strncpy(m_Buffer, iString, iLength);
    }
    return *this;
}

// Format the string sprintf style, ol skool ! yeahhhhh
XString &XString::Format(const char *iFormat, ...)
{
    va_list args;
    va_start(args, iFormat);

    char buf[512];
    int len = vsprintf(buf, iFormat, args);
    Assign(buf, len);

    va_end(args);
    return *this;
}

// Capitalize all the characters of the string
XString &XString::ToUpper()
{
    for (XWORD i = 0; i < m_Length; ++i)
        m_Buffer[i] = toupper(m_Buffer[i]);
    return *this;
}

// Uncapitalize all the characters of the string
XString &XString::ToLower()
{
    for (XWORD i = 0; i < m_Length; ++i)
        m_Buffer[i] = tolower(m_Buffer[i]);
    return *this;
}

// Compare the strings (ignore the case).
int XString::ICompare(const XBaseString &iStr) const
{
    if (!m_Length)
        return -iStr.m_Length; // Null strings
    if (!iStr.m_Length)
        return m_Length;

    char *s1 = m_Buffer;
    char *s2 = iStr.m_Buffer;
    char c1, c2;
    do
    {
        c1 = tolower(*(s1++));
        c2 = tolower(*(s2++));
    } while (*s1 != '\0' && (c1 == c2));

    return tolower(*s1) - tolower(*s2);
}

///
// Complex operations on strings

// removes space ( ' ', '\t' and '\n') from the start and the end of the string
XString &XString::Trim()
{
    if (m_Length != 0)
    {
        while (isspace(m_Buffer[m_Length - 1]) && m_Length != 0)
            --m_Length;
        m_Buffer[m_Length] = '\0';

        if (m_Length != 0)
        {
            XWORD i = 0;
            while (isspace(m_Buffer[i]) && i != m_Length)
                ++i;
            if (i != 0)
            {
                m_Length -= i;
                strncpy(m_Buffer, &m_Buffer[i], m_Length + 1);
            }
        }
    }
    return *this;
}

// replaces space ( ' ', '\t' and '\n') sequences by one space
XString &XString::Strip()
{
    if (m_Length != 0)
    {
        XWORD i;
        XBOOL wasSpace = FALSE;
        for (i = 0; i < m_Length; i++)
        {
            if (isspace(m_Buffer[i]))
            {
                if (wasSpace)
                    break;
                wasSpace = TRUE;
            }
            else
            {
                wasSpace = FALSE;
            }
        }

        for (XWORD j = i + 1; j < m_Length; j++)
        {
            if (isspace(m_Buffer[j]))
            {
                if (!wasSpace)
                {
                    wasSpace = TRUE;
                    m_Buffer[i++] = ' ';
                }
            }
            else
            {
                wasSpace = FALSE;
                m_Buffer[i++] = m_Buffer[j];
            }
        }

        m_Length = i;
        m_Buffer[i] = '\0';
    }
    return *this;
}

// finds a character in the string
XWORD XString::Find(char iCar, XWORD iStart) const
{
    if (m_Length != 0)
    {
        char *str = strchr(&m_Buffer[iStart], iCar);
        if (str)
            return str - m_Buffer;
        else
            return -1;
    }
    return -1;
}

// finds a string in the string
XWORD XString::Find(const XBaseString &iStr, XWORD iStart) const
{
    if (m_Length != 0 && iStr.m_Length != 0)
    {
        char *str = strstr(&m_Buffer[iStart], iStr.m_Buffer);
        if (str)
            return str - m_Buffer;
        else
            return -1;
    }
    return -1;
}

// finds a string in the string
//XWORD XString::IFind(const XBaseString &iStr, XWORD iStart) const
//{
//    if (m_Length == 0 || iStr.m_Length == 0)
//        return -1;
//
//    char *first = strdup(m_Buffer);
//    first = strlwr(first);
//    char *last = iStr.m_Buffer;
//    if (!iStr.m_Buffer)
//        last = NULL;
//    last = strdup(last);
//    last = strlwr(last);
//    char *str = strstr(&first[iStart], last);
//    XWORD ret;
//    if (str)
//        ret = str - first;
//    else
//        ret = -1;
//
//    free(first);
//    free(last);
//    return ret;
//}

// finds a character in the string
XWORD XString::RFind(char iCar, XWORD iStart) const
{
    if (m_Length != 0)
    {
        XWORD i = (iStart != NOTFOUND) ? iStart : m_Length;

        char ch = m_Buffer[i];
        m_Buffer[i] = '\0';
        char *buf = strrchr(&m_Buffer[iStart], iCar);
        m_Buffer[i] = ch;
        if (buf)
            return buf - m_Buffer;
        else
            return -1;
    }
    return -1;
}

// creates a substring
XString XString::Substring(XWORD iStart, XWORD iLength) const
{
    return XString(&m_Buffer[iStart], (iLength != 0) ? iLength : m_Length - iStart);
}

// crops the string
XString &XString::Crop(XWORD iStart, XWORD iLength)
{
    if (iStart > 0)
        strncpy(m_Buffer, &m_Buffer[iStart], iLength);
    m_Length = iLength;
    m_Buffer[iLength] = '\0';
    return *this;
}

// cuts the string
XString &XString::Cut(XWORD iStart, XWORD iLength)
{
    strncpy(&m_Buffer[iStart], &m_Buffer[iLength + iStart], m_Length - (iLength + iStart) + 1);
    m_Length -= iLength;
    return *this;
}

// replaces all the occurrence of a character by another one
int XString::Replace(char iSrc, char iDest)
{
    int count = 0;
    for (int i = 0; i < m_Length; ++i)
    {
        if (m_Buffer[i] == iSrc)
        {
            m_Buffer[i] = iDest;
            ++count;
        }
    }
    return count;
}

// replaces all the occurrence of a string by another string
int XString::Replace(const XBaseString &iSrc, const XBaseString &iDest)
{
    int count = 0;
    char *src = NULL;

    if (iSrc.m_Length == iDest.m_Length)
    {
        src = strstr(m_Buffer, iSrc.m_Buffer);
        if (!src)
            return 0;

        do
        {
            strncpy(src, iDest.m_Buffer, iDest.m_Length);
            ++count;
        } while ((src = strstr(&src[iSrc.m_Length], iSrc.m_Buffer)));
        return count;
    }
    else
    {
        src = strstr(m_Buffer, iSrc.m_Buffer);
        if (src)
        {
            do
            {
                ++count;
            } while ((src = strstr(&src[iSrc.m_Length], iSrc.m_Buffer)));
        }

        XWORD len = m_Length + count * (iDest.m_Length - iSrc.m_Length);
        char *buf = (char *)VxNew(sizeof(char) * (len + 1));

        char *s1 = m_Buffer;
        char *s2 = buf;

        src = strstr(m_Buffer, iSrc.m_Buffer);
        if (src)
        {
            do
            {
                strncpy(s2, s1, src - s1);
                strncpy(&s2[src - s1], iDest.m_Buffer, iDest.m_Length);
                s2 = &s2[iDest.m_Length];
                s1 = &src[iSrc.m_Length];
            } while ((src = strstr(s1, iSrc.m_Buffer)));
        }
        strncpy(s2, s1, (m_Buffer - s1) + 1);

        VxDelete(m_Buffer);
        m_Length = len;
        m_Buffer = buf;
        m_Allocated = len + 1;
        return count;
    }
}

///
// Stream operators

// Concatenation operator
XString &XString::operator<<(const char *iString)
{
    if (iString)
    {
        Assign(iString, strlen(iString));
    }
    else
    {
        m_Length = 0;
        if (m_Buffer)
            m_Buffer[0] = '\0';
    }
    return *this;
}

// Concatenation operator
XString &XString::operator<<(const XBaseString &iString)
{
    CheckSize(m_Length + iString.m_Length);
    if (iString.m_Length != 0)
    {
        strncpy(&m_Buffer[m_Length], iString.m_Buffer, iString.m_Length + 1);
        m_Length += iString.m_Length;
    }
    return *this;
}

// Concatenation operator
XString &XString::operator<<(const char iValue)
{
    CheckSize(m_Length + 1);
    m_Buffer[m_Length++] = iValue;
    m_Buffer[m_Length] = '\0';
    return *this;
}

// Concatenation operator
XString &XString::operator<<(const int iValue)
{
    char buf[16];
    int len = sprintf(buf, "%d", iValue);
    CheckSize(m_Length + len);
    strncpy(&m_Buffer[m_Length], buf, len + 1);
    m_Length += len;
    return *this;
}

// Concatenation operator
XString &XString::operator<<(const unsigned int iValue)
{
    char buf[16];
    int len = sprintf(buf, "%u", iValue);
    CheckSize(m_Length + len);
    strncpy(&m_Buffer[m_Length], buf, len + 1);
    m_Length += len;
    return *this;
}

// Concatenation operator
XString &XString::operator<<(const float iValue)
{
    char buf[16];
    int len = sprintf(buf, "%f", iValue);
    CheckSize(m_Length + len);
    strncpy(&m_Buffer[m_Length], buf, len + 1);
    m_Length += len;
    return *this;
}

// Concatenation operator
XString &XString::operator<<(const void *iValue)
{
    if (iValue)
    {
        XWORD len = (XWORD)strlen((char *)iValue);
        if (len != 0)
        {
            CheckSize(m_Length + len);
            strncpy(&m_Buffer[m_Length], (const char *)iValue, len + 1);
            m_Length += len;
        }
    }
    return *this;
}

///
// Capacity functions

// Sets the capacity of the string
void XString::Reserve(XWORD iLength)
{
    if (iLength + 1 > m_Allocated)
    {
        m_Allocated = iLength + 1;
        char *buf = new char[iLength + 1];
        if (m_Length != 0)
            strncpy(buf, m_Buffer, m_Length);
        delete[] m_Buffer;
        m_Buffer = buf;
    }
}

// Check a string size to see if it fits in
void XString::CheckSize(int iLength)
{
    if (iLength + 1 > m_Allocated)
    {
        m_Allocated = iLength + 1;
        char *buf = new char[iLength + 1];
        if (m_Length != 0)
            strncpy(buf, m_Buffer, m_Length + 1);
        else
            buf[0] = '\0';
        delete[] m_Buffer;
        m_Buffer = buf;
    }
}

// Assign a string to the current string
void XString::Assign(const char *iBuffer, int iLength)
{
    m_Length = 0;
    if (iLength != 0)
    {
        CheckSize(iLength);
        m_Length = iLength;
        strncpy(m_Buffer, iBuffer, iLength);
    }
    else if (m_Buffer)
    {
        m_Buffer[0] = '\0';
    }
}