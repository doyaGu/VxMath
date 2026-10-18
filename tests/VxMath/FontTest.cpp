#include <gtest/gtest.h>
#include "VxMath.h"
#include "XP.h"
#include <SDL3/SDL.h>
#include <cstring>
#include <climits>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <locale.h>
#include <wchar.h>
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include <cmath>

struct FontTestLocale {
    XString previous;
    explicit FontTestLocale(const char *name) : previous(setlocale(LC_CTYPE, NULL)) {
        if (!setlocale(LC_CTYPE, name)) throw "test locale unavailable";
    }
    ~FontTestLocale() { setlocale(LC_CTYPE, previous.CStr()); }
};
#endif

static const int Width = 320, Height = 100;
struct FontTestFont {
    FONT_HANDLE handle;
    FontTestFont(const char *name, int size = -28, int weight = 400, XBOOL italic = FALSE, XBOOL underline = FALSE)
        : handle(VxCreateFont(name, size, weight, italic, underline)) {}
    ~FontTestFont() { VxDeleteFont(handle); }
};
struct FontTestBitmap {
    BITMAP_HANDLE handle;
    FontTestBitmap() {
        XDWORD pixels[Width * Height];
        for (int i = 0; i < Width * Height; ++i) pixels[i] = 0xff102030;
        VxImageDescEx image;
        image.Width = Width; image.Height = Height; image.BytesPerLine = Width * 4;
        image.BitsPerPixel = 32; image.RedMask = 0xff0000; image.GreenMask = 0xff00;
        image.BlueMask = 0xff; image.AlphaMask = 0xff000000;
        image.Image = reinterpret_cast<XBYTE *>(pixels);
        handle = VxCreateBitmap(image);
    }
    ~FontTestBitmap() { VxDeleteBitmap(handle); }
};
static void ExpectSame(FontTestBitmap &first, FontTestBitmap &second) {
    VxImageDescEx a, b;
    XAP<XBYTE> pa(VxConvertBitmap(first.handle, a)), pb(VxConvertBitmap(second.handle, b));
    ASSERT_NE(nullptr, static_cast<XBYTE *>(pa)); ASSERT_NE(nullptr, static_cast<XBYTE *>(pb));
    ASSERT_EQ(a.BytesPerLine, b.BytesPerLine);
    EXPECT_EQ(0, std::memcmp(a.Image, b.Image, a.BytesPerLine * a.Height));
}
static int InkBounds(FontTestBitmap &bitmap, CKRECT &bounds) {
    VxImageDescEx image;
    XAP<XBYTE> pixels(VxConvertBitmap(bitmap.handle, image));
    if (!static_cast<XBYTE *>(pixels)) return 0;
    bounds.left = Width; bounds.top = Height; bounds.right = bounds.bottom = 0;
    int count = 0;
    for (int y = 0; y < Height; ++y) for (int x = 0; x < Width; ++x) {
        XDWORD pixel;
        std::memcpy(&pixel, image.Image + y * image.BytesPerLine + x * 4, 4);
        if ((pixel & 0xffffff) != 0x102030) {
            bounds.left = XMin(bounds.left, x); bounds.top = XMin(bounds.top, y);
            bounds.right = XMax(bounds.right, x + 1); bounds.bottom = XMax(bounds.bottom, y + 1);
            ++count;
        }
    }
    return count;
}

TEST(FontTest, RejectsInvalidHandlesAndSizes) {
    EXPECT_EQ(nullptr, VxCreateFont(nullptr, 12, 400, FALSE, FALSE));
    EXPECT_EQ(nullptr, VxCreateFont("Arial", 0, 400, FALSE, FALSE));
    EXPECT_EQ(nullptr, VxCreateFont("Arial", INT_MIN, 400, FALSE, FALSE));
    VXFONTINFO info;
    EXPECT_FALSE(VxGetFontInfo(nullptr, info));
    EXPECT_FALSE(VxDrawBitmapText(nullptr, nullptr, "", nullptr, 0, 0, 0));
    VxDeleteFont(nullptr);
}

#ifdef _WIN32
TEST(FontTest, MatchesNativeAnsiGdiFontSelectionStylesAndDrawing) {
    const char *names[] = {"Arial", "\xcb\xce\xcc\xe5", "Missing Original Family"};
    const XDWORD align[] = {VXTEXT_LEFT | VXTEXT_TOP, VXTEXT_CENTER | VXTEXT_VCENTER, VXTEXT_RIGHT | VXTEXT_BOTTOM};
    for (int n = 0; n < 3; ++n) {
        FontTestFont font(names[n], -28, 700, TRUE, TRUE);
        ASSERT_NE(nullptr, font.handle);
        LOGFONTA logical;
        ASSERT_NE(0, GetObjectA(font.handle, sizeof(logical), &logical));
        EXPECT_STREQ(names[n], logical.lfFaceName);
        EXPECT_EQ(ANSI_CHARSET, logical.lfCharSet);
        EXPECT_EQ(-28, logical.lfHeight);
        EXPECT_EQ(700, logical.lfWeight);
        EXPECT_TRUE(logical.lfItalic); EXPECT_TRUE(logical.lfUnderline);
        VXFONTINFO info;
        ASSERT_TRUE(VxGetFontInfo(font.handle, info));
        EXPECT_STREQ(names[n], info.FaceName.CStr()); EXPECT_EQ(28, info.Height);
        for (int a = 0; a < 3; ++a) {
            FontTestBitmap actual, expected;
            CKRECT rect = {10, 5, Width - 10, Height - 5};
            const char text[] = "A&B \xd6\xd0\xce\xc4";
            ASSERT_TRUE(VxDrawBitmapText(actual.handle, font.handle, text, &rect, align[a], 0x302010, 0x60c0f0));
            HDC dc = CreateCompatibleDC(nullptr);
            ASSERT_NE(nullptr, dc);
            HGDIOBJ oldBitmap = SelectObject(dc, expected.handle), oldFont = SelectObject(dc, font.handle);
            SetTextColor(dc, RGB(0xf0, 0xc0, 0x60)); SetBkColor(dc, RGB(0x10, 0x20, 0x30));
            RECT native = {10, 5, Width - 10, Height - 5};
            UINT flags = DT_SINGLELINE | (a == 1 ? DT_CENTER | DT_VCENTER : a == 2 ? DT_RIGHT | DT_BOTTOM : DT_LEFT | DT_TOP);
            EXPECT_GT(DrawTextA(dc, text, -1, &native, flags), 0);
            SelectObject(dc, oldFont); SelectObject(dc, oldBitmap); DeleteDC(dc);
            ExpectSame(actual, expected);
            CKRECT ink;
            EXPECT_GT(InkBounds(actual, ink), 0);
            EXPECT_STREQ("A&B \xd6\xd0\xce\xc4", text);
        }
    }
}
#else
TEST(FontTest, NativeCjkGlyphFallbackMatchesTheActualCjkFaceAndKeepsRawNames) {
    FontTestLocale locale("");
    FontTestFont fallback("Missing Original Family"), cjk("Noto Sans SC");
    ASSERT_NE(nullptr, fallback.handle); ASSERT_NE(nullptr, cjk.handle);
    char text[64] = {};
    ASSERT_NE((size_t)-1, wcstombs(text, L"\u4E2D\u6587", sizeof(text)));
    const XString original(text);
    FontTestBitmap first, second;
    CKRECT rect = {0, 0, Width, Height};
    ASSERT_TRUE(VxDrawBitmapText(first.handle, fallback.handle, text, &rect, VXTEXT_LEFT | VXTEXT_TOP, 0x302010, 0x60c0f0));
    ASSERT_TRUE(VxDrawBitmapText(second.handle, cjk.handle, text, &rect, VXTEXT_LEFT | VXTEXT_TOP, 0x302010, 0x60c0f0));
    ExpectSame(first, second);
    CKRECT ink;
    EXPECT_GT(InkBounds(first, ink), 100);
    EXPECT_LE(ink.right, 60); EXPECT_LE(ink.bottom, 35);
    VxImageDescEx image;
    XAP<XBYTE> pixels(VxConvertBitmap(first.handle, image));
    ASSERT_NE(nullptr, static_cast<XBYTE *>(pixels));
    bool foreground = false;
    for (int y = 0; y < image.Height; ++y) for (int x = 0; x < image.Width; ++x) {
        XDWORD pixel;
        std::memcpy(&pixel, image.Image + y * image.BytesPerLine + x * 4, 4);
        foreground = foreground || (pixel & 0xffffff) == 0xf0c060;
    }
    EXPECT_TRUE(foreground);
    VXFONTINFO info;
    ASSERT_TRUE(VxGetFontInfo(fallback.handle, info));
    EXPECT_STREQ("Missing Original Family", info.FaceName.CStr());
    EXPECT_STREQ(original.CStr(), text);
    char name[64] = {};
    ASSERT_NE((size_t)-1, wcstombs(name, L"\u5B8B\u4F53", sizeof(name)));
    FontTestFont ansiName(name);
    ASSERT_NE(nullptr, ansiName.handle);
    ASSERT_TRUE(VxGetFontInfo(ansiName.handle, info));
    EXPECT_STREQ(name, info.FaceName.CStr());
}

TEST(FontTest, InvalidNativeTextAndMissingGlyphLeaveTheBitmapUntouched) {
    FontTestLocale locale("C");
    FontTestFont font("Noto Sans");
    ASSERT_NE(nullptr, font.handle);
    FontTestBitmap actual, original;
    CKRECT rect = {0, 0, Width, Height};
    EXPECT_FALSE(VxDrawBitmapText(actual.handle, font.handle, "\x81", &rect, 0, 0, 0xffffff));
    ExpectSame(actual, original);
    EXPECT_FALSE(VxDrawBitmapText(actual.handle, font.handle, "\x01", &rect, 0, 0, 0xffffff));
    ExpectSame(actual, original);
    EXPECT_EQ(nullptr, VxCreateFont("\x81", -28, 400, FALSE, FALSE));
}

TEST(FontTest, NativeCharactersRenderTheExpectedGlyphPixels) {
    FontTestLocale locale("");
    size_t length = 0;
    void *bytes = SDL_LoadFile(TEST_REGULAR_FONT_FILE, &length);
    ASSERT_NE(nullptr, bytes);
    struct FreeBytes { void *value; ~FreeBytes() { SDL_free(value); } } freeBytes = {bytes};
    stbtt_fontinfo reference;
    ASSERT_NE(0, stbtt_InitFont(&reference, (const unsigned char *)bytes, 0));
    float scale = stbtt_ScaleForPixelHeight(&reference, 28);
    int ascent, descent, gap;
    stbtt_GetFontVMetrics(&reference, &ascent, &descent, &gap);
    const int baseline = (int)std::ceil(ascent * scale);
    const wchar_t values[] = {'A', 0xE9, 0x20AC, 0x2019, 0x153};
    FontTestFont font("Noto Sans"); ASSERT_NE(nullptr, font.handle);
    for (int i = 0; i < 5; ++i) {
        ASSERT_NE(0, stbtt_FindGlyphIndex(&reference, values[i]));
        char text[MB_LEN_MAX + 1]; mbstate_t state = {};
        size_t count = wcrtomb(text, values[i], &state);
        ASSERT_NE((size_t)-1, count); text[count] = 0;
        XString original(text);
        FontTestBitmap actual, expected;
        CKRECT rect = {0, 0, Width, Height};
        ASSERT_TRUE(VxDrawBitmapText(actual.handle, font.handle, text, &rect, VXTEXT_LEFT | VXTEXT_TOP, 0, 0xFFFFFF));
        EXPECT_STREQ(original.CStr(), text);
        SDL_Surface *destination = (SDL_Surface *)expected.handle;
        ASSERT_TRUE(SDL_FillSurfaceRect(destination, NULL, SDL_MapRGBA(SDL_GetPixelFormatDetails(destination->format), NULL, 0, 0, 0, 255)));
        int width, height, x, y;
        unsigned char *mask = stbtt_GetCodepointBitmap(&reference, scale, scale, values[i], &width, &height, &x, &y);
        ASSERT_NE(nullptr, mask);
        SDL_Surface *glyph = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32);
        ASSERT_NE(nullptr, glyph);
        for (int row = 0; row < height; ++row) for (int column = 0; column < width; ++column) {
            unsigned char *pixel = (unsigned char *)glyph->pixels + row * glyph->pitch + column * 4;
            pixel[0] = pixel[1] = pixel[2] = 255; pixel[3] = mask[row * width + column];
        }
        SDL_Rect at = {x, baseline + y, width, height};
        EXPECT_TRUE(SDL_SetSurfaceBlendMode(glyph, SDL_BLENDMODE_BLEND));
        EXPECT_TRUE(SDL_BlitSurface(glyph, NULL, destination, &at));
        SDL_DestroySurface(glyph); stbtt_FreeBitmap(mask, NULL);
        ExpectSame(actual, expected);
        CKRECT ink; EXPECT_GT(InkBounds(actual, ink), 0);
    }
}

TEST(FontTest, AlignmentStylesAndSharedFontLifetimePreservePixels) {
    FontTestFont plain("Noto Sans"), styled("Noto Sans", -28, 700, TRUE, TRUE);
    ASSERT_NE(nullptr, plain.handle); ASSERT_NE(nullptr, styled.handle);
    FontTestBitmap left, right, center, bold;
    CKRECT rect = {5, 5, Width - 5, Height - 5}, l, r, c, b;
    ASSERT_TRUE(VxDrawBitmapText(left.handle, plain.handle, "Ag", &rect, VXTEXT_LEFT | VXTEXT_TOP, 0x302010, 0x60c0f0));
    ASSERT_TRUE(VxDrawBitmapText(right.handle, plain.handle, "Ag", &rect, VXTEXT_RIGHT | VXTEXT_BOTTOM, 0x302010, 0x60c0f0));
    ASSERT_TRUE(VxDrawBitmapText(center.handle, plain.handle, "Ag", &rect, VXTEXT_CENTER | VXTEXT_VCENTER, 0x302010, 0x60c0f0));
    ASSERT_TRUE(VxDrawBitmapText(bold.handle, styled.handle, "Ag", &rect, VXTEXT_LEFT | VXTEXT_TOP, 0x302010, 0x60c0f0));
    int normal = InkBounds(left, l);
    EXPECT_GT(normal, 0); EXPECT_GT(InkBounds(right, r), 0); EXPECT_GT(InkBounds(center, c), 0);
    EXPECT_GT(InkBounds(bold, b), normal);
    EXPECT_GT(r.left, c.left); EXPECT_GT(c.left, l.left);
    EXPECT_GT(r.top, c.top); EXPECT_GT(c.top, l.top);
    EXPECT_GE(l.left, rect.left); EXPECT_LE(r.right, rect.right); EXPECT_LE(r.bottom, rect.bottom);
    VXFONTINFO info;
    ASSERT_TRUE(VxGetFontInfo(styled.handle, info));
    EXPECT_EQ(700, info.Weight); EXPECT_TRUE(info.Italic); EXPECT_TRUE(info.Underline);
    FontTestFont survivor("Noto Sans");
    for (int i = 0; i < 20; ++i) { FontTestFont transient("Noto Sans"); ASSERT_NE(nullptr, transient.handle); }
    FontTestBitmap after;
    ASSERT_TRUE(VxDrawBitmapText(after.handle, survivor.handle, "Ag", &rect, VXTEXT_LEFT | VXTEXT_TOP, 0x302010, 0x60c0f0));
    ExpectSame(left, after);
}
#endif
