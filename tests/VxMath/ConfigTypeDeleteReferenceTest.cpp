#include "VxConfiguration.h"
#include "VxWindowFunctions.h"
#include "XP.h"
#include <gtest/gtest.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

static const VxConfig::Mode TypeReadWrite = static_cast<VxConfig::Mode>(VxConfig::READ | VxConfig::WRITE);

class ConfigTypeDeleteReferenceTest : public ::testing::Test {
protected:
    VxConfig Config;
    XString Home, PreviousHome;
    char File[2048] = {};
    bool HadHome = false, ChangedHome = false, OwnFile = false;

    void SetUp() override {
        HadHome = VxGetEnvironmentVariable("BALLANCE_USER_CONFIG_HOME", PreviousHome) != FALSE;
        Home = VxGetCurrentDirectory(); Home += "/config-type-delete-home";
        ASSERT_TRUE(VxSetEnvironmentVariable("BALLANCE_USER_CONFIG_HOME", Home.CStr()));
        ChangedHome = true;
        ASSERT_TRUE(VxConfig::GetRegistryFilePath(File, sizeof(File)));
        FILE *existing = fopen(File, "rb");
        if (existing) { fclose(existing); FAIL() << "Fixture file already exists"; }
        OwnFile = true;
        Config.OpenSection("values", TypeReadWrite);
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

    void ExpectText(const char *key, const char *expected) {
        char output[256] = {};
        ASSERT_EQ(strlen(expected) + 1, Config.ReadStringEntry(key, output));
        EXPECT_STREQ(expected, output);
    }

    void ExpectType(const char *key, VxConfigValueType expected) {
        VxConfigValueType type = VXCONFIG_VALUE_STRING;
        ASSERT_TRUE(Config.GetEntryType(key, &type));
        EXPECT_EQ(expected, type);
    }

    void WriteFile(const char *text) {
        FILE *file = fopen(File, "wb"); ASSERT_NE(nullptr, file);
        size_t length = strlen(text), written = fwrite(text, 1, length, file);
        EXPECT_EQ(0, fclose(file)); ASSERT_EQ(length, written);
    }

    XString ReadFile() {
        FILE *file = fopen(File, "rb");
        if (!file) { ADD_FAILURE(); return XString(); }
        XString result; char line[512];
        while (fgets(line, sizeof(line), file)) result << line;
        EXPECT_EQ(0, fclose(file)); return result;
    }
};

TEST_F(ConfigTypeDeleteReferenceTest, IntegersUseDecimalTextAndPreserveTheFullSignedRange) {
    const int values[] = {INT_MIN, -37, 0, 42, INT_MAX};
    const char *texts[] = {"-2147483648", "-37", "0", "42", "2147483647"};
    for (int i = 0; i < 5; ++i) {
        Config.WriteIntegerEntry("integer", values[i]); ExpectText("integer", texts[i]);
        int value = 123; ASSERT_TRUE(Config.ReadIntegerEntry("integer", &value));
        EXPECT_EQ(values[i], value); ExpectType("integer", VXCONFIG_VALUE_INTEGER);
    }
}

TEST_F(ConfigTypeDeleteReferenceTest, FiniteFloatTextRetainsBinary32Precision) {
    const unsigned int bits[] = {0, 0x80000000u, 0x3f800001u, 0x3eaaaaabu, 0x00800000u, 0x7f7fffffu, 0xff7fffffu};
    for (unsigned int input : bits) {
        float value; memcpy(&value, &input, sizeof(value));
        Config.WriteFloatEntry("float", value);
        float output = 0; ASSERT_TRUE(Config.ReadFloatEntry("float", &output));
        unsigned int actual; memcpy(&actual, &output, sizeof(actual));
        EXPECT_EQ(input, actual); ExpectType("float", VXCONFIG_VALUE_FLOAT);
    }
}

TEST_F(ConfigTypeDeleteReferenceTest, BooleanWritesNormalizeAnyNonzeroInput) {
    const int inputs[] = {0, 1, -3, INT_MIN, INT_MAX};
    for (int input : inputs) {
        Config.WriteBooleanEntry("boolean", input); ExpectText("boolean", input ? "1" : "0");
        XBOOL value = 99; ASSERT_TRUE(Config.ReadBooleanEntry("boolean", &value));
        EXPECT_EQ(input ? TRUE : FALSE, value); ExpectType("boolean", VXCONFIG_VALUE_BOOLEAN);
    }
}

TEST_F(ConfigTypeDeleteReferenceTest, NumericReadersKeepDecimalPrefixConversion) {
    Config.WriteStringEntry("value", " \t-37suffix");
    int integer = 0; ASSERT_TRUE(Config.ReadIntegerEntry("value", &integer)); EXPECT_EQ(-37, integer);
    Config.WriteStringEntry("value", "0x10");
    ASSERT_TRUE(Config.ReadIntegerEntry("value", &integer)); EXPECT_EQ(0, integer);
    Config.WriteStringEntry("value", " \n-1.25e2suffix");
    float real = 0; ASSERT_TRUE(Config.ReadFloatEntry("value", &real)); EXPECT_EQ(-125.0f, real);
    XBOOL boolean = FALSE; ASSERT_TRUE(Config.ReadBooleanEntry("value", &boolean)); EXPECT_EQ(TRUE, boolean);
}

TEST_F(ConfigTypeDeleteReferenceTest, IntegerReadsDoNotLoseLongLeadingWhitespace) {
    XString text; for (int i = 0; i < 300; ++i) text << ' ';
    text << "-27"; Config.WriteStringEntry("value", text.CStr());
    int value = 0; ASSERT_TRUE(Config.ReadIntegerEntry("value", &value)); EXPECT_EQ(-27, value);
}

TEST_F(ConfigTypeDeleteReferenceTest, IntegerReadsDoNotConvertATruncatedZeroPrefix) {
    XString text; for (int i = 0; i < 300; ++i) text << '0';
    text << "47"; Config.WriteStringEntry("value", text.CStr());
    int value = 0; ASSERT_TRUE(Config.ReadIntegerEntry("value", &value)); EXPECT_EQ(47, value);
}

TEST_F(ConfigTypeDeleteReferenceTest, FloatReadsKeepTheExponentAfterALongMantissa) {
    XString text("1."); for (int i = 0; i < 300; ++i) text << '0';
    text << "e20"; Config.WriteStringEntry("value", text.CStr());
    float value = 0; ASSERT_TRUE(Config.ReadFloatEntry("value", &value)); EXPECT_EQ(1e20f, value);
}

TEST_F(ConfigTypeDeleteReferenceTest, BooleanReadsUseTheCompleteIntegerText) {
    XString text; for (int i = 0; i < 300; ++i) text << '0';
    text << '1'; Config.WriteStringEntry("value", text.CStr());
    XBOOL value = FALSE; ASSERT_TRUE(Config.ReadBooleanEntry("value", &value)); EXPECT_EQ(TRUE, value);
}

TEST_F(ConfigTypeDeleteReferenceTest, IntegerOverflowFailsWithoutPublishingAWrappedValue) {
    const char *texts[] = {"2147483648", "-2147483649", "9999999999999999999999999999999999"};
    for (const char *text : texts) {
        Config.WriteStringEntry("value", text);
        int value = 73; EXPECT_FALSE(Config.ReadIntegerEntry("value", &value)); EXPECT_EQ(73, value);
        XBOOL boolean = 73; EXPECT_FALSE(Config.ReadBooleanEntry("value", &boolean)); EXPECT_EQ(73, boolean);
    }
}

TEST_F(ConfigTypeDeleteReferenceTest, MissingInvalidAndNullOutputsFailWithoutChangingValues) {
    const char *keys[] = {"missing", "invalid"}; Config.WriteStringEntry("invalid", "not a number");
    for (const char *key : keys) {
        int integer = 31; float real = 3.5f; XBOOL boolean = 17;
        EXPECT_FALSE(Config.ReadIntegerEntry(key, &integer)); EXPECT_EQ(31, integer);
        EXPECT_FALSE(Config.ReadFloatEntry(key, &real)); EXPECT_EQ(3.5f, real);
        EXPECT_FALSE(Config.ReadBooleanEntry(key, &boolean)); EXPECT_EQ(17, boolean);
    }
    EXPECT_FALSE(Config.ReadIntegerEntry("invalid", nullptr));
    EXPECT_FALSE(Config.ReadFloatEntry("invalid", nullptr));
    EXPECT_FALSE(Config.ReadBooleanEntry("invalid", nullptr));
}

TEST_F(ConfigTypeDeleteReferenceTest, AllTypedMethodsUseTheSameDefaultValueName) {
    Config.WriteIntegerEntry(nullptr, 42); ExpectText("", "42"); ExpectType(nullptr, VXCONFIG_VALUE_INTEGER);
    int integer = 0; ASSERT_TRUE(Config.ReadIntegerEntry(nullptr, &integer)); EXPECT_EQ(42, integer);
    Config.WriteFloatEntry(nullptr, -2.5f); float real = 0;
    ASSERT_TRUE(Config.ReadFloatEntry(nullptr, &real)); EXPECT_EQ(-2.5f, real);
    Config.WriteBooleanEntry(nullptr, -7); XBOOL boolean = FALSE;
    ASSERT_TRUE(Config.ReadBooleanEntry(nullptr, &boolean)); EXPECT_EQ(TRUE, boolean);
    EXPECT_TRUE(Config.EntryExists(nullptr)); EXPECT_TRUE(Config.DeleteEntry(nullptr));
    EXPECT_FALSE(Config.EntryExists(""));
}

TEST_F(ConfigTypeDeleteReferenceTest, SizedStringReadSupportsDefaultNameAndGuardsItsBoundary) {
    Config.WriteStringEntry(nullptr, "abcdef");
    char output[6]; memset(output, 0x5a, sizeof(output));
    ASSERT_TRUE(Config.ReadStringEntry(nullptr, output, 4));
    EXPECT_EQ(0, memcmp(output, "abc\0ZZ", 6));
    EXPECT_FALSE(Config.ReadStringEntry("", output, 0));
    EXPECT_FALSE(Config.ReadStringEntry("", nullptr, 4));
}

TEST_F(ConfigTypeDeleteReferenceTest, TypesCanBeOverwrittenAndAreNotConversionGates) {
    Config.WriteIntegerEntry("key", 12); float real = 0;
    ASSERT_TRUE(Config.ReadFloatEntry("KEY", &real)); EXPECT_EQ(12.0f, real);
    Config.WriteFloatEntry("KEY", 3.5f); ExpectType("key", VXCONFIG_VALUE_FLOAT);
    int integer = 0; ASSERT_TRUE(Config.ReadIntegerEntry("key", &integer)); EXPECT_EQ(3, integer);
    Config.WriteBooleanEntry("key", TRUE); ExpectType("key", VXCONFIG_VALUE_BOOLEAN);
    Config.WriteStringEntry("key", "literal"); ExpectType("key", VXCONFIG_VALUE_STRING);
    EXPECT_TRUE(Config.GetEntryType("key", nullptr));
}

TEST_F(ConfigTypeDeleteReferenceTest, OldAndUnknownTypeMetadataFallsBackToString) {
    VxConfiguration file;
    const char *section = "CurrentUser.Software.Virtools.UserConfig.values";
    file.AddEntry(section, "old", "27"); file.AddEntry(section, "unknown", "42");
    file.AddEntry(section, "unknown.type", "future"); ASSERT_TRUE(file.SaveToFile(File));
    ExpectType("old", VXCONFIG_VALUE_STRING); ExpectType("unknown", VXCONFIG_VALUE_STRING);
    int integer = 0; ASSERT_TRUE(Config.ReadIntegerEntry("old", &integer)); EXPECT_EQ(27, integer);
}

TEST_F(ConfigTypeDeleteReferenceTest, MetadataQueriesDoNotPublishATypeForMissingEntries) {
    VxConfigValueType type = VXCONFIG_VALUE_FLOAT;
    EXPECT_FALSE(Config.GetEntryType("missing", &type)); EXPECT_EQ(VXCONFIG_VALUE_FLOAT, type);
    EXPECT_FALSE(Config.EntryExists("missing"));
    Config.WriteStringEntry("empty", ""); EXPECT_TRUE(Config.EntryExists("empty"));
}

TEST_F(ConfigTypeDeleteReferenceTest, ReadOnlySelectionRejectsAllTypedWritesAndEntryDeletion) {
    Config.WriteIntegerEntry("key", 37); Config.OpenSection("values", VxConfig::READ);
    Config.WriteIntegerEntry("key", 1); Config.WriteFloatEntry("key", 2); Config.WriteBooleanEntry("key", FALSE);
    EXPECT_FALSE(Config.DeleteEntry("key")); ExpectText("key", "37");
}

TEST_F(ConfigTypeDeleteReferenceTest, WriteOnlyAndClosedSelectionsRejectQueries) {
    Config.WriteIntegerEntry("key", 37);
    const VxConfig::Mode modes[] = {VxConfig::WRITE, static_cast<VxConfig::Mode>(0)};
    for (VxConfig::Mode mode : modes) {
        if (mode) Config.OpenSection("values", mode); else Config.CloseSection(nullptr);
        int value = 17; EXPECT_FALSE(Config.ReadIntegerEntry("key", &value)); EXPECT_EQ(17, value);
        EXPECT_FALSE(Config.EntryExists("key")); EXPECT_FALSE(Config.GetEntryType("key", nullptr));
    }
}

TEST_F(ConfigTypeDeleteReferenceTest, EntryDeletionRemovesOnlyTheNamedValueAndItsMetadata) {
    Config.WriteIntegerEntry("MiXeD", 7); Config.WriteStringEntry("mixed.type", "public");
    EXPECT_TRUE(Config.DeleteEntry("MIXED")); EXPECT_FALSE(Config.DeleteEntry("mixed"));
    EXPECT_FALSE(Config.EntryExists("mixed")); ExpectText("mixed.type", "public");
}

TEST_F(ConfigTypeDeleteReferenceTest, NullSectionDeletionRequiresAnOpenWritableSelection) {
    Config.WriteStringEntry("key", "keep"); Config.OpenSection("values", VxConfig::READ);
    EXPECT_FALSE(Config.DeleteSection(nullptr)); ExpectText("key", "keep");
    Config.CloseSection(nullptr); EXPECT_FALSE(Config.DeleteSection(nullptr));
    Config.OpenSection("values", VxConfig::READ); ExpectText("key", "keep");
}

TEST_F(ConfigTypeDeleteReferenceTest, DeletingTheCurrentSectionClosesItAndDoesNotRecreateIt) {
    Config.WriteStringEntry("key", "value"); ASSERT_TRUE(Config.DeleteSection(nullptr));
    Config.WriteIntegerEntry("ghost", 42); EXPECT_FALSE(Config.EntryExists("ghost"));
    VxConfiguration file; int line = 0; XString error; ASSERT_TRUE(file.BuildFromFile(File, line, error));
    EXPECT_EQ(nullptr, file.GetSubSection("CurrentUser.Software.Virtools.UserConfig.values", TRUE));
}

TEST_F(ConfigTypeDeleteReferenceTest, DeletingAnAncestorClosesItsSelectedDescendant) {
    Config.OpenSection("parent\\child", TypeReadWrite); Config.WriteIntegerEntry("key", 7);
    ASSERT_TRUE(Config.DeleteSection("PARENT")); Config.WriteIntegerEntry("ghost", 42);
    EXPECT_FALSE(Config.EntryExists("ghost"));
    VxConfiguration file; int line = 0; XString error; ASSERT_TRUE(file.BuildFromFile(File, line, error));
    EXPECT_EQ(nullptr, file.GetSubSection("CurrentUser.Software.Virtools.UserConfig.parent", TRUE));
}

TEST_F(ConfigTypeDeleteReferenceTest, DeletingAnUnrelatedSectionPreservesTheCurrentSelection) {
    Config.OpenSection("other", TypeReadWrite); Config.WriteIntegerEntry("key", 3);
    Config.OpenSection("values", TypeReadWrite); Config.WriteIntegerEntry("key", 7);
    EXPECT_TRUE(Config.DeleteSection("OTHER")); EXPECT_FALSE(Config.DeleteSection("missing"));
    Config.WriteIntegerEntry("key", 9); ExpectText("key", "9");
}

TEST_F(ConfigTypeDeleteReferenceTest, ExplicitSectionDeletionWorksWithoutACurrentSelection) {
    Config.WriteIntegerEntry("key", 7); Config.CloseSection(nullptr);
    EXPECT_TRUE(Config.DeleteSection("VALUES")); EXPECT_FALSE(Config.DeleteSection("values"));
}

TEST_F(ConfigTypeDeleteReferenceTest, NullDeletionCanDeleteTheExplicitlySelectedBase) {
    Config.OpenSection("", TypeReadWrite); Config.WriteIntegerEntry("key", 7);
    EXPECT_TRUE(Config.DeleteSection(nullptr)); Config.WriteIntegerEntry("ghost", 9);
    EXPECT_FALSE(Config.EntryExists("ghost"));
}

TEST_F(ConfigTypeDeleteReferenceTest, MalformedFilesAreNotReplacedByWritesOrDeletion) {
    const char *text = "key=prefix\ninvalid\n"; WriteFile(text);
    Config.WriteIntegerEntry("key", 3); Config.WriteFloatEntry("key", 4); Config.WriteBooleanEntry("key", FALSE);
    EXPECT_FALSE(Config.DeleteEntry("key")); EXPECT_FALSE(Config.DeleteSection("values"));
    EXPECT_STREQ(text, ReadFile().CStr());
}

#ifdef _WIN32
TEST_F(ConfigTypeDeleteReferenceTest, SaveFailurePreservesTheFileAndCurrentSelection) {
    Config.WriteIntegerEntry("key", 7); XString before = ReadFile();
    DWORD attributes = GetFileAttributesA(File); ASSERT_NE(INVALID_FILE_ATTRIBUTES, attributes);
    ASSERT_TRUE(SetFileAttributesA(File, attributes | FILE_ATTRIBUTE_READONLY));
    Config.WriteFloatEntry("key", 4); EXPECT_FALSE(Config.DeleteEntry("key"));
    EXPECT_FALSE(Config.DeleteSection(nullptr));
    EXPECT_TRUE(SetFileAttributesA(File, attributes));
    EXPECT_STREQ(before.CStr(), ReadFile().CStr()); ExpectText("key", "7");
    Config.WriteIntegerEntry("key", 9); ExpectText("key", "9");
}
#endif
