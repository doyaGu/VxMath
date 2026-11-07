#include "VxMessageBox.h"

#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_error.h>

int VxMessageBox(WIN_HANDLE hWnd, const char *lpText, const char *lpCaption, XDWORD uType) {
    // Preserve the MB_* button groups and their ID* return values.
    static const SDL_MessageBoxButtonData buttonSets[7][3] = {
        {{0, 1, "OK"}},
        {{0, 1, "OK"}, {0, 2, "Cancel"}},
        {{0, 3, "Abort"}, {0, 4, "Retry"}, {0, 5, "Ignore"}},
        {{0, 6, "Yes"}, {0, 7, "No"}, {0, 2, "Cancel"}},
        {{0, 6, "Yes"}, {0, 7, "No"}},
        {{0, 4, "Retry"}, {0, 2, "Cancel"}},
        {{0, 2, "Cancel"}, {0, 10, "Try Again"}, {0, 11, "Continue"}}
    };
    static const int buttonCounts[] = {1, 2, 3, 3, 2, 2, 3};
    const XDWORD type = uType & 0x0fu;
    if (type >= 7) {
        SDL_SetError("Invalid VxMessageBox button type");
        return 0;
    }
    const int count = buttonCounts[type];
    const int defaultButton = (uType >> 8) & 3u;
    if (defaultButton >= count) {
        SDL_SetError("Invalid VxMessageBox default button");
        return 0;
    }
    SDL_MessageBoxButtonData buttons[3];
    for (int i = 0; i < count; ++i) {
        buttons[i] = buttonSets[type][i];
        if (i == defaultButton)
            buttons[i].flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
        if (buttons[i].buttonID == 2 || type == 0)
            buttons[i].flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
    }

    SDL_MessageBoxData data = {};
    data.flags = SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT;
    switch (uType & 0xf0u) {
        case 0x10: data.flags |= SDL_MESSAGEBOX_ERROR; break;
        case 0x20: data.flags |= SDL_MESSAGEBOX_INFORMATION; break; // Portable question fallback.
        case 0x30: data.flags |= SDL_MESSAGEBOX_WARNING; break;
        case 0x40: data.flags |= SDL_MESSAGEBOX_INFORMATION; break;
    }
    data.window = static_cast<SDL_Window *>(hWnd);
    data.title = lpCaption ? lpCaption : "Error";
    data.message = lpText ? lpText : "";
    data.numbuttons = count;
    data.buttons = buttons;
    int selected = -1;
    return SDL_ShowMessageBox(&data, &selected) && selected >= 0 ? selected : 0;
}
