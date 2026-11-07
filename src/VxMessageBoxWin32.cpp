#include "VxMessageBox.h"

#include <SDL3/SDL_video.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_error.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

int VxMessageBox(WIN_HANDLE hWnd, const char *lpText, const char *lpCaption, XDWORD uType) {
    HWND owner = static_cast<HWND>(hWnd);
    if (owner && !IsWindow(owner)) {
        // Never dereference an arbitrary native handle as an SDL object.
        int count = 0;
        SDL_Window **windows = SDL_GetWindows(&count);
        SDL_Window *window = nullptr;
        for (int i = 0; windows && i < count; ++i) {
            if (windows[i] == hWnd) {
                window = windows[i];
                break;
            }
        }
        SDL_free(windows);
        if (window) {
            const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
            if (!properties)
                return 0;
            owner = static_cast<HWND>(SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
            if (!owner) {
                SDL_SetError("The message-box owner has no native window");
                return 0;
            }
        }
    }
    // The original forwards ANSI strings and the entire flag word to User32.
    // Native handling preserves Help, modality, desktop, RTL and question icons.
    const int selected = MessageBoxA(owner, lpText, lpCaption, uType);
    if (!selected) {
        const DWORD error = GetLastError();
        SDL_SetError("MessageBoxA failed (%lu)", static_cast<unsigned long>(error));
        SetLastError(error);
    }
    return selected;
}
