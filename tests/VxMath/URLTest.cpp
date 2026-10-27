#include "VxWindowFunctions.h"
#include "XP.h"
#include <gtest/gtest.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

static void ExpectEscaped(const char *input, const char *expected) {
    XString output("previous");
    ASSERT_EQ(0u, VxEscapeURL(input, output));
    EXPECT_STREQ(expected, output.CStr());
}

TEST(URLTest, NullInputClearsTheOutputAndReturnsFailure) {
    XString output("previous");
    EXPECT_EQ(static_cast<XDWORD>(-1), VxEscapeURL(nullptr, output)); EXPECT_TRUE(output.Empty());
}

TEST(URLTest, EmptyInputRemainsEmpty) { ExpectEscaped("", ""); }

TEST(URLTest, UnchangedPathsDoNotAcquireAFilePrefix) {
    const char *inputs[] = {"plain", "/path/to/file", "C:/file.txt", "relative/file.txt", "a:b?c"};
    for (const char *input : inputs) ExpectEscaped(input, input);
}

TEST(URLTest, FileProtocolPassesThroughVerbatim) {
    const char *inputs[] = {"file:///a b", "FILE:///a#b", "File://a%20b", "file://C:\\a b"};
    for (const char *input : inputs) ExpectEscaped(input, input);
}

TEST(URLTest, ProtocolAndPathDelimitersArePreserved) {
    ExpectEscaped("http://host/path", "http://host/path");
    ExpectEscaped("https://host/a b?x=1&y=2", "https://host/a%20b?x%3d1%26y%3d2");
}

TEST(URLTest, OriginalProtocolMarkerSuppressesTheFilePrefix) {
    ExpectEscaped(":// a", "://%20a"); ExpectEscaped("1bad://a b", "1bad://a%20b");
}

TEST(URLTest, SpecialCharactersUseTheOriginalTableAndLowercaseHex) {
    const char *inputs[] = {"a b", "a#b", "a$b", "a%b", "a&b", "a+b", "a,b", "a;b", "a=b", "a@b", "a[b]", "a^b", "a{b}"};
    const char *expected[] = {"file://a%20b", "file://a%23b", "file://a%24b", "file://a%25b", "file://a%26b", "file://a%2bb", "file://a%2cb", "file://a%3bb", "file://a%3db", "file://a%40b", "file://a%5bb%5d", "file://a%5eb", "file://a%7bb%7d"};
    for (int i = 0; i < 13; ++i) ExpectEscaped(inputs[i], expected[i]);
}

TEST(URLTest, BackslashesNormalizeToPathSeparators) {
    ExpectEscaped("a\\b", "file://a/b");
    ExpectEscaped("C:\\a b\\file.txt", "file://C:/a%20b/file.txt");
    ExpectEscaped("\\\\server\\a b", "file:////server/a%20b");
}

TEST(URLTest, CharactersOutsideTheOriginalTableArePreserved) {
    ExpectEscaped("a\tb\r\nc", "a\tb\r\nc"); ExpectEscaped("!'()*<>\"", "!'()*<>\"");
}

TEST(URLTest, RepeatedEscapesAdvanceWithoutLosingBytes) {
    ExpectEscaped("a b c#d$e%f", "file://a%20b%20c%23d%24e%25f");
    ExpectEscaped("%%%", "file://%25%25%25");
}

TEST(URLTest, EscapingPercentDoesNotInterpretAnExistingEscape) {
    ExpectEscaped("http://host/%2f", "http://host/%252f");
}

TEST(URLTest, OutputCanOwnTheInputBuffer) {
    XString value("http://host/a b"); ASSERT_EQ(0u, VxEscapeURL(value.CStr(), value));
    EXPECT_STREQ("http://host/a%20b", value.CStr());
}

TEST(URLTest, OutputCanOwnAnInteriorInputPointer) {
    XString value("prefix:a b"); ASSERT_EQ(0u, VxEscapeURL(value.CStr() + 7, value));
    EXPECT_STREQ("file://a%20b", value.CStr());
}

TEST(URLTest, MaximumLengthPlainInputIsNotTruncatedOrPrefixed) {
    XAP<char> input(new char[XString::MAX_LENGTH + 1]);
    memset(input, 'x', XString::MAX_LENGTH); input[XString::MAX_LENGTH] = 0;
    XString output; ASSERT_EQ(0u, VxEscapeURL(input, output));
    EXPECT_EQ(XString::MAX_LENGTH, output.Length());
    EXPECT_EQ(0, memcmp(output.CStr(), input, XString::MAX_LENGTH + 1));
}

TEST(URLTest, EncodedOutputCanFillTheEntireStringCapacity) {
    const int spaces = 21842;
    XAP<char> input(new char[spaces + 2]); memset(input, ' ', spaces); input[spaces] = 'a'; input[spaces + 1] = 0;
    XString output; ASSERT_EQ(0u, VxEscapeURL(input, output));
    EXPECT_EQ(XString::MAX_LENGTH, output.Length());
    EXPECT_EQ(0, memcmp(output.CStr(), "file://%20", 10));
    EXPECT_STREQ("%20a", output.CStr() + output.Length() - 4);
}

TEST(URLTest, OversizedEncodedOutputFailsWithoutPublishingAPrefix) {
    const int spaces = 21843;
    XAP<char> input(new char[spaces + 1]); memset(input, ' ', spaces); input[spaces] = 0;
    XString output("keep"); EXPECT_EQ(static_cast<XDWORD>(-1), VxEscapeURL(input, output));
    EXPECT_STREQ("keep", output.CStr());
}

TEST(URLTest, OversizedPlainInputFailsWithoutTruncatingTheOutput) {
    XAP<char> input(new char[XString::MAX_LENGTH + 2]);
    memset(input, 'x', XString::MAX_LENGTH + 1); input[XString::MAX_LENGTH + 1] = 0;
    XString output("keep"); EXPECT_EQ(static_cast<XDWORD>(-1), VxEscapeURL(input, output));
    EXPECT_STREQ("keep", output.CStr());
}

#ifdef _WIN32
TEST(URLTest, DbcsTrailBackslashIsPartOfTheNativeCharacter) {
    char native[16] = {}; BOOL substituted = FALSE;
    int length = WideCharToMultiByte(GetACP(), GetACP() == CP_UTF8 ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS,
        L"\u4E57", -1, native, sizeof(native), nullptr, GetACP() == CP_UTF8 ? nullptr : &substituted);
    if (!length || substituted) { SUCCEED() << "Character is unavailable in this ANSI code page"; return; }
    ExpectEscaped(native, native);
    XString input = XString(native) + " path";
    XString expected = XString("file://") + native + "%20path";
    ExpectEscaped(input.CStr(), expected.CStr());
    printf("URLNativeACP: %u; Bytes:", GetACP());
    for (int i = 0; i < length - 1; ++i) printf("%02x", static_cast<unsigned char>(native[i]));
    printf("\n");
}
#endif
