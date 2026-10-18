#include "VxMath.h"
#include "XP.h"
#include <SDL3/SDL.h>
#include <climits>
#include <cstdio>
#include <cstring>
#include <locale.h>
#include <wchar.h>

static void Require(bool condition, const char *message) {
    if (!condition) throw message;
}

static XString FontPath(const char *name) {
    XString path(SDL_GetBasePath());
    path << "data/fonts/" << name;
    return path;
}

static void ReadFile(const char *path, XArray<XBYTE> &bytes) {
    SDL_IOStream *stream = SDL_IOFromFile(path, "rb");
    Require(stream != NULL, "open font fixture");
    struct Close { SDL_IOStream *stream; ~Close() { SDL_CloseIO(stream); } } close = {stream};
    Sint64 size = SDL_GetIOSize(stream);
    Require(size > 0 && size <= INT_MAX, "font fixture size");
    bytes.Resize((int)size);
    Require(SDL_ReadIO(stream, bytes.Begin(), (size_t)size) == (size_t)size, "read font fixture");
}

static void WriteFile(const char *path, const XBYTE *bytes, int size) {
    SDL_IOStream *stream = SDL_IOFromFile(path, "wb");
    Require(stream != NULL, "create font fixture");
    bool written = SDL_WriteIO(stream, bytes, size) == (size_t)size;
    bool closed = SDL_CloseIO(stream);
    Require(written && closed, "write font fixture");
}

static unsigned Read16(const XBYTE *at) { return (at[0] << 8) | at[1]; }
static unsigned Read32(const XBYTE *at) {
    return (unsigned(at[0]) << 24) | (unsigned(at[1]) << 16) | (unsigned(at[2]) << 8) | at[3];
}
static void Write32(XBYTE *at, unsigned value) {
    at[0] = value >> 24; at[1] = value >> 16; at[2] = value >> 8; at[3] = value;
}

static void PrepareFonts(bool collection) {
    XString directory(SDL_GetBasePath()); directory << "data/fonts";
    Require(SDL_CreateDirectory(directory.CStr()), "font fixture directory");
    const char *names[] = {"NotoSans-Regular.ttf", "NotoSans-Bold.ttf", "NotoSansSC-Regular.otf"};
    XArray<XBYTE> data[3];
    for (int i = 0; i < 3; ++i) {
        XString source(TEST_FONT_DIR); source << '/' << names[i];
        ReadFile(source.CStr(), data[i]);
    }
    if (!collection) {
        std::remove(FontPath("NotoSans.ttc").CStr());
        for (int i = 0; i < 3; ++i)
            WriteFile(FontPath(names[i]).CStr(), data[i].Begin(), data[i].Size());
        return;
    }

    // Assemble a TTC from the unchanged Regular and Bold sfnt tables. Table
    // offsets in a collection are relative to the collection, not each face.
    int offsets[] = {20, (20 + data[0].Size() + 3) & ~3};
    XArray<XBYTE> ttc;
    ttc.Resize(offsets[1] + data[1].Size());
    memset(ttc.Begin(), 0, ttc.Size());
    memcpy(ttc.Begin(), "ttcf", 4);
    Write32(ttc.Begin() + 4, 0x00010000);
    Write32(ttc.Begin() + 8, 2);
    for (int i = 0; i < 2; ++i) {
        Write32(ttc.Begin() + 12 + i * 4, offsets[i]);
        XBYTE *face = ttc.Begin() + offsets[i];
        memcpy(face, data[i].Begin(), data[i].Size());
        Require(data[i].Size() >= 12, "sfnt header");
        unsigned tables = Read16(face + 4);
        Require(tables <= unsigned((data[i].Size() - 12) / 16), "sfnt directory");
        for (unsigned table = 0; table < tables; ++table) {
            XBYTE *record = face + 12 + table * 16;
            unsigned offset = Read32(record + 8);
            Require(offset <= unsigned(data[i].Size()) && Read32(record + 12) <= unsigned(data[i].Size()) - offset,
                    "sfnt table extent");
            Write32(record + 8, offset + offsets[i]);
        }
    }
    WriteFile(FontPath("NotoSans.ttc").CStr(), ttc.Begin(), ttc.Size());
    // Only this executable's generated fixtures are removed. Its font directory
    // is separate from the other tests and from installed runtime assets.
    Require(std::remove(FontPath(names[0]).CStr()) == 0, "remove standalone Regular fixture");
    Require(std::remove(FontPath(names[1]).CStr()) == 0, "remove standalone Bold fixture");
    WriteFile(FontPath(names[2]).CStr(), data[2].Begin(), data[2].Size());
}

struct FontCollectionTestFont {
    FONT_HANDLE handle;
    explicit FontCollectionTestFont(int weight) : handle(VxCreateFont("Noto Sans", -28, weight, FALSE, FALSE)) {
        Require(handle != NULL, "create collection font");
    }
    ~FontCollectionTestFont() { VxDeleteFont(handle); }
};

static void CheckPixels(FontCollectionTestFont &font, const char *text, const char *name, bool collection) {
    const int width = 320, height = 100;
    XDWORD background[width * height];
    for (int i = 0; i < width * height; ++i) background[i] = 0xff102030;
    VxImageDescEx image;
    image.Width = width; image.Height = height; image.BytesPerLine = width * 4;
    image.BitsPerPixel = 32; image.RedMask = 0xff0000; image.GreenMask = 0xff00;
    image.BlueMask = 0xff; image.AlphaMask = 0xff000000;
    image.Image = reinterpret_cast<XBYTE *>(background);
    BITMAP_HANDLE bitmap = VxCreateBitmap(image);
    Require(bitmap != NULL, "create text bitmap");
    struct DeleteBitmap { BITMAP_HANDLE handle; ~DeleteBitmap() { VxDeleteBitmap(handle); } } cleanup = {bitmap};
    CKRECT rect = {0, 0, width, height};
    Require(VxDrawBitmapText(bitmap, font.handle, text, &rect, VXTEXT_LEFT | VXTEXT_TOP, 0x302010, 0x60c0f0),
            "draw collection text in the native character set");
    VxImageDescEx actual;
    XAP<XBYTE> pixels(VxConvertBitmap(bitmap, actual));
    Require(pixels != NULL && actual.Width == width && actual.Height == height && actual.BytesPerLine == width * 4,
            "owned text pixels");
    Require(memcmp(pixels, background, sizeof(background)) != 0, "visible collection text");
    XString reference(SDL_GetBasePath()); reference << "reference-" << name << ".bin";
    if (collection) {
        XArray<XBYTE> expected;
        ReadFile(reference.CStr(), expected);
        Require(expected.Size() == sizeof(background) && memcmp(expected.Begin(), pixels, expected.Size()) == 0,
                "collection face and mixed-text pixels match the standalone fonts");
    } else {
        WriteFile(reference.CStr(), pixels, sizeof(background));
    }
}

int main(int argc, char **argv) {
    try {
        Require(setlocale(LC_CTYPE, "") != NULL, "native test locale");
        Require(argc == 2 && (!strcmp(argv[1], "--reference") || !strcmp(argv[1], "--collection")), "font test mode");
        bool collection = !strcmp(argv[1], "--collection");
        PrepareFonts(collection);
        FontCollectionTestFont regular(400), bold(700);
        // Keep both faces live: a cache keyed only by file path mixes their data.
        CheckPixels(regular, "Agj AV", "regular", collection);
        CheckPixels(bold, "Agj AV", "bold", collection);
        char text[128] = {};
        Require(wcstombs(text, L"Agj \u4E2D\u6587 AV", sizeof(text)) != (size_t)-1, "native mixed text");
        CheckPixels(regular, text, "mixed-regular", collection);
        CheckPixels(bold, text, "mixed-bold", collection);
        return 0;
    } catch (const char *failure) {
        std::fprintf(stderr, "%s\n", failure);
        return 1;
    }
}
