#include "VxConfiguration.h"
#include "VxWindowFunctions.h"
#include "XP.h"
#include <gtest/gtest.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

static const VxConfig::Mode ConfigValueReadWrite = static_cast<VxConfig::Mode>(VxConfig::READ | VxConfig::WRITE);

class ConfigValueReferenceTest : public ::testing::Test {
protected:
    VxConfig Config;
    XString Home, PreviousHome;
    char File[2048] = {};
    bool HadHome = false, ChangedHome = false, OwnFile = false;

    void SetUp() override {
        HadHome = VxGetEnvironmentVariable("BALLANCE_USER_CONFIG_HOME", PreviousHome) != FALSE;
        Home = VxGetCurrentDirectory(); Home += "/config-value-reference-home";
        ASSERT_TRUE(VxSetEnvironmentVariable("BALLANCE_USER_CONFIG_HOME", Home.CStr()));
        ChangedHome = true;
        ASSERT_TRUE(VxConfig::GetRegistryFilePath(File, sizeof(File)));
        FILE *existing = fopen(File, "rb");
        if (existing) { fclose(existing); FAIL() << "Fixture file already exists"; }
        OwnFile = true;
        Config.OpenSection("values", ConfigValueReadWrite);
    }

    void TearDown() override {
        Config.CloseSection(nullptr);
        if (OwnFile) {
            remove(File);
            XString directory = Home + "/Ballance";
            VxRemoveDirectory(directory.CStr()); VxRemoveDirectory(Home.CStr());
        }
        if (ChangedHome)
            EXPECT_TRUE(VxSetEnvironmentVariable("BALLANCE_USER_CONFIG_HOME", HadHome ? PreviousHome.CStr() : nullptr));
    }

    void ExpectValue(const char *key, const char *expected) {
        size_t size = strlen(expected) + 1;
        EXPECT_EQ(size, Config.ReadStringEntry(key, nullptr));
        XAP<char> output(new char[size + 16]);
        memset(output, 0xa5, size + 16);
        ASSERT_TRUE(Config.ReadStringEntry(key, output, size));
        EXPECT_EQ(0, memcmp(output, expected, size));
        for (int i = 0; i < 16; ++i) EXPECT_EQ(0xa5, static_cast<unsigned char>(output[size + i]));
        char legacy[272]; memset(legacy, 0xa5, sizeof(legacy));
        XDWORD result = Config.ReadStringEntry(key, legacy);
        EXPECT_EQ(size <= 256 ? static_cast<XDWORD>(size) : static_cast<XDWORD>(-1), result);
        if (size <= 256) EXPECT_EQ(0, memcmp(legacy, expected, size));
        for (int i = 256; i < 272; ++i) EXPECT_EQ(0xa5, static_cast<unsigned char>(legacy[i]));
    }

    void StoreAndCheck(const char *value) {
        Config.WriteStringEntry("value", value);
        ExpectValue("value", value);
    }

    void WriteRawValue(const char *value, const char *type, const char *tail = nullptr) {
        VxConfiguration file;
        file.AddEntry("CurrentUser.Software.Virtools.UserConfig.values", "value", value);
        if (type) file.AddEntry("CurrentUser.Software.Virtools.UserConfig.values", "value.type", type);
        if (tail) file.AddEntry("CurrentUser.Software.Virtools.UserConfig.values", "value.tail", tail);
        ASSERT_TRUE(file.SaveToFile(File));
    }

    VxConfigurationSection *LoadValues(VxConfiguration &file) {
        int line = 0; XString error;
        if (!file.BuildFromFile(File, line, error)) { ADD_FAILURE() << error.CStr(); return nullptr; }
        return file.GetSubSection("CurrentUser.Software.Virtools.UserConfig.values", TRUE);
    }
};

TEST_F(ConfigValueReferenceTest, LeadingAndTrailingSpacesArePreserved) {
    const char *values[] = {" leading", "trailing ", "  wrapped  ", " ", "    "};
    for (const char *value : values) StoreAndCheck(value);
}

TEST_F(ConfigValueReferenceTest, TabsAndWhitespaceOnlyValuesArePreserved) {
    const char *values[] = {"\t", "\ttext\t", " \t  ", "text\tinside", "\t \t"};
    for (const char *value : values) StoreAndCheck(value);
}

TEST_F(ConfigValueReferenceTest, LineFeedsAndBlankLinesRemainPartOfTheValue) {
    const char *values[] = {"first\nsecond", "\nfirst\n", "a\n\nb", "\n", "\n\n"};
    for (const char *value : values) StoreAndCheck(value);
}

TEST_F(ConfigValueReferenceTest, CarriageReturnsAndCRLFAreNotNormalized) {
    const char *values[] = {"first\rsecond", "first\r\nsecond", "\r\n", "a\r\nb\nc\r"};
    for (const char *value : values) StoreAndCheck(value);
}

TEST_F(ConfigValueReferenceTest, LineContentCannotInjectSectionsOrOverwriteOtherEntries) {
    Config.WriteStringEntry("other", "unchanged");
    StoreAndCheck("first\n#literal\n;literal\n<injected>\nother=wrong\n</injected>\n");
    ExpectValue("other", "unchanged");
    VxConfiguration file;
    VxConfigurationSection *values = LoadValues(file);
    ASSERT_NE(nullptr, values);
    EXPECT_EQ(0u, values->GetNumberOfSubSections());
}

TEST_F(ConfigValueReferenceTest, EveryNonzeroAsciiControlByteIsPreserved) {
    char value[33];
    for (int i = 1; i <= 31; ++i) value[i - 1] = static_cast<char>(i);
    value[31] = 0x7f; value[32] = 0;
    StoreAndCheck(value);
}

TEST_F(ConfigValueReferenceTest, LiteralPrefixesQuotesAndEscapesStayLiteral) {
    const char *values[] = {"string.hex", "41420A", "hex:4142", "\\n is literal", "\"quoted\" \\literal", "#;=<>%"};
    for (const char *value : values) StoreAndCheck(value);
}

TEST_F(ConfigValueReferenceTest, OrdinaryValuesRemainReadableInTheExistingTextFormat) {
    StoreAndCheck("ordinary value");
    VxConfiguration file;
    VxConfigurationSection *values = LoadValues(file);
    ASSERT_NE(nullptr, values);
    EXPECT_STREQ("ordinary value", values->GetEntry("value")->GetValue());
    EXPECT_STREQ("string", values->GetEntry("value.type")->GetValue());
    EXPECT_EQ(nullptr, values->GetEntry("value.tail"));
}

TEST_F(ConfigValueReferenceTest, ExistingUnmarkedValuesAreNotInterpretedAsEncodedData) {
    WriteRawValue("41420A", "string"); ExpectValue("value", "41420A");
    WriteRawValue("string.hex", "string"); ExpectValue("value", "string.hex");
    WriteRawValue("\\n literal", "string"); ExpectValue("value", "\\n literal");
    WriteRawValue("41420A", nullptr); ExpectValue("value", "41420A");
}

TEST_F(ConfigValueReferenceTest, MarkedHexDecodesIndependentStoredBytes) {
    WriteRawValue("200a41", "string.hex", "0d20");
    ExpectValue("value", " \nA\r ");
}

TEST_F(ConfigValueReferenceTest, MultilineValuesSurviveCloseAndAnotherInstance) {
    const char *value = " \tfirst\r\nsecond\n\nlast \t";
    Config.WriteStringEntry("value", value); Config.CloseSection(nullptr);
    VxConfig other;
    other.OpenSection("VALUES", VxConfig::READ);
    char output[256] = {};
    ASSERT_EQ(strlen(value) + 1, other.ReadStringEntry("VALUE", output));
    EXPECT_STREQ(value, output); other.CloseSection(nullptr);
    Config.OpenSection("values", VxConfig::READ); ExpectValue("value", value);
}

TEST_F(ConfigValueReferenceTest, SizedReadTruncatesDecodedBytesAndTerminatesTheOutput) {
    Config.WriteStringEntry("value", " \nabc\r\n ");
    char output[4] = {};
    ASSERT_TRUE(Config.ReadStringEntry("value", output, sizeof(output)));
    EXPECT_EQ(0, memcmp(output, " \na", 3)); EXPECT_EQ(0, output[3]);
}

TEST_F(ConfigValueReferenceTest, LegacyCapacityAppliesToDecodedPayloadRatherThanItsFileEncoding) {
    char value[257]; memset(value, 'x', 255); value[0] = ' '; value[254] = '\n'; value[255] = 0;
    StoreAndCheck(value);
    value[255] = '\n'; value[256] = 0; StoreAndCheck(value);
}

TEST_F(ConfigValueReferenceTest, PlainAndEncodedValuesKeepTheFullExistingStringCapacity) {
    const int sizes[] = {32767, 32768, XString::MAX_LENGTH};
    for (int size : sizes) {
        XAP<char> value(new char[size + 1]);
        memset(value, 'x', size); value[0] = ' '; value[size - 1] = '\n'; value[size] = 0;
        StoreAndCheck(value);
    }
    XAP<char> plain(new char[XString::MAX_LENGTH + 1]);
    memset(plain, 'x', XString::MAX_LENGTH); plain[XString::MAX_LENGTH] = 0;
    StoreAndCheck(plain);
}

TEST_F(ConfigValueReferenceTest, ShorterAndPlainOverwritesRemoveOldContinuationData) {
    XAP<char> value(new char[40001]); memset(value, 'x', 40000); value[0] = '\n'; value[40000] = 0;
    StoreAndCheck(value); StoreAndCheck(" \nshort "); StoreAndCheck("plain");
    VxConfiguration file;
    VxConfigurationSection *values = LoadValues(file);
    ASSERT_NE(nullptr, values);
    EXPECT_EQ(nullptr, values->GetEntry("value.tail"));
    EXPECT_STREQ("string", values->GetEntry("value.type")->GetValue());
}

TEST_F(ConfigValueReferenceTest, TypeMetadataStaysStringAndNumericOverwriteClearsEncoding) {
    StoreAndCheck(" \n42\r\n ");
    VxConfigValueType type = VXCONFIG_VALUE_INTEGER;
    EXPECT_TRUE(Config.GetEntryType("value", &type)); EXPECT_EQ(VXCONFIG_VALUE_STRING, type);
    int number = 0; EXPECT_TRUE(Config.ReadIntegerEntry("value", &number)); EXPECT_EQ(42, number);
    Config.WriteIntegerEntry("value", 37); ExpectValue("value", "37");
    EXPECT_TRUE(Config.GetEntryType("value", &type)); EXPECT_EQ(VXCONFIG_VALUE_INTEGER, type);
}

TEST_F(ConfigValueReferenceTest, DefaultValueAndCaseInsensitiveNamesUseTheSameRepresentation) {
    Config.WriteStringEntry(nullptr, " \ndefault\r\n "); ExpectValue("", " \ndefault\r\n ");
    Config.WriteStringEntry("MiXeD", " \nfirst "); Config.WriteStringEntry("mixed", " \nreplacement ");
    ExpectValue("MIXED", " \nreplacement ");
}

TEST_F(ConfigValueReferenceTest, MetadataSuffixesCannotAliasPublicValueNames) {
    Config.WriteStringEntry("value.tail", "public tail"); Config.WriteStringEntry("value.type", "public type");
    XAP<char> value(new char[40001]); memset(value, '\n', 40000); value[40000] = 0;
    StoreAndCheck(value);
    ExpectValue("value.tail", "public tail"); ExpectValue("value.type", "public type");
    EXPECT_TRUE(Config.DeleteEntry("VALUE"));
    ExpectValue("value.tail", "public tail"); ExpectValue("value.type", "public type");
    VxConfiguration file; VxConfigurationSection *values = LoadValues(file); ASSERT_NE(nullptr, values);
    EXPECT_EQ(nullptr, values->GetEntry("value")); EXPECT_EQ(nullptr, values->GetEntry("value.type"));
    EXPECT_EQ(nullptr, values->GetEntry("value.tail"));
}

TEST_F(ConfigValueReferenceTest, InvalidHexAndEmbeddedZeroFailWithoutChangingCallerOutputs) {
    const char *invalid[] = {"0", "GG", "00", "410042"};
    for (const char *text : invalid) {
        WriteRawValue(text, "string.hex");
        char output[256]; memset(output, 0x5a, sizeof(output));
        EXPECT_EQ(static_cast<XDWORD>(-1), Config.ReadStringEntry("value", output));
        EXPECT_EQ(static_cast<XDWORD>(-1), Config.ReadStringEntry("value", nullptr));
        EXPECT_FALSE(Config.ReadStringEntry("value", output, sizeof(output)));
        for (char c : output) EXPECT_EQ(0x5a, static_cast<unsigned char>(c));
    }
}

TEST_F(ConfigValueReferenceTest, InvalidContinuationIsRejectedRatherThanReturningAPrefix) {
    WriteRawValue("410A", "string.hex", "broken");
    EXPECT_EQ(static_cast<XDWORD>(-1), Config.ReadStringEntry("value", nullptr));
}

TEST_F(ConfigValueReferenceTest, InputsBeyondTheStringLimitDoNotReplaceTheExistingValue) {
    StoreAndCheck("keep");
    const int size = XString::MAX_LENGTH + 1;
    XAP<char> value(new char[size + 1]); memset(value, 'x', size); value[size] = 0;
    Config.WriteStringEntry("value", value); ExpectValue("value", "keep");
    value[0] = '\n'; Config.WriteStringEntry("value", value); ExpectValue("value", "keep");
}

TEST_F(ConfigValueReferenceTest, CStringInputStopsAtItsFirstTerminator) {
    const char value[] = {'a', 0, 'b', 0};
    Config.WriteStringEntry("value", value); ExpectValue("value", "a");
}

#ifdef _WIN32
TEST_F(ConfigValueReferenceTest, AnsiCharactersSurviveWhitespaceAndAContinuationBoundary) {
    char native[16] = {}; BOOL substituted = FALSE;
    int length = WideCharToMultiByte(GetACP(), GetACP() == CP_UTF8 ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS,
        L"\u4E2D", -1, native, sizeof(native), nullptr, GetACP() == CP_UTF8 ? nullptr : &substituted);
    if (!length || substituted) { SUCCEED() << "Character is not representable in the active ANSI code page"; return; }
    XString shortValue = XString(" \t") + native + "\r\n ";
    StoreAndCheck(shortValue.CStr());
    const int size = 32767 + length;
    XAP<char> value(new char[size + 1]); memset(value, 'x', size);
    value[0] = ' '; memcpy(static_cast<char *>(value) + 32766, native, length - 1);
    value[size - 1] = '\n'; value[size] = 0;
    StoreAndCheck(value);
}
#endif
