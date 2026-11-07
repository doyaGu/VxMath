#include "VxMessageBox.h"

#include <gtest/gtest.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_error.h>
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>

static HWND capturedOwner, nativeOwner;
static const char *capturedText, *capturedTitle;
static UINT capturedType;
static int nativeCalls, propertyCalls, errorCalls, nativeResult;
static DWORD nativeError;
static bool ownerIsNative, hasProperties;
static SDL_Window *sdlOwner;
static int windowListCalls;

static BOOL WINAPI TestIsWindow(HWND) { return ownerIsNative; }
static int WINAPI TestMessageBoxA(HWND owner, const char *text, const char *title, UINT type) {
    ++nativeCalls;
    capturedOwner = owner; capturedText = text; capturedTitle = title; capturedType = type;
    if (!nativeResult) SetLastError(nativeError);
    return nativeResult;
}

extern "C" SDL_PropertiesID SDLCALL SDL_GetWindowProperties(SDL_Window *) {
    ++propertyCalls;
    return hasProperties ? 73 : 0;
}
extern "C" SDL_Window **SDLCALL SDL_GetWindows(int *count) {
    ++windowListCalls;
    *count = sdlOwner ? 1 : 0;
    return &sdlOwner;
}
extern "C" void SDLCALL SDL_free(void *memory) {
    EXPECT_EQ(memory, &sdlOwner);
}
extern "C" void *SDLCALL SDL_GetPointerProperty(SDL_PropertiesID properties, const char *name, void *fallback) {
    EXPECT_EQ(properties, 73u);
    EXPECT_STREQ(name, SDL_PROP_WINDOW_WIN32_HWND_POINTER);
    EXPECT_EQ(fallback, nullptr);
    return nativeOwner;
}
extern "C" bool SDLCALL SDL_SetError(const char *format, ...) {
    char message[128];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);
    EXPECT_NE(message[0], 0);
    ++errorCalls;
    SetLastError(999);
    return false;
}

// Compile the actual adapter with only the two native UI calls substituted.
// No production callback, alternate implementation or modal dialog is involved.
#define IsWindow TestIsWindow
#define MessageBoxA TestMessageBoxA
#include "../../src/VxMessageBoxWin32.cpp"
#undef MessageBoxA
#undef IsWindow

class MessageBoxWin32Test : public testing::Test {
protected:
    void SetUp() override {
        capturedOwner = nativeOwner = nullptr;
        capturedText = capturedTitle = nullptr;
        capturedType = 0;
        nativeCalls = propertyCalls = errorCalls = 0;
        nativeResult = IDOK;
        nativeError = ERROR_INVALID_WINDOW_HANDLE;
        ownerIsNative = false;
        hasProperties = true;
        sdlOwner = nullptr;
        windowListCalls = 0;
        SetLastError(0);
    }
};

TEST_F(MessageBoxWin32Test, NullOwnerAndStringsReachTheOriginalApiUnchanged) {
    EXPECT_EQ(VxMessageBox(nullptr, nullptr, nullptr, 0), IDOK);
    EXPECT_EQ(capturedOwner, nullptr);
    EXPECT_EQ(capturedText, nullptr);
    EXPECT_EQ(capturedTitle, nullptr);
    EXPECT_EQ(nativeCalls, 1);
    EXPECT_EQ(propertyCalls, 0);
    EXPECT_EQ(windowListCalls, 0);
}

TEST_F(MessageBoxWin32Test, EveryOriginalFlagBitIsForwardedWithoutFiltering) {
    const XDWORD flags[] = {MB_HELP, MB_DEFBUTTON4, MB_SYSTEMMODAL, MB_TASKMODAL, MB_DEFAULT_DESKTOP_ONLY,
        MB_RIGHT, MB_RTLREADING, MB_SETFOREGROUND, MB_TOPMOST, MB_SERVICE_NOTIFICATION,
        MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2 | MB_HELP | MB_TOPMOST | MB_RIGHT,
        0xffffffffu};
    for (XDWORD type : flags) {
        EXPECT_EQ(VxMessageBox(nullptr, "Text", "Title", type), IDOK);
        EXPECT_EQ(capturedType, type);
    }
}

TEST_F(MessageBoxWin32Test, EveryNativeSelectionIsReturnedUnchanged) {
    const int selections[] = {IDOK, IDCANCEL, IDABORT, IDRETRY, IDIGNORE, IDYES, IDNO, IDTRYAGAIN, IDCONTINUE};
    for (int selection : selections) {
        nativeResult = selection;
        EXPECT_EQ(VxMessageBox(nullptr, "Text", "Title", MB_YESNOCANCEL), selection);
    }
}

TEST_F(MessageBoxWin32Test, NativeOwnerNeedsNoSdlObjectConversion) {
    int owner;
    ownerIsNative = true;
    EXPECT_EQ(VxMessageBox(&owner, "Text", "Title", MB_HELP), IDOK);
    EXPECT_EQ(capturedOwner, reinterpret_cast<HWND>(&owner));
    EXPECT_EQ(propertyCalls, 0);
}

TEST_F(MessageBoxWin32Test, SdlOwnerUsesItsActualNativeWindow) {
    int window, owner;
    sdlOwner = reinterpret_cast<SDL_Window *>(&window);
    nativeOwner = reinterpret_cast<HWND>(&owner);
    EXPECT_EQ(VxMessageBox(&window, "Text", "Title", MB_TASKMODAL), IDOK);
    EXPECT_EQ(capturedOwner, nativeOwner);
    EXPECT_EQ(propertyCalls, 1);
}

TEST_F(MessageBoxWin32Test, InvalidOwnerCannotSilentlyBecomeAnUnownedDialog) {
    int window;
    sdlOwner = reinterpret_cast<SDL_Window *>(&window);
    hasProperties = false;
    EXPECT_EQ(VxMessageBox(&window, "Text", "Title", 0), 0);
    EXPECT_EQ(nativeCalls, 0);
    hasProperties = true;
    EXPECT_EQ(VxMessageBox(&window, "Text", "Title", 0), 0);
    EXPECT_EQ(nativeCalls, 0);
    EXPECT_EQ(errorCalls, 1);
}

TEST_F(MessageBoxWin32Test, InvalidNativeHandleIsForwardedWithoutDereferencingIt) {
    WIN_HANDLE owner = reinterpret_cast<WIN_HANDLE>(1);
    nativeResult = 0;
    EXPECT_EQ(VxMessageBox(owner, "Text", "Title", 0), 0);
    EXPECT_EQ(capturedOwner, static_cast<HWND>(owner));
    EXPECT_EQ(propertyCalls, 0);
    EXPECT_EQ(nativeCalls, 1);
    EXPECT_EQ(GetLastError(), ERROR_INVALID_WINDOW_HANDLE);
}

TEST_F(MessageBoxWin32Test, NativeAnsiBytesArePassedWithoutUtf8Reinterpretation) {
    const char text[] = {'A', char(0x81), char(0x5c), 0};
    const char title[] = {char(0xd6), char(0xd0), 0};
    EXPECT_EQ(VxMessageBox(nullptr, text, title, MB_ICONQUESTION), IDOK);
    EXPECT_EQ(capturedText, text);
    EXPECT_EQ(capturedTitle, title);
    EXPECT_EQ(capturedType, MB_ICONQUESTION);
}

TEST_F(MessageBoxWin32Test, BackendFailurePreservesTheNativeErrorAndReportsSdlError) {
    nativeResult = 0;
    EXPECT_EQ(VxMessageBox(nullptr, "Text", "Title", 0), 0);
    EXPECT_EQ(GetLastError(), nativeError);
    EXPECT_EQ(errorCalls, 1);
}

TEST_F(MessageBoxWin32Test, SuccessDoesNotReplaceTheNativeLastError) {
    SetLastError(37);
    EXPECT_EQ(VxMessageBox(nullptr, "Text", "Title", 0), IDOK);
    EXPECT_EQ(GetLastError(), 37u);
    EXPECT_EQ(errorCalls, 0);
}
