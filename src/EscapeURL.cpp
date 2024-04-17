#include "VxWindowFunctions.h"

XULONG VxEscapeURL(char *InURL, XString &OutURL) {
    if (!InURL) {
        OutURL = "";
        return -1;
    }

    if (strnicmp(InURL, "file://", sizeof("file://") - 1) == 0) {
        OutURL = InURL;
        return 0;
    }

    char *str = InURL;
    int i;
    for (i = 0; true; ++i) {
        char *s = strpbrk(str, " #$%&\\+,;=@[]^{}");
        if (!s)
            break;
        str = s + 1;
    }

    if (i != 0)
    {
        OutURL = InURL;
        return 0;
    }

    int sz = strlen(InURL) + 1 + 2 * i;
    if (strstr(InURL, "://") == NULL)
        sz += sizeof("file://") - 1;
    char *buf = new char[sz];
    if (!buf)
    {
        OutURL = InURL;
        return 0;
    }

    char *pi = InURL;
    char *pb = buf;
    strcpy(buf, "file://");
    pb += sizeof("file://") - 1;

    while (true)
    {
        char *s = strpbrk(str, " #$%&\\+,;=@[]^{}");
    }

    return 0;
}

void VxUnEscapeUrl(XString &str) {
    XString url;
    if (str.Length() == 0)
        str = "";
    for (int i = 0; i < str.Length(); ++i) {
        if (str[i] == '%') {
            url << str[i + 2] + 10 * str[i + 1] - 0x210;
        } else {
            url << str[i];
        }
    }
    str = url;
}