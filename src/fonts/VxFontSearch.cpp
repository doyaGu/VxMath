#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "VxFontSearch.h"
#include "XArray.h"
#include <SDL3/SDL.h>
#include <sys/stat.h>
#include <limits.h>
#include <wchar.h>

#include "VxWindowFunctions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma GCC visibility push(hidden)
struct VxFontScanFailure {};
static XString ExpandHomePath(const char *path)
{
    if (!path || !*path)
        return XString();

    if (path[0] != '~')
        return XString(path);

    const char *home = getenv("HOME");
    if (!home || !*home)
        return XString();

    XString expanded(home);
    expanded << (path + 1);
    return expanded;
}

static void AddDirectory(XClassArray<XString> &dirs, const XString &dir)
{
    if (dir.Length() == 0)
        return;
    if (dirs.Find(dir) == dirs.End())
        dirs.PushBack(dir);
}

static void AddPathList(XClassArray<XString> &dirs, const char *pathList, const char *suffix)
{
    if (!pathList || !*pathList)
        return;

    const char *start = pathList;
    while (*start)
    {
        const char *end = strchr(start, ':');
        XString item(start, end ? (int)(end - start) : (int)strlen(start));
        if (item.Length() > 0)
        {
            if (suffix && *suffix)
                item << suffix;
            AddDirectory(dirs, item);
        }
        if (!end)
            break;
        start = end + 1;
    }
}

static XString NormalizeName(const XString &name)
{
    XString out;
    for (int i = 0; i < name.Length(); ++i)
    {
        unsigned char c = (unsigned char)name[i];
        if (c >= 128 || SDL_isalnum(c))
            out << (char)SDL_tolower(c);
    }
    return out;
}

static XBOOL HasFontExtension(const char *path)
{
    if (!path)
        return FALSE;

    const char *dot = strrchr(path, '.');
    if (!dot)
        return FALSE;

    XString ext(dot);
    return ext.ICompare(".ttf") == 0 ||
           ext.ICompare(".ttc") == 0 ||
           ext.ICompare(".otf") == 0;
}

static XBOOL ReadBinaryFile(const char *path, XArray<XBYTE> &data)
{
    data.Clear();
    if (!path || !*path)
        return FALSE;

    FILE *file = fopen(path, "rb");
    if (!file)
        return FALSE;

    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return FALSE;
    }

    long size = ftell(file);
    if (size <= 0 || size > INT_MAX)
    {
        fclose(file);
        return FALSE;
    }
    rewind(file);

    struct CloseFile { FILE *file; ~CloseFile() { fclose(file); } } close = {file};
    data.Resize((int)size);
    size_t read = fread(data.Begin(), 1, (size_t)size, file);

    if (read != (size_t)size)
    {
        data.Clear();
        return FALSE;
    }

    return TRUE;
}

static XString DecodeUtf16BE(const unsigned char *data, int length)
{
    char *text = SDL_iconv_string("UTF-8", "UTF-16BE", (const char *)data, (size_t)length);
    struct FreeText { char *text; ~FreeText() { SDL_free(text); } } freeText = {text};
    XString result(text ? text : "");
    return result;
}

static XString DecodeNameString(const char *data, int length, int platformId)
{
    if (!data || length <= 0)
        return XString();

    const unsigned char *bytes = (const unsigned char *)data;
    if (platformId == 0 || platformId == 3)
        return DecodeUtf16BE(bytes, length);

    XString out;
    for (int i = 0; i < length; ++i)
    {
        if (data[i])
            out << data[i];
    }
    return out;
}

static XString GetFontName(const stbtt_fontinfo &font, int nameId)
{
    struct Request
    {
        int platformId;
        int encodingId;
        int languageId;
    };

    static const Request requests[] = {
        {3, 1, 0x409},
        {3, 10, 0x409},
        {1, 0, 0},
        {0, 0, 0},
    };

    for (int i = 0; i < (int)(sizeof(requests) / sizeof(requests[0])); ++i)
    {
        int length = 0;
        const char *name = stbtt_GetFontNameString(&font, &length,
                                                   requests[i].platformId,
                                                   requests[i].encodingId,
                                                   requests[i].languageId,
                                                   nameId);
        XString decoded = DecodeNameString(name, length, requests[i].platformId);
        if (decoded.Length() > 0)
            return decoded;
    }

    return XString();
}

static XString FileStem(const char *path)
{
    if (!path)
        return XString();

    const char *name = strrchr(path, '/');
    const char *altName = strrchr(path, '\\');
    if (!name || (altName && altName > name))
        name = altName;
    name = name ? name + 1 : path;

    const char *dot = strrchr(name, '.');
    return XString(name, dot && dot > name ? (int)(dot - name) : 0);
}

static int WeightFromStyle(const XString &style, const XString &path)
{
    XString combined(style);
    combined << " ";
    combined << path;
    XString haystack = NormalizeName(combined);

    if (haystack.Find("thin") != XString::NOTFOUND)
        return 100;
    if (haystack.Find("extralight") != XString::NOTFOUND || haystack.Find("ultralight") != XString::NOTFOUND)
        return 200;
    if (haystack.Find("light") != XString::NOTFOUND)
        return 300;
    if (haystack.Find("medium") != XString::NOTFOUND)
        return 500;
    if (haystack.Find("semibold") != XString::NOTFOUND || haystack.Find("demibold") != XString::NOTFOUND)
        return 600;
    if (haystack.Find("extrabold") != XString::NOTFOUND || haystack.Find("ultrabold") != XString::NOTFOUND)
        return 800;
    if (haystack.Find("black") != XString::NOTFOUND || haystack.Find("heavy") != XString::NOTFOUND)
        return 900;
    if (haystack.Find("bold") != XString::NOTFOUND)
        return 700;
    return 400;
}

static XBOOL StyleIsItalic(const XString &style, const XString &path)
{
    XString combined(style);
    combined << " ";
    combined << path;
    XString haystack = NormalizeName(combined);
    return haystack.Find("italic") != XString::NOTFOUND ||
           haystack.Find("oblique") != XString::NOTFOUND;
}

static void AddFontFileFaces(const char *path, int directory, XClassArray<VxFontFace> &faces)
{
    XArray<XBYTE> data;
    if (!ReadBinaryFile(path, data))
        return;

    int fontCount = stbtt_GetNumberOfFonts(data.Begin());
    if (fontCount <= 0)
        fontCount = 1;

    for (int i = 0; i < fontCount; ++i)
    {
        int offset = stbtt_GetFontOffsetForIndex(data.Begin(), i);
        if (offset < 0)
            continue;

        stbtt_fontinfo font;
        if (!stbtt_InitFont(&font, data.Begin(), offset))
            continue;

        VxFontFace face;
        face.family = GetFontName(font, 16);
        if (face.family.Length() == 0)
            face.family = GetFontName(font, 1);
        if (face.family.Length() == 0)
            face.family = FileStem(path);

        face.style = GetFontName(font, 17);
        if (face.style.Length() == 0)
            face.style = GetFontName(font, 2);

        face.path = path;
        face.fontIndex = i;
        face.directory = directory;
        face.weight = WeightFromStyle(face.style, face.path);
        face.italic = StyleIsItalic(face.style, face.path);
        int os2 = stbtt__find_table(data.Begin(), offset, "OS/2");
        if (os2 && os2 <= data.Size() - 8)
            face.weight = data[os2 + 4] * 256 + data[os2 + 5];
        int head = stbtt__find_table(data.Begin(), offset, "head");
        if (head && head <= data.Size() - 46)
            face.italic = (data[head + 45] & 2) != 0;
        faces.PushBack(face);
    }
}

static XBOOL FindFamilies(const XClassArray<VxFontFace> &faces, const char *const *families,
                          int count, int weight, XBOOL italic, VxFontFace &result)
{
    XBOOL found = FALSE;
    int bestDirectory = INT_MAX, bestAlias = INT_MAX, bestStyle = INT_MAX;
    weight = XMax(0, XMin(weight, 1000));
    for (int i = 0; i < count; ++i)
    {
        XString normalized = NormalizeName(XString(families[i]));
        for (int j = 0; j < faces.Size(); ++j)
        {
            const VxFontFace &face = faces[j];
            if (NormalizeName(face.family) != normalized)
                continue;
            int style = XAbs(face.weight - weight) + ((face.italic != FALSE) != (italic != FALSE) ? 500 : 0);
            if (!found || face.directory < bestDirectory ||
                (face.directory == bestDirectory && (i < bestAlias || (i == bestAlias && style < bestStyle))))
            {
                found = TRUE;
                result = face;
                bestDirectory = face.directory;
                bestAlias = i;
                bestStyle = style;
            }
        }
    }
    return found;
}

static const char *Sans[] = {"Noto Sans", "Liberation Sans", "DejaVu Sans", "Helvetica", "Arial", "Droid Sans", "Roboto"};
static const char *Serif[] = {"Noto Serif", "Liberation Serif", "DejaVu Serif", "Times New Roman", "Times"};
static const char *Mono[] = {"Noto Sans Mono", "Liberation Mono", "DejaVu Sans Mono", "Courier New", "Courier", "Droid Sans Mono", "Roboto Mono"};
static const char *Cjk[] = {"Noto Sans SC", "Noto Sans CJK SC", "Source Han Sans SC", "WenQuanYi Zen Hei", "SimSun"};

static int CompareFaces(const void *lhs, const void *rhs)
{
    const VxFontFace &a = *(const VxFontFace *)lhs;
    const VxFontFace &b = *(const VxFontFace *)rhs;

    if (a.directory != b.directory)
        return a.directory < b.directory ? -1 : 1;
    int cmp = a.family.Compare(b.family);
    if (cmp != 0)
        return cmp;
    if (a.weight != b.weight)
        return a.weight < b.weight ? -1 : 1;
    if (a.italic != b.italic)
        return a.italic ? 1 : -1;
    cmp = a.path.Compare(b.path);
    return cmp ? cmp : (a.fontIndex > b.fontIndex) - (a.fontIndex < b.fontIndex);
}

static void SortFaces(XClassArray<VxFontFace> &faces)
{
    for (int i = 1; i < faces.Size(); ++i)
    {
        VxFontFace face = faces[i];
        int j = i - 1;
        while (j >= 0 && CompareFaces(&face, &faces[j]) < 0)
        {
            faces[j + 1] = faces[j];
            --j;
        }
        faces[j + 1] = face;
    }
}

struct VxFontScanContext
{
    XClassArray<VxFontFace> *faces;
    XClassArray<XString> *visitedFiles;
    XString directory;
    int depth;
    int priority;
    XBOOL failed;
};

static XString MakeFontChildPath(const XString &directory, const char *name)
{
    XString path(directory);
    if (path.Length() > 0) {
        char last = path.CStr()[path.Length() - 1];
        if (last != '/' && last != '\\')
            path << "/";
    }
    path << name;
    return path;
}

static void ScanFontDirectory(const XString &directory,
                              XClassArray<VxFontFace> &faces,
                              XClassArray<XString> &visitedFiles,
                              int depth, int priority);

static SDL_EnumerationResult SDLCALL ScanFontDirectoryEntry(void *userData, const char *, const char *name)
{
    VxFontScanContext *context = (VxFontScanContext *)userData;
    if (!name || !context)
        return SDL_ENUM_FAILURE;

    try
    {
        XString path = MakeFontChildPath(context->directory, name);
        SDL_PathInfo pathInfo;
        if (!SDL_GetPathInfo(path.CStr(), &pathInfo))
            return SDL_ENUM_CONTINUE;
        if (pathInfo.type == SDL_PATHTYPE_DIRECTORY)
        {
            struct stat info;
            if (lstat(path.CStr(), &info) == 0 && !S_ISLNK(info.st_mode))
                ScanFontDirectory(path, *context->faces, *context->visitedFiles, context->depth + 1, context->priority);
        }
        else if (HasFontExtension(path.CStr()) && context->visitedFiles->Find(path) == context->visitedFiles->End())
        {
            context->visitedFiles->PushBack(path);
            AddFontFileFaces(path.CStr(), context->priority, *context->faces);
        }
    }
    catch (...)
    {
        context->failed = TRUE;
        return SDL_ENUM_FAILURE;
    }
    return SDL_ENUM_CONTINUE;
}

static void ScanFontDirectory(const XString &directory,
                              XClassArray<VxFontFace> &faces,
                              XClassArray<XString> &visitedFiles,
                              int depth, int priority)
{
    SDL_PathInfo info;
    if (directory.Length() == 0 || depth > 24 || !SDL_GetPathInfo(directory.CStr(), &info) ||
        info.type != SDL_PATHTYPE_DIRECTORY)
        return;

    VxFontScanContext context;
    context.faces = &faces;
    context.visitedFiles = &visitedFiles;
    context.directory = directory;
    context.depth = depth;
    context.priority = priority;
    context.failed = FALSE;
    // The font catalog owns native UTF-8 paths, rather than legacy SDK paths.
    SDL_EnumerateDirectory(directory.CStr(), ScanFontDirectoryEntry, &context);
    if (context.failed)
        throw VxFontScanFailure();
}

XBOOL VxDecodeAnsiFontText(const char *text, XArray<char> &result)
{
    if (!text) return FALSE;
    size_t left = strlen(text);
    if (left > (INT_MAX - 1) / 4) return FALSE;
    XArray<char> buffer;
    buffer.Resize((int)(left * 4 + 1));
    char *output = buffer.Begin();
    mbstate_t state = {};
    while (left)
    {
        wchar_t character;
        size_t length = mbrtowc(&character, text, left, &state);
        if (length == (size_t)-1 || length == (size_t)-2 || !length ||
            character < 0 || character > 0x10FFFF || (character >= 0xD800 && character <= 0xDFFF))
            return FALSE;
        output = SDL_UCS4ToUTF8((Uint32)character, output);
        text += length;
        left -= length;
    }
    *output = 0;
    buffer.Resize((int)(output - buffer.Begin()) + 1);
    result = buffer;
    return TRUE;
}

int VxGetFontCharacter(unsigned char character)
{
    mbstate_t state = {};
    wchar_t value;
    char byte = (char)character;
    size_t length = mbrtowc(&value, &byte, 1, &state);
    if (length == (size_t)-1 || length == (size_t)-2) return -1;
    return (int)value;
}

XBOOL VxDecodeAnsiFontName(const char *name, XString &result)
{
    XArray<char> text;
    if (!VxDecodeAnsiFontText(name, text) || text.Size() - 1 > XString::MAX_LENGTH)
        return FALSE;
    result = text.Begin();
    return TRUE;
}

XBOOL VxEncodeAnsiFontName(const char *name, XString &result)
{
    if (!name) return FALSE;
    size_t remaining = strlen(name);
    XArray<char> bytes;
    bytes.Resize(XString::MAX_LENGTH + 1);
    int length = 0;
    mbstate_t state = {};
    while (remaining)
    {
        const char *start = name;
        Uint32 character = SDL_StepUTF8(&name, &remaining);
        if (character == SDL_INVALID_UNICODE_CODEPOINT &&
            (name - start != 3 || memcmp(start, "\xEF\xBF\xBD", 3))) return FALSE;
        char encoded[MB_LEN_MAX];
        size_t count = wcrtomb(encoded, (wchar_t)character, &state);
        if (count == (size_t)-1 || count > (size_t)(XString::MAX_LENGTH - length)) return FALSE;
        memcpy(bytes.Begin() + length, encoded, count);
        length += (int)count;
    }
    char terminator[MB_LEN_MAX];
    size_t count = wcrtomb(terminator, 0, &state);
    if (count == (size_t)-1 || count > (size_t)(XString::MAX_LENGTH + 1 - length)) return FALSE;
    memcpy(bytes.Begin() + length, terminator, count);
    result = bytes.Begin();
    return TRUE;
}

void VxGetFontDirectories(XClassArray<XString> &directories)
{
    directories.Clear();
    const char *base = SDL_GetBasePath();
    if (base && *base)
    {
        XString bundled(base);
        bundled << "data/fonts";
        AddDirectory(directories, bundled);
    }
#if defined(__APPLE__)
    AddDirectory(directories, ExpandHomePath("~/Library/Fonts"));
    AddDirectory(directories, XString("/Library/Fonts"));
    AddDirectory(directories, XString("/System/Library/Fonts"));
#else
    const char *userData = SDL_getenv("XDG_DATA_HOME");
    if (userData && *userData)
    {
        XString path(userData);
        path << "/fonts";
        AddDirectory(directories, path);
    }
    else
        AddDirectory(directories, ExpandHomePath("~/.local/share/fonts"));
    AddDirectory(directories, ExpandHomePath("~/.fonts"));
    const char *systemData = SDL_getenv("XDG_DATA_DIRS");
    AddPathList(directories, systemData && *systemData ? systemData : "/usr/local/share:/usr/share", "/fonts");
#endif
}

void VxEnumerateFontDirectories(const XClassArray<XString> &directories, XClassArray<VxFontFace> &faces)
{
    XClassArray<VxFontFace> found;
    XClassArray<XString> visitedFiles;
    for (int i = 0; i < directories.Size(); ++i)
        ScanFontDirectory(directories[i], found, visitedFiles, 0, i);
    SortFaces(found);
    faces = found;
}

const XClassArray<VxFontFace> &VxGetSystemFonts()
{
    struct Catalog
    {
        XClassArray<VxFontFace> faces;
        Catalog()
        {
            XClassArray<XString> directories;
            VxGetFontDirectories(directories);
            VxEnumerateFontDirectories(directories, faces);
        }
    };
    static const Catalog catalog;
    return catalog.faces;
}

XBOOL VxFindFontFace(const XClassArray<VxFontFace> &faces, const char *family, int weight, XBOOL italic, VxFontFace &result)
{
    if (family && *family && FindFamilies(faces, &family, 1, weight, italic, result))
        return TRUE;
    XString normalized = NormalizeName(XString(family ? family : ""));
    if ((normalized == "times" || normalized == "timesnewroman") &&
        FindFamilies(faces, Serif, sizeof(Serif) / sizeof(*Serif), weight, italic, result))
        return TRUE;
    if ((normalized == "courier" || normalized == "couriernew") &&
        FindFamilies(faces, Mono, sizeof(Mono) / sizeof(*Mono), weight, italic, result))
        return TRUE;
    if ((normalized == "simsun" || normalized == "songti" || normalized == "\xe5\xae\x8b\xe4\xbd\x93") &&
        FindFamilies(faces, Cjk, sizeof(Cjk) / sizeof(*Cjk), weight, italic, result))
        return TRUE;
    if (FindFamilies(faces, Sans, sizeof(Sans) / sizeof(*Sans), weight, italic, result))
        return TRUE;
    return FindFamilies(faces, Cjk, sizeof(Cjk) / sizeof(*Cjk), weight, italic, result);
}

static void AppendUnique(XClassArray<VxFontFace> &faces, const VxFontFace &face)
{
    for (int i = 0; i < faces.Size(); ++i)
        if (faces[i].path == face.path && faces[i].fontIndex == face.fontIndex)
            return;
    faces.PushBack(face);
}

void VxGetFontFallbacks(const XClassArray<VxFontFace> &faces, const char *family, int weight, XBOOL italic,
                        XClassArray<VxFontFace> &result)
{
    result.Clear();
    VxFontFace face;
    if (!VxFindFontFace(faces, family, weight, italic, face))
        return;
    AppendUnique(result, face);
    // A fixed family list bounds per-glyph work; collection faces are distinct.
    XClassArray<VxFontFace> cjk;
    for (int i = 0; i < (int)(sizeof(Cjk) / sizeof(*Cjk)); ++i)
        if (FindFamilies(faces, &Cjk[i], 1, weight, italic, face))
            cjk.PushBack(face);
    // Preserve family order within each directory, with bundled faces first.
    for (int i = 1; i < cjk.Size(); ++i)
    {
        VxFontFace candidate = cjk[i];
        int j = i - 1;
        while (j >= 0 && candidate.directory < cjk[j].directory)
        {
            cjk[j + 1] = cjk[j];
            --j;
        }
        cjk[j + 1] = candidate;
    }
    for (int i = 0; i < cjk.Size(); ++i)
        AppendUnique(result, cjk[i]);
    if (FindFamilies(faces, Sans, sizeof(Sans) / sizeof(*Sans), weight, italic, face))
        AppendUnique(result, face);
}
#pragma GCC visibility pop
