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

TEST(URLTest, EmptyPathAcquiresTheFilePrefix) {
    ExpectEscaped("", "file://");
}

TEST(URLTest, PlainPathsAlwaysAcquireAFilePrefix) {
    const struct { const char *input; const char *expected; } cases[] = {
        {"plain", "file://plain"},
        {"/path/to/file", "file:///path/to/file"},
        {"C:/file.txt", "file://C:/file.txt"},
        {"relative/file.txt", "file://relative/file.txt"},
        {"a:b?c", "file://a:b?c"}
    };
    for (const auto &test : cases) ExpectEscaped(test.input, test.expected);
}

TEST(URLTest, FileUrlsEscapeRawContentAndPreserveSeparators) {
    ExpectEscaped("file:///a b", "file:///a%20b");
    ExpectEscaped("FILE:///a#b", "FILE:///a%23b");
    ExpectEscaped("file://C:\\a b", "file://C:/a%20b");
    ExpectEscaped("file:////server/share/a b", "file:////server/share/a%20b");
    ExpectEscaped("file:///a?b:c/d", "file:///a?b:c/d");
}

TEST(URLTest, FileUrlsPreserveExistingEscapesAlongsideRawSpaces) {
    ExpectEscaped("File://a%20b", "File://a%20b");
    ExpectEscaped("file:///a%2Fb c%5c%00", "file:///a%2Fb%20c%5c%00");
    ExpectEscaped("file:///a%2520b", "file:///a%2520b");
}

TEST(URLTest, FileUrlsEscapeMalformedAndIncompletePercentSequences) {
    const struct { const char *input; const char *expected; } cases[] = {
        {"file://%", "file://%25"},
        {"file://%2", "file://%252"},
        {"file://%GG", "file://%25GG"},
        {"file://%0g", "file://%250g"},
        {"file://%g0", "file://%25g0"},
        {"file://%%20", "file://%25%20"}
    };
    for (const auto &test : cases) ExpectEscaped(test.input, test.expected);
}

TEST(URLTest, RawPathsTreatPercentEscapesAsLiteralFileNameText) {
    ExpectEscaped("a%20b", "file://a%2520b");
    ExpectEscaped("C:/a%2fb%25c", "file://C:/a%252fb%2525c");
}

TEST(URLTest, ProtocolAndPathDelimitersArePreserved) {
    ExpectEscaped("http://host/path", "http://host/path");
    ExpectEscaped("https://host/a b?x=1&y=2", "https://host/a%20b?x%3d1%26y%3d2");
}

TEST(URLTest, ValidSchemesSuppressTheFilePrefix) {
    const struct { const char *input; const char *expected; } cases[] = {
        {"x://a b", "x://a%20b"},
        {"HTTP://host/a b", "HTTP://host/a%20b"},
        {"a1+.-://a b", "a1+.-://a%20b"}
    };
    for (const auto &test : cases) ExpectEscaped(test.input, test.expected);
}

TEST(URLTest, InvalidOrMisplacedSchemesRemainFilePaths) {
    const struct { const char *input; const char *expected; } cases[] = {
        {":// a", "file://://%20a"},
        {"1bad://a b", "file://1bad://a%20b"},
        {"+bad://a b", "file://%2bbad://a%20b"},
        {"a_b://a b", "file://a_b://a%20b"},
        {"a b://c", "file://a%20b://c"},
        {"dir/http://a b", "file://dir/http://a%20b"},
        {"x:", "file://x:"},
        {"x:/", "file://x:/"},
        {"x", "file://x"},
        {":", "file://:"},
        {":/", "file://:/"}
    };
    for (const auto &test : cases) ExpectEscaped(test.input, test.expected);
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
    ExpectEscaped("a\tb\r\nc", "file://a\tb\r\nc");
    ExpectEscaped("!'()*<>\"", "file://!'()*<>\"");
}

TEST(URLTest, RepeatedEscapesAdvanceWithoutLosingBytes) {
    ExpectEscaped("a b c#d$e%f", "file://a%20b%20c%23d%24e%25f");
    ExpectEscaped("%%%", "file://%25%25%25");
}

TEST(URLTest, EscapingPercentDoesNotInterpretAnExistingEscape) {
    ExpectEscaped("http://host/%2f", "http://host/%252f");
}

TEST(URLTest, RepeatedFileUrlEscapingDoesNotEncodeExistingEscapesAgain) {
    XString value;
    ASSERT_EQ(0u, VxEscapeURL("C:\\a b%2f\\#x", value));
    const char *expected = "file://C:/a%20b%252f/%23x";
    EXPECT_STREQ(expected, value.CStr());
    for (int i = 0; i < 3; ++i) {
        ASSERT_EQ(0u, VxEscapeURL(value.CStr(), value));
        EXPECT_STREQ(expected, value.CStr());
    }
}

TEST(URLTest, OutputCanOwnTheInputBuffer) {
    XString value("http://host/a b"); ASSERT_EQ(0u, VxEscapeURL(value.CStr(), value));
    EXPECT_STREQ("http://host/a%20b", value.CStr());
}

TEST(URLTest, OutputCanOwnAnInteriorInputPointer) {
    XString value("prefix:a b"); ASSERT_EQ(0u, VxEscapeURL(value.CStr() + 7, value));
    EXPECT_STREQ("file://a%20b", value.CStr());
}

TEST(URLTest, FileUrlOutputCanOwnTheInputAndAnInteriorPointer) {
    XString value("file:///a%2f b");
    ASSERT_EQ(0u, VxEscapeURL(value.CStr(), value));
    EXPECT_STREQ("file:///a%2f%20b", value.CStr());
    value = "prefix:file:///a%20b c";
    ASSERT_EQ(0u, VxEscapeURL(value.CStr() + 7, value));
    EXPECT_STREQ("file:///a%20b%20c", value.CStr());
}

TEST(URLTest, MaximumLengthExistingUrlIsNotTruncated) {
    XAP<char> input(new char[XString::MAX_LENGTH + 1]);
    memset(input, 'x', XString::MAX_LENGTH);
    memcpy(input, "http://", 7);
    input[XString::MAX_LENGTH] = 0;
    XString output; ASSERT_EQ(0u, VxEscapeURL(input, output));
    EXPECT_EQ(XString::MAX_LENGTH, output.Length());
    EXPECT_EQ(0, memcmp(output.CStr(), input, XString::MAX_LENGTH + 1));
}

TEST(URLTest, PlainPathCapacityIncludesTheFilePrefix) {
    const int length = XString::MAX_LENGTH - 7;
    XAP<char> input(new char[length + 2]);
    memset(input, 'x', length + 1);
    input[length] = 0;
    XString output;
    ASSERT_EQ(0u, VxEscapeURL(input, output));
    ASSERT_EQ(XString::MAX_LENGTH, output.Length());
    EXPECT_EQ(0, memcmp(output.CStr(), "file://", 7));
    EXPECT_EQ(0, memcmp(output.CStr() + 7, input, length + 1));

    input[length] = 'x';
    input[length + 1] = 0;
    output = "keep";
    EXPECT_EQ(static_cast<XDWORD>(-1), VxEscapeURL(input, output));
    EXPECT_STREQ("keep", output.CStr());
}

TEST(URLTest, FileUrlCapacityCountsNewEscapesButNotExistingEscapes) {
    XAP<char> input(new char[XString::MAX_LENGTH + 1]);
    memset(input, 'x', XString::MAX_LENGTH);
    memcpy(input, "file://%2F", 10);
    input[XString::MAX_LENGTH] = 0;
    XString output;
    ASSERT_EQ(0u, VxEscapeURL(input, output));
    EXPECT_EQ(XString::MAX_LENGTH, output.Length());
    EXPECT_EQ(0, memcmp(output.CStr(), input, XString::MAX_LENGTH + 1));

    input[XString::MAX_LENGTH - 1] = ' ';
    output = "keep";
    EXPECT_EQ(static_cast<XDWORD>(-1), VxEscapeURL(input, output));
    EXPECT_STREQ("keep", output.CStr());

    input[XString::MAX_LENGTH - 3] = ' ';
    input[XString::MAX_LENGTH - 2] = 0;
    ASSERT_EQ(0u, VxEscapeURL(input, output));
    EXPECT_EQ(XString::MAX_LENGTH, output.Length());
    EXPECT_STREQ("%20", output.CStr() + output.Length() - 3);
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

TEST(URLTest, UnescapeDecodesHexBytesRatherThanDecimalText) {
    XString value("%20%2f%2F%41%7f"); VxUnEscapeUrl(value);
    const char expected[] = {' ', '/', '/', 'A', 0x7f, 0};
    EXPECT_EQ(5u, value.Length()); EXPECT_EQ(0, memcmp(value.CStr(), expected, sizeof(expected)));
}

TEST(URLTest, UnescapePreservesMalformedAndIncompleteSequences) {
    const char *inputs[] = {"%", "%2", "%GG", "%g0", "%0g", "x%2", "end%", "%G0%20"};
    const char *expected[] = {"%", "%2", "%GG", "%g0", "%0g", "x%2", "end%", "%G0 "};
    for (int i = 0; i < 8; ++i) {
        XString value(inputs[i]); VxUnEscapeUrl(value); EXPECT_STREQ(expected[i], value.CStr());
    }
}

TEST(URLTest, UnescapePerformsOnePassAndDoesNotTranslatePlus) {
    XString value("%2520+a+b"); VxUnEscapeUrl(value); EXPECT_STREQ("%20+a+b", value.CStr());
}

TEST(URLTest, UnescapePreservesTheLengthOfDecodedZeroBytes) {
    XString value("A%00B"); VxUnEscapeUrl(value);
    const char expected[] = {'A', 0, 'B', 0};
    EXPECT_EQ(3u, value.Length()); EXPECT_EQ(0, memcmp(value.CStr(), expected, sizeof(expected)));
}

TEST(URLTest, UnescapeHandlesControlAndHighBytes) {
    XString value("%01%09%0a%0D%20%7f%80%ff"); VxUnEscapeUrl(value);
    const unsigned char expected[] = {1, 9, 10, 13, 32, 127, 128, 255, 0};
    EXPECT_EQ(8u, value.Length()); EXPECT_EQ(0, memcmp(value.CStr(), expected, sizeof(expected)));
}

TEST(URLTest, UnescapeCoversEveryByteWithBothHexCases) {
    for (int byte = 0; byte < 256; ++byte) {
        const char *formats[] = {"%%%02x", "%%%02X"};
        for (const char *format : formats) {
            char encoded[4]; snprintf(encoded, sizeof(encoded), format, byte);
            XString value(encoded); VxUnEscapeUrl(value);
            ASSERT_EQ(1u, value.Length());
            EXPECT_EQ(static_cast<unsigned char>(byte), static_cast<unsigned char>(value[0]));
            EXPECT_EQ(0, value.CStr()[1]);
        }
    }
}

TEST(URLTest, EmptyAndLiteralUnescapeInputsRemainUnchanged) {
    XString empty; VxUnEscapeUrl(empty); EXPECT_TRUE(empty.Empty());
    XString literal("http://host/path?a+b"); VxUnEscapeUrl(literal); EXPECT_STREQ("http://host/path?a+b", literal.CStr());
}

TEST(URLTest, MaximumLengthUnescapeInputRetainsAllLiteralBytes) {
    XString value; value.Reserve(XString::MAX_LENGTH);
    for (int i = 0; i < XString::MAX_LENGTH; ++i) value << 'x';
    VxUnEscapeUrl(value); ASSERT_EQ(XString::MAX_LENGTH, value.Length());
    for (int i = 0; i < value.Length(); ++i) ASSERT_EQ('x', value[i]);
}

TEST(URLTest, EscapeAndDecodeRetainDataAndNormalizeNativeSeparators) {
    XString value; ASSERT_EQ(0u, VxEscapeURL("C:\\a b\\file#x", value));
    VxUnEscapeUrl(value); EXPECT_STREQ("file://C:/a b/file#x", value.CStr());
}

#ifdef _WIN32
TEST(URLTest, DbcsTrailBackslashIsPartOfTheNativeCharacter) {
    char native[16] = {}; BOOL substituted = FALSE;
    int length = WideCharToMultiByte(GetACP(), GetACP() == CP_UTF8 ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS,
        L"\u4E57", -1, native, sizeof(native), nullptr, GetACP() == CP_UTF8 ? nullptr : &substituted);
    if (!length || substituted) { SUCCEED() << "Character is unavailable in this ANSI code page"; return; }
    XString prefixed = XString("file://") + native;
    ExpectEscaped(native, prefixed.CStr());
    ExpectEscaped(prefixed.CStr(), prefixed.CStr());
    XString input = XString(native) + " path";
    XString expected = XString("file://") + native + "%20path";
    ExpectEscaped(input.CStr(), expected.CStr());
    XString fileInput = XString("file://") + native + " path";
    ExpectEscaped(fileInput.CStr(), expected.CStr());
    printf("URLNativeACP: %u; Bytes:", GetACP());
    for (int i = 0; i < length - 1; ++i) printf("%02x", static_cast<unsigned char>(native[i]));
    printf("\n");
}
#endif
