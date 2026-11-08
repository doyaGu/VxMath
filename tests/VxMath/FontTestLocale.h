#ifndef FONTTESTLOCALE_H
#define FONTTESTLOCALE_H

#include "XString.h"

#include <locale.h>
#include <wchar.h>

struct FontTestLocale {
    XString previous;
    explicit FontTestLocale(const char *name) : previous(setlocale(LC_CTYPE, NULL)) {
        if (!setlocale(LC_CTYPE, name)) throw "test locale unavailable";
    }
    ~FontTestLocale() { setlocale(LC_CTYPE, previous.CStr()); }
};

struct FontTestUtf8Locale : FontTestLocale {
    FontTestUtf8Locale() : FontTestLocale("C") {
        const char *names[] = {"C.UTF-8", ".UTF-8", "en_US.UTF-8", "UTF-8"};
        for (int i = 0; i < 4; ++i) {
            if (!setlocale(LC_CTYPE, names[i])) continue;
            wchar_t character = 0;
            mbstate_t state = {};
            if (mbrtowc(&character, "\xC3\xA9", 2, &state) != 2 || character != 0xE9) continue;
            state = {};
            if (mbrtowc(&character, "\x81", 1, &state) == (size_t)-1) return;
        }
        throw "UTF-8 test locale unavailable";
    }
};

#endif
