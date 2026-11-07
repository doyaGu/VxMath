#ifndef NATIVETEXTREFERENCE_H
#define NATIVETEXTREFERENCE_H

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

static inline bool NativeTextEncode(const wchar_t *text, char *output, int size) {
    const UINT page = GetACP();
    BOOL substituted = FALSE;
    return WideCharToMultiByte(page, page == CP_UTF8 ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS,
        text, -1, output, size, nullptr, page == CP_UTF8 ? nullptr : &substituted) > 0 && !substituted;
}

// Select real input representable by this process, without substituting '?' bytes.
static inline bool NativeTextCharacter(char *output, int size, bool preferTrailBackslash = false) {
    if (preferTrailBackslash && size >= 3) {
        for (int lead = 0x81; lead <= 0xfe; ++lead) {
            const char bytes[] = {static_cast<char>(lead), '\\'};
            const char alternate[] = {static_cast<char>(lead), '_'};
            wchar_t wide, other;
            if (IsDBCSLeadByteEx(GetACP(), lead) &&
                MultiByteToWideChar(GetACP(), MB_ERR_INVALID_CHARS, bytes, 2, &wide, 1) == 1 &&
                MultiByteToWideChar(GetACP(), MB_ERR_INVALID_CHARS, alternate, 2, &other, 1) == 1 && wide != other) {
                output[0] = bytes[0]; output[1] = bytes[1]; output[2] = 0;
                return true;
            }
        }
    }
    const wchar_t *choices[] = {L"\u4e2d", L"\u00e9", L"\u0410", L"\u03b1", L"\u0e01", L"\u0627", L"\u05d0"};
    for (const wchar_t *text : choices)
        if (NativeTextEncode(text, output, size)) return true;
    return false;
}

static inline bool NativeTextCasePair(char *upper, char *lower, int size) {
    const wchar_t *pairs[][2] = {{L"\u0410", L"\u0430"}, {L"\u00c9", L"\u00e9"},
                               {L"\u0391", L"\u03b1"}, {L"A", L"a"}};
    for (const auto &pair : pairs)
        if (NativeTextEncode(pair[0], upper, size) && NativeTextEncode(pair[1], lower, size)) return true;
    return false;
}
#endif

#endif // NATIVETEXTREFERENCE_H
