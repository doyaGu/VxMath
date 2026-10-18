// Font handles own their loaded faces and pixels; no CK/editor types are needed.
#include "VxWindowFunctions.h"
#include "XArray.h"
#include "XClassArray.h"
#include "XP.h"
#include "VxMutex.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <climits>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include "VxFontSearch.h"
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#pragma GCC visibility push(hidden)
static bool ReadFontFile(const char *path, XArray<XBYTE> &bytes) {
    SDL_IOStream *input = SDL_IOFromFile(path, "rb");
    if (!input) return false;
    struct Close { SDL_IOStream *stream; ~Close() { SDL_CloseIO(stream); } } close = {input};
    const Sint64 length = SDL_GetIOSize(input);
    if (length <= 0 || length > INT_MAX) return false;
    bytes.Resize(static_cast<int>(length));
    return SDL_ReadIO(input, bytes.Begin(), static_cast<size_t>(length)) == static_cast<size_t>(length);
}
struct VxFontData {
    XArray<XBYTE> bytes;
    stbtt_fontinfo font{};
    VxFontFace description;
    int references = 0;
};
struct VxFontFiles {
    VxMutex mutex;
    XArray<VxFontData *> active;
};
static VxFontFiles &LoadedFiles() {
    static VxFontFiles files;
    return files;
}
struct VxFontFileLock {
    VxMutex &mutex;
    bool locked;
    explicit VxFontFileLock(VxMutex &value) : mutex(value), locked(value.EnterMutex() != FALSE) {}
    ~VxFontFileLock() { if (locked) mutex.LeaveMutex(); }
};
static VxFontData *AcquireFontData(const VxFontFace &face) {
    VxFontFiles &files = LoadedFiles();
    VxFontFileLock lock(files.mutex);
    if (!lock.locked) return NULL;
    for (int i = 0; i < files.active.Size(); ++i) {
        VxFontData *data = files.active[i];
        if (data->description.path == face.path && data->description.fontIndex == face.fontIndex) {
            ++data->references;
            return data;
        }
    }
    XP<VxFontData> data(new VxFontData);
    if (!ReadFontFile(face.path.CStr(), data->bytes)) return NULL;
    const int offset = stbtt_GetFontOffsetForIndex(data->bytes.Begin(), face.fontIndex);
    if (offset < 0 || !stbtt_InitFont(&data->font, data->bytes.Begin(), offset)) return NULL;
    data->description = face;
    data->references = 1;
    files.active.PushBack(data);
    return data.Release();
}
static void ReleaseFontData(VxFontData *data) {
    if (!data) return;
    VxFontFiles &files = LoadedFiles();
    VxFontFileLock lock(files.mutex);
    if (!lock.locked) return;
    if (--data->references == 0) {
        files.active.Remove(data);
        delete data;
    }
}
struct VxLoadedFontFace {
    VxFontData *data = NULL;
    float scale = 0;
    VxLoadedFontFace() = default;
    VxLoadedFontFace(const VxLoadedFontFace &) = delete;
    VxLoadedFontFace &operator=(const VxLoadedFontFace &) = delete;
    ~VxLoadedFontFace() { ReleaseFontData(data); }
    bool Load(const VxFontFace &face, float pixels) {
        data = AcquireFontData(face);
        if (!data) return false;
        scale = stbtt_ScaleForPixelHeight(&data->font, pixels);
        if (scale > 0 && std::isfinite(scale)) return true;
        ReleaseFontData(data);
        data = NULL;
        return false;
    }
};
struct VxBitmapFont {
    XClassArray<VxFontFace> candidates;
    XArray<VxLoadedFontFace *> loaded;
    ~VxBitmapFont() {
        for (int i = 0; i < loaded.Size(); ++i) delete loaded[i];
    }
    VxLoadedFontFace *GetFace(int index) {
        if (!loaded[index]) {
            XP<VxLoadedFontFace> face(new VxLoadedFontFace);
            // Keep an unsuccessful attempt too: do not reopen it for every glyph.
            face->Load(candidates[index], pixels);
            loaded[index] = face.Release();
        }
        return loaded[index]->data ? loaded[index] : NULL;
    }
    VXFONTINFO info{};
    float pixels = 0;
};
struct VxFontSurface {
    SDL_Surface *surface;
    explicit VxFontSurface(SDL_Surface *value = NULL) : surface(value) {}
    ~VxFontSurface() { SDL_DestroySurface(surface); }
    SDL_Surface *Release() { SDL_Surface *value = surface; surface = NULL; return value; }
};
struct VxFontGlyph {
    VxLoadedFontFace *face;
    int index;
    float x;
    bool bold, italic;
};
static void BlendGlyphPixel(SDL_Surface *image, int x, int y, Uint8 coverage, XDWORD color) {
    if (x < 0 || x >= image->w || y < 0 || y >= image->h || !coverage) return;
    Uint8 *pixel = static_cast<Uint8 *>(image->pixels) + y * image->pitch + x * 4;
    pixel[0] = color & 255; pixel[1] = (color >> 8) & 255; pixel[2] = (color >> 16) & 255;
    pixel[3] = static_cast<Uint8>(255 - ((255 - pixel[3]) * (255 - coverage) + 127) / 255);
}
// Prepare the complete text image before touching the caller's bitmap.
static SDL_Surface *Rasterize(VxBitmapFont &font, const XArray<wchar_t> &text, XDWORD color, int &left) {
    XArray<VxFontGlyph> glyphs;
    VxLoadedFontFace *previous = NULL;
    int previousGlyph = 0;
    float pen = 0, ascent = 0, descent = 0;
    int xMin = 0, xMax = 0;
    for (int character = 0; character < text.Size() - 1; ++character) {
        int codepoint = (int)text[character];
        if (codepoint == '\n' || codepoint == '\r') codepoint = ' ';
        VxLoadedFontFace *face = NULL;
        int index = 0;
        for (int i = 0; i < font.candidates.Size(); ++i) {
            face = font.GetFace(i);
            if (face) index = stbtt_FindGlyphIndex(&face->data->font, static_cast<int>(codepoint));
            if (index) break;
        }
        if (!index && codepoint != 0) return NULL;
        if (previous == face) pen += stbtt_GetGlyphKernAdvance(&face->data->font, previousGlyph, index) * face->scale;
        if (!std::isfinite(pen) || XAbs(pen) > INT_MAX / 2) return NULL;
        const bool bold = font.info.Weight >= 650 && face->data->description.weight < 650;
        const bool italic = font.info.Italic && !face->data->description.italic;
        VxFontGlyph glyph = {face, index, pen, bold, italic};
        glyphs.PushBack(glyph);
        int a, d, gap, advance, bearing, x0, y0, x1, y1;
        stbtt_GetFontVMetrics(&face->data->font, &a, &d, &gap);
        ascent = XMax(ascent, a * face->scale);
        descent = XMin(descent, d * face->scale);
        stbtt_GetGlyphBitmapBox(&face->data->font, index, face->scale, face->scale, &x0, &y0, &x1, &y1);
        const int slant = italic ? static_cast<int>(std::ceil(font.pixels * .22f)) : 0;
        xMin = XMin(xMin, static_cast<int>(std::floor(pen)) + x0 - slant);
        xMax = XMax(xMax, static_cast<int>(std::ceil(pen)) + x1 + slant + (bold ? 1 : 0));
        stbtt_GetGlyphHMetrics(&face->data->font, index, &advance, &bearing);
        pen += advance * face->scale + (bold ? 1 : 0);
        previous = face; previousGlyph = index;
    }
    xMax = XMax(xMax, static_cast<int>(std::ceil(pen)));
    const int baseline = static_cast<int>(std::ceil(ascent));
    const int underlineY = baseline + XMax(1, static_cast<int>(font.pixels / 12));
    const int thickness = XMax(1, static_cast<int>(font.pixels / 14));
    const int height = XMax(static_cast<int>(std::ceil(ascent - descent)), font.info.Underline ? underlineY + thickness : 0);
    const int width = xMax - xMin;
    if (width <= 0 || height <= 0) return NULL;
    VxFontSurface image(SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32));
    if (!image.surface || !SDL_ClearSurface(image.surface, 0, 0, 0, 0)) return NULL;
    for (int i = 0; i < glyphs.Size(); ++i) {
        const VxFontGlyph &glyph = glyphs[i];
        int width, height, x, y;
        unsigned char *bitmap = stbtt_GetGlyphBitmap(&glyph.face->data->font, glyph.face->scale, glyph.face->scale, glyph.index, &width, &height, &x, &y);
        if (!bitmap && width > 0 && height > 0) return NULL;
        for (int row = 0; row < height; ++row) {
            const int shear = glyph.italic ? static_cast<int>(std::round(-.22f * (y + row))) : 0;
            for (int column = 0; column < width; ++column) {
                const int at = static_cast<int>(std::round(glyph.x)) + x + column + shear - xMin;
                const Uint8 coverage = bitmap[row * width + column];
                BlendGlyphPixel(image.surface, at, baseline + y + row, coverage, color);
                if (glyph.bold) BlendGlyphPixel(image.surface, at + 1, baseline + y + row, coverage, color);
            }
        }
        stbtt_FreeBitmap(bitmap, NULL);
    }
    if (font.info.Underline) {
        for (int y = underlineY; y < underlineY + thickness; ++y)
            for (int x = -xMin; x < static_cast<int>(std::ceil(pen)) - xMin; ++x) BlendGlyphPixel(image.surface, x, y, 255, color);
    }
    left = xMin;
    return image.Release();
}
#pragma GCC visibility pop
#endif

// ============================================================================
// Font Functions
// ============================================================================

FONT_HANDLE VxCreateFont(const char *FontName, int FontSize, int Weight, XBOOL italic, XBOOL underline) {
#ifdef _WIN32
    if (!FontName || FontSize == 0 || FontSize == INT_MIN)
        return NULL;

    HDC dc = CreateCompatibleDC(NULL);
    if (!dc)
        return NULL;
    int logicalHeight = FontSize < 0 ? FontSize : -MulDiv(FontSize, GetDeviceCaps(dc, LOGPIXELSY), 72);
    HFONT font = CreateFontA(logicalHeight, 0, 0, 0, Weight, italic, underline, FALSE,
                             ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, FontName);
    DeleteDC(dc);
    return font;
#else
    if (!FontName || !FontSize || FontSize == INT_MIN) return NULL;
    try {
        XString family;
        if (!VxDecodeAnsiFontName(FontName, family)) return NULL;
        XP<VxBitmapFont> record(new VxBitmapFont);
        VxGetFontFallbacks(VxGetSystemFonts(), family.CStr(), Weight, italic, record->candidates);
        if (record->candidates.Size() == 0) return NULL;
        record->loaded.Resize(record->candidates.Size());
        for (int i = 0; i < record->loaded.Size(); ++i) record->loaded[i] = NULL;
        record->pixels = FontSize < 0 ? float(-FontSize) : float(FontSize) * 96.0f / 72.0f;
        if (!std::isfinite(record->pixels) || record->pixels > INT_MAX / 8) return NULL;
        if (!record->GetFace(0)) return NULL;
        record->info.FaceName = FontName;
        record->info.Height = static_cast<int>(std::ceil(record->pixels)); record->info.Weight = Weight;
        record->info.Italic = italic; record->info.Underline = underline;
        return reinterpret_cast<FONT_HANDLE>(record.Release());
    } catch (...) { return NULL; }
#endif
}

XBOOL VxGetFontInfo(FONT_HANDLE Font, VXFONTINFO &desc) {
#ifdef _WIN32
    if (!Font)
        return FALSE;
    LOGFONTA logFont;
    if (!GetObjectA(Font, sizeof(logFont), &logFont))
        return FALSE;
    desc.FaceName = logFont.lfFaceName;
    desc.Height = logFont.lfHeight < 0 ? -logFont.lfHeight : logFont.lfHeight;
    desc.Weight = logFont.lfWeight;
    desc.Italic = logFont.lfItalic ? TRUE : FALSE;
    desc.Underline = logFont.lfUnderline ? TRUE : FALSE;
    return TRUE;
#else
    if (!Font) return FALSE;
    desc = reinterpret_cast<VxBitmapFont *>(Font)->info;
    return TRUE;
#endif
}

XBOOL VxDrawBitmapText(BITMAP_HANDLE Bitmap, FONT_HANDLE Font, const char *string, CKRECT *rect, XDWORD Align, XDWORD BkColor, XDWORD FontColor) {
#ifdef _WIN32
    if (!Bitmap || !Font || !string || !rect)
        return FALSE;

    HDC dc = CreateCompatibleDC(NULL);
    if (!dc)
        return FALSE;
    HGDIOBJ oldBitmap = SelectObject(dc, Bitmap);
    HGDIOBJ oldFont = SelectObject(dc, Font);
    if (!oldBitmap || oldBitmap == HGDI_ERROR || !oldFont || oldFont == HGDI_ERROR) {
        if (oldBitmap && oldBitmap != HGDI_ERROR)
            SelectObject(dc, oldBitmap);
        if (oldFont && oldFont != HGDI_ERROR)
            SelectObject(dc, oldFont);
        DeleteDC(dc);
        return FALSE;
    }

    SetTextColor(dc, RGB(FontColor & 0xFF, (FontColor >> 8) & 0xFF, (FontColor >> 16) & 0xFF));
    SetBkColor(dc, RGB(BkColor & 0xFF, (BkColor >> 8) & 0xFF, (BkColor >> 16) & 0xFF));

    UINT flags = (Align & (VXTEXT_CENTER | VXTEXT_HCENTER)) ? DT_CENTER
               : (Align & VXTEXT_RIGHT) ? DT_RIGHT : DT_LEFT;
    if (Align & VXTEXT_BOTTOM)
        flags |= DT_BOTTOM | DT_SINGLELINE;
    else if (Align & VXTEXT_VCENTER)
        flags |= DT_VCENTER | DT_SINGLELINE;
    else
        flags |= DT_TOP | DT_SINGLELINE;

    const int result = DrawTextA(dc, string, -1, reinterpret_cast<LPRECT>(rect), flags);
    SelectObject(dc, oldFont);
    SelectObject(dc, oldBitmap);
    DeleteDC(dc);
    return result != 0;
#else
    if (!Bitmap || !Font || !string || !rect) return FALSE;
    XArray<wchar_t> text;
    try { if (!VxDecodeAnsiFontText(string, text)) return FALSE; } catch (...) { return FALSE; }
    SDL_Surface *destination = reinterpret_cast<SDL_Surface *>(Bitmap);
    SDL_Color background = {static_cast<Uint8>(BkColor & 255), static_cast<Uint8>((BkColor >> 8) & 255), static_cast<Uint8>((BkColor >> 16) & 255), 255};
    SDL_Rect area = {rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top};
    if (area.w <= 0 || area.h <= 0) return FALSE;
    const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(destination->format);
    VxFontSurface glyphs;
    int left = 0;
    if (text.Size() != 1) {
        try { glyphs.surface = Rasterize(*reinterpret_cast<VxBitmapFont *>(Font), text, FontColor, left); }
        catch (...) { return FALSE; }
        if (!glyphs.surface) return FALSE;
    }
    if (!SDL_FillSurfaceRect(destination, &area, SDL_MapRGBA(format, nullptr, background.r, background.g, background.b, 255))) return FALSE;
    if (text.Size() == 1) return TRUE;
    SDL_Rect at = {area.x, area.y, glyphs.surface->w, glyphs.surface->h};
    at.x += left;
    if (Align & (VXTEXT_CENTER | VXTEXT_HCENTER)) at.x += (area.w - glyphs.surface->w) / 2;
    else if (Align & VXTEXT_RIGHT) at.x += area.w - glyphs.surface->w;
    if (Align & VXTEXT_BOTTOM) at.y += area.h - glyphs.surface->h;
    else if (Align & VXTEXT_VCENTER) at.y += (area.h - glyphs.surface->h) / 2;
    SDL_Rect previous; SDL_GetSurfaceClipRect(destination, &previous);
    SDL_Rect clip;
    if (!SDL_GetRectIntersection(&previous, &area, &clip)) return TRUE;
    SDL_SetSurfaceClipRect(destination, &clip);
    const bool drawn = SDL_BlitSurface(glyphs.surface, nullptr, destination, &at);
    SDL_SetSurfaceClipRect(destination, &previous);
    return drawn ? TRUE : FALSE;
#endif
}

void VxDeleteFont(FONT_HANDLE Font) {
#ifdef _WIN32
    if (Font)
        DeleteObject(Font);
#else
    delete reinterpret_cast<VxBitmapFont *>(Font);
#endif
}
