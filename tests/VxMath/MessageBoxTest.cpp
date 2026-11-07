#include <gtest/gtest.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_error.h>

#include "VxMessageBox.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static SDL_MessageBoxData captured;
static SDL_MessageBoxButtonData capturedButtons[3];
static int messageBoxCalls;
static int simpleBoxCalls;
static int selectedButton;
static bool backendSucceeded;
static char backendError[128];

extern "C" bool SDLCALL SDL_ShowMessageBox(const SDL_MessageBoxData *data, int *button) {
    ++messageBoxCalls;
    captured = *data;
    if (data->numbuttons < 1 || data->numbuttons > 3 || !data->buttons) {
        ADD_FAILURE() << "Invalid message-box descriptor";
        return false;
    }
    memcpy(capturedButtons, data->buttons, sizeof(*data->buttons) * data->numbuttons);
    captured.buttons = capturedButtons;
    *button = selectedButton;
    return backendSucceeded;
}

extern "C" bool SDLCALL SDL_ShowSimpleMessageBox(SDL_MessageBoxFlags flags, const char *title,
                                                 const char *message, SDL_Window *window) {
    ++simpleBoxCalls;
    captured.flags = flags;
    captured.title = title;
    captured.message = message;
    captured.window = window;
    return backendSucceeded;
}

extern "C" bool SDLCALL SDL_SetError(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(backendError, sizeof(backendError), format, args);
    va_end(args);
    return false;
}

struct MessageBoxButtons {
    int count;
    int ids[3];
    const char *labels[3];
};

static const MessageBoxButtons buttonSets[] = {
    {1, {1}, {"OK"}},
    {2, {1, 2}, {"OK", "Cancel"}},
    {3, {3, 4, 5}, {"Abort", "Retry", "Ignore"}},
    {3, {6, 7, 2}, {"Yes", "No", "Cancel"}},
    {2, {6, 7}, {"Yes", "No"}},
    {2, {4, 2}, {"Retry", "Cancel"}},
    {3, {2, 10, 11}, {"Cancel", "Try Again", "Continue"}}
};

class MessageBoxTest : public testing::Test {
protected:
    void SetUp() override {
        captured = {};
        messageBoxCalls = simpleBoxCalls = 0;
        selectedButton = 1;
        backendSucceeded = true;
        backendError[0] = '\0';
    }
};

TEST_F(MessageBoxTest, DefaultOkReturnsSelectionAndPreservesNullDefaults) {
    EXPECT_EQ(VxMessageBox(nullptr, nullptr, nullptr, 0), 1);
    EXPECT_EQ(messageBoxCalls, 1);
    EXPECT_EQ(simpleBoxCalls, 0);
    EXPECT_EQ(captured.flags, SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT);
    EXPECT_STREQ(captured.title, "Error");
    EXPECT_STREQ(captured.message, "");
    EXPECT_EQ(captured.window, nullptr);
    ASSERT_EQ(captured.numbuttons, 1);
    EXPECT_EQ(capturedButtons[0].buttonID, 1);
    EXPECT_EQ(capturedButtons[0].flags, SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT);
}

TEST_F(MessageBoxTest, EveryButtonSetReturnsEverySelectedId) {
    for (XDWORD type = 0; type < 7; ++type) {
        const MessageBoxButtons &buttons = buttonSets[type];
        for (int selection = 0; selection < buttons.count; ++selection) {
            selectedButton = buttons.ids[selection];
            EXPECT_EQ(VxMessageBox(nullptr, "Text", "Title", type), selectedButton) << type << ',' << selection;
            ASSERT_EQ(captured.numbuttons, buttons.count);
            for (int i = 0; i < buttons.count; ++i) {
                EXPECT_EQ(capturedButtons[i].buttonID, buttons.ids[i]);
                EXPECT_STREQ(capturedButtons[i].text, buttons.labels[i]);
            }
        }
    }
    EXPECT_EQ(simpleBoxCalls, 0);
}

TEST_F(MessageBoxTest, EveryValidDefaultButtonReceivesReturnKey) {
    for (XDWORD type = 0; type < 7; ++type) {
        for (int selected = 0; selected < buttonSets[type].count; ++selected) {
            selectedButton = buttonSets[type].ids[selected];
            EXPECT_EQ(VxMessageBox(nullptr, "Text", "Title", type | (XDWORD(selected) << 8)), selectedButton);
            ASSERT_EQ(captured.numbuttons, buttonSets[type].count);
            for (int i = 0; i < captured.numbuttons; ++i)
                EXPECT_EQ((capturedButtons[i].flags & SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT) != 0, i == selected);
        }
    }
}

TEST_F(MessageBoxTest, EscapeSelectsCancelOrTheOnlyOkButton) {
    for (XDWORD type = 0; type < 7; ++type) {
        selectedButton = buttonSets[type].ids[0];
        VxMessageBox(nullptr, "Text", "Title", type);
        ASSERT_EQ(captured.numbuttons, buttonSets[type].count);
        for (int i = 0; i < captured.numbuttons; ++i) {
            const bool escape = capturedButtons[i].buttonID == 2 || (type == 0 && capturedButtons[i].buttonID == 1);
            EXPECT_EQ((capturedButtons[i].flags & SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT) != 0, escape);
        }
    }
}

TEST_F(MessageBoxTest, IconFieldIsDecodedAsAValue) {
    const SDL_MessageBoxFlags icons[] = {0, SDL_MESSAGEBOX_ERROR, SDL_MESSAGEBOX_INFORMATION,
                                         SDL_MESSAGEBOX_WARNING, SDL_MESSAGEBOX_INFORMATION};
    for (XDWORD icon = 0; icon < 5; ++icon) {
        EXPECT_EQ(VxMessageBox(nullptr, "Text", "Title", icon << 4), 1);
        EXPECT_EQ(captured.flags, icons[icon] | SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT);
    }
}

TEST_F(MessageBoxTest, PreservesParentAndUtf8Strings) {
    int parent;
    const char *message = "Text \xE6\xB5\x8B\xE8\xAF\x95";
    const char *title = "Title \xE6\xB5\x8B\xE8\xAF\x95";
    EXPECT_EQ(VxMessageBox(&parent, message, title, 0), 1);
    EXPECT_EQ(captured.window, reinterpret_cast<SDL_Window *>(&parent));
    EXPECT_STREQ(captured.message, message);
    EXPECT_STREQ(captured.title, title);
}

TEST_F(MessageBoxTest, BackendFailureReturnsZero) {
    backendSucceeded = false;
    selectedButton = 6;
    EXPECT_EQ(VxMessageBox(nullptr, "Text", "Title", 4), 0);
    EXPECT_EQ(messageBoxCalls, 1);
}

TEST_F(MessageBoxTest, ClosingWithoutSelectionReturnsZero) {
    selectedButton = -1;
    EXPECT_EQ(VxMessageBox(nullptr, "Text", "Title", 4), 0);
    EXPECT_EQ(messageBoxCalls, 1);
}

TEST_F(MessageBoxTest, InvalidButtonSetDoesNotOpenDialog) {
    for (XDWORD type = 7; type < 16; ++type)
        EXPECT_EQ(VxMessageBox(nullptr, "Text", "Title", type), 0);
    EXPECT_EQ(messageBoxCalls, 0);
    EXPECT_EQ(simpleBoxCalls, 0);
    EXPECT_NE(backendError[0], '\0');
}

TEST_F(MessageBoxTest, InvalidDefaultButtonDoesNotOpenDialog) {
    EXPECT_EQ(VxMessageBox(nullptr, "Text", "Title", 0x100), 0);
    EXPECT_EQ(messageBoxCalls, 0);
    EXPECT_EQ(simpleBoxCalls, 0);
    EXPECT_NE(backendError[0], '\0');
}
