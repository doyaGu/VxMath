#include <gtest/gtest.h>
#include "VxFontSearch.h"
#include <SDL3/SDL.h>
#include <locale.h>
#include <wchar.h>
#include <limits.h>

struct FontTestLocale {
    XString previous;
    explicit FontTestLocale(const char *name) : previous(setlocale(LC_CTYPE, NULL)) {
        if (!setlocale(LC_CTYPE, name)) throw "test locale unavailable";
    }
    ~FontTestLocale() { setlocale(LC_CTYPE, previous.CStr()); }
};

static VxFontFace MakeFace(const char *family, const char *path, int directory, int weight = 400, int index = 0) {
    VxFontFace face;
    face.family = family; face.path = path;
    face.directory = directory; face.weight = weight; face.fontIndex = index;
    return face;
}

TEST(FontSearchTest, ExactFamilyPrecedesAliasesAndDirectoryPrecedesStyle) {
    XClassArray<VxFontFace> faces;
    faces.PushBack(MakeFace("Liberation Sans", "/system/a.ttf", 3, 700));
    faces.PushBack(MakeFace("Noto Sans", "/app/z.ttf", 0));
    faces.PushBack(MakeFace("Noto Sans", "/system/bold.ttf", 3, 700));
    VxFontFace selected;
    ASSERT_TRUE(VxFindFontFace(faces, "Arial", 700, FALSE, selected));
    EXPECT_STREQ("/app/z.ttf", selected.path.CStr());
    ASSERT_TRUE(VxFindFontFace(faces, "Liberation Sans", 700, FALSE, selected));
    EXPECT_STREQ("/system/a.ttf", selected.path.CStr());
    faces.PushBack(MakeFace("Noto Sans", "/app/bold.ttf", 0, 700));
    ASSERT_TRUE(VxFindFontFace(faces, "Noto Sans", 700, FALSE, selected));
    EXPECT_STREQ("/app/bold.ttf", selected.path.CStr());
    faces[3].italic = TRUE;
    ASSERT_TRUE(VxFindFontFace(faces, "Noto Sans", 400, TRUE, selected));
    EXPECT_STREQ("/app/bold.ttf", selected.path.CStr());
}

TEST(FontSearchTest, AliasesRetainSerifAndMonospaceAndUnknownUsesSans) {
    XClassArray<VxFontFace> faces;
    faces.PushBack(MakeFace("DejaVu Sans", "/system/sans.ttf", 2));
    faces.PushBack(MakeFace("Liberation Serif", "/system/serif.ttf", 2));
    faces.PushBack(MakeFace("Noto Sans Mono", "/app/mono.ttf", 0));
    VxFontFace face;
    ASSERT_TRUE(VxFindFontFace(faces, "Unknown Original Family", 400, FALSE, face));
    EXPECT_STREQ("DejaVu Sans", face.family.CStr());
    ASSERT_TRUE(VxFindFontFace(faces, "Times New Roman", 400, FALSE, face));
    EXPECT_STREQ("Liberation Serif", face.family.CStr());
    ASSERT_TRUE(VxFindFontFace(faces, "Courier New", 400, FALSE, face));
    EXPECT_STREQ("Noto Sans Mono", face.family.CStr());
}

TEST(FontSearchTest, FallbackKeepsDistinctCollectionFacesAndRemovesDuplicates) {
    XClassArray<VxFontFace> faces, fallbacks;
    faces.PushBack(MakeFace("Noto Sans", "/app/fonts.ttc", 0, 400, 0));
    faces.PushBack(MakeFace("Noto Sans CJK SC", "/app/fonts.ttc", 0, 400, 1));
    VxGetFontFallbacks(faces, "Unknown", 400, FALSE, fallbacks);
    ASSERT_EQ(2, fallbacks.Size());
    EXPECT_EQ(0, fallbacks[0].fontIndex);
    EXPECT_EQ(1, fallbacks[1].fontIndex);
    faces.PushBack(MakeFace("Noto Sans SC", "/system/sc.ttf", 2));
    VxGetFontFallbacks(faces, "Unknown", 400, FALSE, fallbacks);
    ASSERT_EQ(3, fallbacks.Size());
    EXPECT_EQ(1, fallbacks[1].fontIndex);
    EXPECT_STREQ("/system/sc.ttf", fallbacks[2].path.CStr());
}

TEST(FontSearchTest, AnsiNamesRoundTripWithoutChangingInputOrReplacingFailures) {
    FontTestLocale locale("C");
    const char name[] = "Original Family";
    XString unicode, ansi;
    ASSERT_TRUE(VxDecodeAnsiFontName(name, unicode));
    EXPECT_STREQ(name, unicode.CStr());
    ASSERT_TRUE(VxEncodeAnsiFontName(unicode.CStr(), ansi));
    EXPECT_STREQ(name, ansi.CStr());
    EXPECT_STREQ("Original Family", name);
    unicode = "unchanged";
    EXPECT_FALSE(VxDecodeAnsiFontName("\x81", unicode));
    EXPECT_STREQ("unchanged", unicode.CStr());
    ansi = "unchanged";
    EXPECT_FALSE(VxEncodeAnsiFontName("\xf0\x9f\x98\x80", ansi));
    EXPECT_STREQ("unchanged", ansi.CStr());
}

TEST(FontSearchTest, TextNamesAndAtlasFollowTheCurrentRuntimeCharacterSet) {
    const char *locales[] = {"C", ""};
    for (int i = 0; i < 2; ++i) {
        FontTestLocale locale(locales[i]);
        for (int c = 1; c < 256; ++c) {
            char source[] = {(char)c, 0};
            wchar_t expected = 0;
            mbstate_t state = {};
            size_t count = mbrtowc(&expected, source, 1, &state);
            bool valid = count != (size_t)-1 && count != (size_t)-2;
            EXPECT_EQ(valid ? (int)expected : -1, VxGetFontCharacter((unsigned char)c));
            XArray<char> text;
            text.PushBack('x'); text.PushBack(0);
            XString name = "unchanged";
            EXPECT_EQ(valid, VxDecodeAnsiFontText(source, text) != FALSE);
            EXPECT_EQ(valid, VxDecodeAnsiFontName(source, name) != FALSE);
            if (valid) {
                const char *cursor = text.Begin(); size_t left = text.Size() - 1;
                EXPECT_EQ((Uint32)expected, SDL_StepUTF8(&cursor, &left));
                EXPECT_EQ(0u, left);
                EXPECT_STREQ(text.Begin(), name.CStr());
                XString original;
                ASSERT_TRUE(VxEncodeAnsiFontName(name.CStr(), original));
                EXPECT_STREQ(source, original.CStr());
            } else {
                EXPECT_STREQ("x", text.Begin());
                EXPECT_STREQ("unchanged", name.CStr());
            }
            EXPECT_EQ(c, (unsigned char)source[0]);
        }
        const wchar_t values[] = {0xE9, 0x20AC, 0x4E2D, 0x1F642, 0xFFFD};
        for (int v = 0; v < 5; ++v) {
            mbstate_t state = {};
            char native[MB_LEN_MAX + 1], utf8[5] = {};
            size_t count = wcrtomb(native, values[v], &state);
            *SDL_UCS4ToUTF8(values[v], utf8) = 0;
            XString name = "unchanged", original;
            if (count == (size_t)-1) {
                EXPECT_FALSE(VxEncodeAnsiFontName(utf8, name));
                EXPECT_STREQ("unchanged", name.CStr());
                continue;
            }
            native[count] = 0;
            ASSERT_TRUE(VxDecodeAnsiFontName(native, name));
            EXPECT_STREQ(utf8, name.CStr());
            ASSERT_TRUE(VxEncodeAnsiFontName(name.CStr(), original));
            EXPECT_STREQ(native, original.CStr());
            XArray<char> text;
            ASSERT_TRUE(VxDecodeAnsiFontText(native, text));
            EXPECT_STREQ(utf8, text.Begin());
        }
    }
}

TEST(FontSearchTest, InvalidInputAndOversizedNamesDoNotChangeTheResult) {
    FontTestLocale locale("C");
    XString name = "unchanged";
    const char *invalid[] = {NULL, "\xC0\xAF", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\xE2\x82"};
    for (int i = 0; i < 5; ++i) {
        EXPECT_FALSE(VxEncodeAnsiFontName(invalid[i], name));
        EXPECT_STREQ("unchanged", name.CStr());
    }
    XArray<char> source, text;
    source.Resize(70001); memset(source.Begin(), 'A', 70000); source[70000] = 0;
    ASSERT_TRUE(VxDecodeAnsiFontText(source.Begin(), text));
    EXPECT_EQ(source.Size(), text.Size());
    EXPECT_EQ(0, memcmp(source.Begin(), text.Begin(), source.Size()));
    EXPECT_FALSE(VxDecodeAnsiFontName(source.Begin(), name));
    EXPECT_FALSE(VxEncodeAnsiFontName(source.Begin(), name));
    EXPECT_STREQ("unchanged", name.CStr());
}

TEST(FontSearchTest, BundledDirectoryUsesExecutableLocationAndEnumeratesRealFaces) {
    XClassArray<XString> directories;
    VxGetFontDirectories(directories);
    ASSERT_GT(directories.Size(), 0);
    XString expected(SDL_GetBasePath()); expected << "data/fonts";
    EXPECT_EQ(expected, directories[0]);
    XClassArray<XString> bundled;
    bundled.PushBack(directories[0]);
    XClassArray<VxFontFace> faces;
    VxEnumerateFontDirectories(bundled, faces);
    VxFontFace regular, bold, cjk;
    ASSERT_TRUE(VxFindFontFace(faces, "Noto Sans", 400, FALSE, regular));
    EXPECT_NE(XString::NOTFOUND, regular.path.Find("NotoSans-Regular.ttf"));
    ASSERT_TRUE(VxFindFontFace(faces, "Noto Sans", 700, FALSE, bold));
    EXPECT_NE(XString::NOTFOUND, bold.path.Find("NotoSans-Bold.ttf"));
    ASSERT_TRUE(VxFindFontFace(faces, "Noto Sans SC", 400, FALSE, cjk));
    EXPECT_STREQ("Noto Sans SC", cjk.family.CStr());
    EXPECT_EQ(0, cjk.directory);
}

TEST(FontSearchTest, NativeFontDirectoriesRetainUtf8Names) {
    XString root(SDL_GetBasePath());
    char suffix[80];
    snprintf(suffix, sizeof(suffix), "VxFontPathTest-%llu/", (unsigned long long)SDL_GetPerformanceCounter());
    root << suffix;
    XString directory = root;
    directory << u8"\u5B57\u4F53-\U0001F642";
    ASSERT_TRUE(SDL_CreateDirectory(directory.CStr()));
    struct RemoveFontFixture {
        XString root, directory, file;
        ~RemoveFontFixture() {
            SDL_RemovePath(file.CStr());
            SDL_RemovePath(directory.CStr());
            SDL_RemovePath(root.CStr());
        }
    } cleanup = {root, directory, directory + "/regular.ttf"};
    XString source(SDL_GetBasePath());
    source << "data/fonts/NotoSans-Regular.ttf";
    ASSERT_TRUE(SDL_CopyFile(source.CStr(), cleanup.file.CStr()));
    XClassArray<XString> directories;
    directories.PushBack(directory);
    XClassArray<VxFontFace> faces;
    VxEnumerateFontDirectories(directories, faces);
    ASSERT_EQ(1, faces.Size());
    EXPECT_EQ(cleanup.file, faces[0].path);
    EXPECT_STREQ("Noto Sans", faces[0].family.CStr());
}
