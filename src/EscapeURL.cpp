#include "VxWindowFunctions.h"

#include "VxPathEncoding.h"

static bool IsHexDigit(char c) {
    return (c >= '0' && c <= '9') ||
        (c >= 'A' && c <= 'F') ||
        (c >= 'a' && c <= 'f');
}

static int HexCharToValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

XDWORD VxEscapeURL(const char *InURL, XString &OutURL) {
    if (!InURL) {
        OutURL = "";
        return static_cast<XDWORD>(-1);
    }
    size_t length = strlen(InURL);
    if (length > XString::MAX_LENGTH) return static_cast<XDWORD>(-1);
    if (strnicmp(InURL, "file://", 7) == 0) {
        OutURL = InURL;
        return 0;
    }

    const char *special = " #$%&\\+,;=@[]^{}";
    size_t required = length;
    bool escaped = false;
    for (const char *p = InURL; *p;) {
        const char *next = VxNextPathCharacter(p);
        if (next == p + 1 && strchr(special, *p)) {
            escaped = true;
            if (*p != '\\') required += 2;
        }
        p = next;
    }
    bool prefix = escaped && !strstr(InURL, "://");
    if (prefix) required += 7;
    if (required > XString::MAX_LENGTH) return static_cast<XDWORD>(-1);
    if (!escaped) {
        OutURL = InURL;
        return 0;
    }

    const char *digits = "0123456789abcdef";
    XString result;
    result.Reserve(static_cast<XWORD>(required));
    if (prefix) result << "file://";
    for (const char *p = InURL; *p;) {
        const char *next = VxNextPathCharacter(p);
        if (next != p + 1) {
            while (p != next) result << *p++;
            continue;
        }
        unsigned char c = static_cast<unsigned char>(*p++);
        if (c == '\\') result << '/';
        else if (strchr(special, c)) result << '%' << digits[c >> 4] << digits[c & 15];
        else result << static_cast<char>(c);
    }
    OutURL = result;
    return 0;
}

void VxUnEscapeUrl(XString &str) {
    if (str.Length() == 0) return;

    XString result;
    result.Reserve(str.Length());

    for (int i = 0; i < str.Length(); ++i) {
        if (str[i] == '%' && i + 2 < str.Length()) {
            char hex1 = str[i + 1];
            char hex2 = str[i + 2];

            // Validate that both characters are valid hex digits
            if (IsHexDigit(hex1) && IsHexDigit(hex2)) {
                // Convert hex to character
                int value = (HexCharToValue(hex1) << 4) | HexCharToValue(hex2);
                result += (char) value;
                i += 2; // Skip the next two characters
            } else {
                // Invalid hex sequence, keep the % and continue
                result += str[i];
            }
        } else {
            // Regular character or incomplete % sequence at end
            result += str[i];
        }
    }

    str = result;
}

// Alternative version of VxUnEscapeUrl with more strict validation
void VxUnEscapeUrlStrict(XString &str) {
    if (str.Length() == 0) {
        return;
    }

    XString result;
    result.Reserve(str.Length());

    for (int i = 0; i < str.Length(); ++i) {
        if (str[i] == '%') {
            // Check if we have enough characters for a complete hex sequence
            if (i + 2 < str.Length()) {
                char hex1 = str[i + 1];
                char hex2 = str[i + 2];

                if (IsHexDigit(hex1) && IsHexDigit(hex2)) {
                    // Valid hex sequence
                    int value = (HexCharToValue(hex1) << 4) | HexCharToValue(hex2);
                    result += (char) value;
                    i += 2;
                } else {
                    // Invalid hex sequence - could either skip or treat as literal
                    // This version treats as literal %
                    result += str[i];
                }
            } else {
                // Incomplete sequence at end of string
                result += str[i];
            }
        } else {
            result += str[i];
        }
    }

    str = result;
}
