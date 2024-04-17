#ifndef XHASHFUN_H
#define XHASHFUN_H

#include "VxMathDefines.h"
#include "XString.h"

#include <ctype.h>
#include <stdlib.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4996)
#endif

/************************************************
Summary: A series of comparison function

Remarks


************************************************/
template <class K>
struct XEqual
{
    int operator()(const K &iK1, const K &iK2) const
    {
        return (iK1 == iK2);
    }
};

template <>
struct XEqual<const char *>
{
    int operator()(const char *iS1, const char *iS2) const { return !strcmp(iS1, iS2); }
};

template <>
struct XEqual<char *>
{
    int operator()(const char *iS1, const char *iS2) const { return !strcmp(iS1, iS2); }
};

struct XEqualXStringI
{
    int operator()(const XString &iS1, const XString &iS2) const { return !iS1.ICompare(iS2); }
};

struct XEqualStringI
{
    int operator()(const char *iS1, const char *iS2) const { return !stricmp(iS1, iS2); }
};

/************************************************
Summary: A series of hash functions

Remarks
These hash functions are designed to be used when declaring a hash table where the
key is one of the following type :

    o XHashFunString : hash function for char* key
    o XHashFunXString : hash function for XString key
    o XHashFunChar : hash function for char key
    o XHashFunByte : hash function for BYTE key
    o XHashFunWord : hash function for WORD key
    o XHashFunDword : hash function for DWORD key
    o XHashFunInt : hash function for int key
    o XHashFunFloat : hash function for float key
    o XHashFunPtr : hash function for void* key


************************************************/
template <class K>
struct XHashFun
{
    int operator()(const K &iK)
    {
        return (int)iK;
    }
};

// NB hashing should return unsigned int
// and be multiplied by 2654435761 (Knuth method : golden ratio of 2^32)

inline int XHashString(const char *_s)
{
    unsigned int _h = 0;
    for (; *_s; ++_s)
        _h = 5 * _h + *_s;

    return int(_h);
}

inline int XHashStringI(const char *_s)
{
    unsigned int _h = 0;

    for (; *_s; ++_s)
    {
        // GG : ANSI Compliant
        _h = 5 * _h + tolower(*_s);
    }

    return int(_h);
}

template <>
struct XHashFun<char *>
{
    int operator()(const char *_s) const { return XHashString(_s); }
};

template <>
struct XHashFun<const char *>
{
    int operator()(const char *_s) const { return XHashString(_s); }
};

template <>
struct XHashFun<XString>
{
    int operator()(const XString &_s) const { return XHashString(_s.CStr()); }
};

template <>
struct XHashFun<float>
{
    int operator()(const float _x) const { return *(int *)&_x; }
};

template <>
struct XHashFun<void *>
{
    int operator()(const void *_x) const { return *(int*)_x >> 8; }
};

template <>
struct XHashFun<XGUID>
{
    int operator()(const XGUID &_s) const
    {
        return _s.d1;
    }
};

struct XHashFunStringI
{
    int operator()(const char *_s) const { return XHashStringI(_s); }
};

struct XHashFunXStringI
{
    int operator()(const XString &_s) const { return XHashStringI(_s.CStr()); }
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif // XHASHFUN_H
