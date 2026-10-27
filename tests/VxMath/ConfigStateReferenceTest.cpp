#include "VxConfiguration.h"
#include "VxWindowFunctions.h"
#include <gtest/gtest.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

static const VxConfig::Mode ConfigReadWrite = static_cast<VxConfig::Mode>(VxConfig::READ | VxConfig::WRITE);

class ConfigStateReferenceTest : public ::testing::Test {
protected:
    VxConfig Config;
    XString Home, PreviousHome;
    char File[2048] = {};
    bool HadHome = false, ChangedHome = false, OwnFile = false;

    void SetUp() override {
        HadHome = VxGetEnvironmentVariable("BALLANCE_USER_CONFIG_HOME", PreviousHome) != FALSE;
        Home = VxGetCurrentDirectory();
        Home += "/config-state-reference-home";
        ASSERT_TRUE(VxSetEnvironmentVariable("BALLANCE_USER_CONFIG_HOME", Home.CStr()));
        ChangedHome = true;
        ASSERT_TRUE(VxConfig::GetRegistryFilePath(File, sizeof(File)));
        FILE *existing = fopen(File, "rb");
        if (existing) { fclose(existing); FAIL() << "Fixture file already exists"; }
        OwnFile = true;
    }

    void TearDown() override {
        Config.CloseSection(nullptr);
        if (OwnFile) {
            remove(File);
            XString directory = Home + "/Ballance";
            VxRemoveDirectory(directory.CStr());
            VxRemoveDirectory(Home.CStr());
        }
        if (ChangedHome)
            EXPECT_TRUE(VxSetEnvironmentVariable("BALLANCE_USER_CONFIG_HOME", HadHome ? PreviousHome.CStr() : nullptr));
    }

    void Store(const char *section, const char *key, const char *value) {
        Config.OpenSection(section, VxConfig::WRITE);
        Config.WriteStringEntry(key, value);
        Config.CloseSection(section);
    }

    void ExpectValue(const char *key, const char *expected) {
        char value[256] = {};
        ASSERT_EQ(strlen(expected) + 1, Config.ReadStringEntry(key, value));
        EXPECT_STREQ(expected, value);
    }

    void WriteFile(const char *text) {
        FILE *file = fopen(File, "wb");
        ASSERT_NE(nullptr, file);
        EXPECT_EQ(strlen(text), fwrite(text, 1, strlen(text), file));
        EXPECT_EQ(0, fclose(file));
    }

    XString ReadFile() {
        FILE *file = fopen(File, "rb");
        if (!file) { ADD_FAILURE() << "Cannot read fixture"; return ""; }
        char text[4096];
        size_t length = fread(text, 1, sizeof(text) - 1, file);
        text[length] = 0;
        EXPECT_EQ(0, ferror(file)); EXPECT_EQ(0, fclose(file));
        return text;
    }
};

TEST_F(ConfigStateReferenceTest, UnopenedConfigurationCannotReadOrWriteTheBaseSection) {
    Config.WriteStringEntry("value", "unopened");
    char output[256] = {};
    EXPECT_EQ(static_cast<XDWORD>(-1), Config.ReadStringEntry("value", output));
    EXPECT_EQ(static_cast<XDWORD>(-1), Config.ReadStringEntry("value", nullptr));
    EXPECT_FALSE(VxFileExists(File));
}

TEST_F(ConfigStateReferenceTest, ReadModeCannotChangeExistingValuesOrInsertNewOnes) {
    Store("mode", "value", "first");
    Config.OpenSection("mode", VxConfig::READ);
    Config.WriteStringEntry("value", "denied");
    Config.WriteIntegerEntry("new", 17);
    EXPECT_FALSE(Config.DeleteEntry("value"));
    ExpectValue("value", "first");
    EXPECT_FALSE(Config.EntryExists("new"));
}

TEST_F(ConfigStateReferenceTest, WriteModeCannotReadButReopeningForReadSeesItsWrites) {
    Config.OpenSection("mode", VxConfig::WRITE);
    Config.WriteStringEntry("value", "written");
    char text[256] = {}; int integer = 71;
    EXPECT_EQ(static_cast<XDWORD>(-1), Config.ReadStringEntry("value", text));
    EXPECT_EQ(static_cast<XDWORD>(-1), Config.ReadStringEntry("value", nullptr));
    EXPECT_FALSE(Config.ReadStringEntry("value", text, sizeof(text)));
    EXPECT_FALSE(Config.ReadIntegerEntry("value", &integer));
    EXPECT_EQ(71, integer);
    Config.CloseSection("mode");
    Config.OpenSection("mode", VxConfig::READ);
    ExpectValue("value", "written");
}

TEST_F(ConfigStateReferenceTest, CombinedModeAllowsBothOperations) {
    Config.OpenSection("combined", ConfigReadWrite);
    Config.WriteStringEntry("value", "both");
    ExpectValue("value", "both");
}

TEST_F(ConfigStateReferenceTest, CloseIgnoresTheNameAndPreventsFurtherReadsAndWrites) {
    Store("closed", "value", "original");
    Config.OpenSection("closed", ConfigReadWrite);
    Config.CloseSection("different section");
    Config.CloseSection(nullptr);
    Config.WriteStringEntry("value", "denied");
    char text[256] = {};
    EXPECT_EQ(static_cast<XDWORD>(-1), Config.ReadStringEntry("value", text));
    Config.OpenSection("closed", VxConfig::READ);
    ExpectValue("value", "original");
    Config.OpenSection("", VxConfig::READ);
    EXPECT_FALSE(Config.EntryExists("value"));
}

TEST_F(ConfigStateReferenceTest, ReadOpeningCreatesAnEmptyPersistentSection) {
    Config.OpenSection("CreatedByRead", VxConfig::READ);
    VxConfiguration file;
    XString error; int line = 0;
    ASSERT_TRUE(file.BuildFromFile(File, line, error));
    EXPECT_NE(nullptr, file.GetSubSection("CurrentUser.Software.Virtools.UserConfig.CreatedByRead", TRUE));
}

TEST_F(ConfigStateReferenceTest, OpeningAnotherSectionReplacesTheCurrentSelection) {
    Config.OpenSection("first", VxConfig::WRITE);
    Config.WriteStringEntry("value", "one");
    Config.OpenSection("second", VxConfig::WRITE);
    Config.WriteStringEntry("value", "two");
    Config.OpenSection("first", VxConfig::READ); ExpectValue("value", "one");
    Config.OpenSection("second", VxConfig::READ); ExpectValue("value", "two");
}

TEST_F(ConfigStateReferenceTest, NullSectionFailsAndDoesNotLeaveThePreviousSelectionWritable) {
    Store("valid", "value", "original");
    Config.OpenSection("valid", ConfigReadWrite);
    Config.OpenSection(nullptr, VxConfig::WRITE);
    Config.WriteStringEntry("value", "denied");
    EXPECT_EQ(static_cast<XDWORD>(-1), Config.ReadStringEntry("value", nullptr));
    Config.OpenSection("valid", VxConfig::READ); ExpectValue("value", "original");
}

TEST_F(ConfigStateReferenceTest, LeadingBackslashIsInvalidAndClosesThePreviousSelection) {
    Store("valid", "value", "original");
    Config.OpenSection("\\invalid", ConfigReadWrite);
    Config.WriteStringEntry("value", "denied");
    EXPECT_EQ(static_cast<XDWORD>(-1), Config.ReadStringEntry("value", nullptr));
    Config.OpenSection("valid", VxConfig::READ); ExpectValue("value", "original");
}

TEST_F(ConfigStateReferenceTest, EmptySectionExplicitlyOpensTheBaseWithoutConfusingItWithClosedState) {
    Config.OpenSection("", ConfigReadWrite);
    Config.WriteStringEntry("value", "base"); ExpectValue("value", "base");
    Config.CloseSection("");
    EXPECT_EQ(static_cast<XDWORD>(-1), Config.ReadStringEntry("value", nullptr));
    Config.OpenSection("", VxConfig::READ); ExpectValue("value", "base");
}

TEST_F(ConfigStateReferenceTest, RepeatedAndTrailingBackslashesKeepTheSameHierarchy) {
    Store("parent\\\\child\\", "value", "nested");
    Config.OpenSection("parent\\child", VxConfig::READ); ExpectValue("value", "nested");
}

TEST_F(ConfigStateReferenceTest, SectionAndValueNamesIgnoreCaseWithoutDuplicatingValues) {
    Store("MiXeD\\Child", "VaLuE", "first");
    Config.OpenSection("mixed\\CHILD", ConfigReadWrite);
    ExpectValue("value", "first");
    Config.WriteStringEntry("vAlUe", "replacement");
    Config.OpenSection("MIXED\\child", VxConfig::READ); ExpectValue("VALUE", "replacement");
}

TEST_F(ConfigStateReferenceTest, LiteralSectionPunctuationDoesNotCollapseIntoHierarchyOrOtherNames) {
    const char *names[] = {"a.b", "a/b", "a\\b", "a_b", "a%b", "a%2Eb", "x'y", "xy", "x\"y", "x<y", " x ", "#x", "x=y"};
    for (int i = 0; i < 13; ++i) {
        char value[16]; snprintf(value, sizeof(value), "section%d", i);
        Store(names[i], "value", value);
    }
    for (int i = 0; i < 13; ++i) {
        char value[16]; snprintf(value, sizeof(value), "section%d", i);
        Config.OpenSection(names[i], VxConfig::READ); ExpectValue("value", value);
    }
}

TEST_F(ConfigStateReferenceTest, LiteralValueNamesAndTypeSuffixesRemainDistinct) {
    const char *names[] = {"a.b", "a/b", "a\\b", "a_b", "a%b", "a%2Eb", "#x", " x ", "=x", "x'y", "xy", "x\"y", "x<y", "value.type", "value_type", ""};
    Config.OpenSection("names", ConfigReadWrite);
    for (int i = 0; i < 16; ++i) {
        char value[16]; snprintf(value, sizeof(value), "key%d", i);
        Config.WriteStringEntry(names[i], value);
    }
    for (int i = 0; i < 16; ++i) {
        char value[16]; snprintf(value, sizeof(value), "key%d", i);
        ExpectValue(names[i], value);
    }
}

TEST_F(ConfigStateReferenceTest, CaseInsensitiveNamesPersistAcrossInstances) {
    Store("MiXeD", "VaLuE", "persisted");
    VxConfig reader;
    reader.OpenSection("mixed", VxConfig::READ);
    char value[256] = {};
    ASSERT_EQ(10u, reader.ReadStringEntry("value", value));
    EXPECT_STREQ("persisted", value);
    reader.CloseSection(nullptr);
}

TEST_F(ConfigStateReferenceTest, EntryDeletionUsesTheSameCaseInsensitiveLosslessNames) {
    Config.OpenSection("delete", ConfigReadWrite);
    Config.WriteStringEntry("Key.With=Marks", "remove");
    Config.WriteStringEntry("Key_With_Marks", "keep");
    EXPECT_TRUE(Config.DeleteEntry("kEY.wITH=Marks"));
    EXPECT_FALSE(Config.EntryExists("Key.With=Marks"));
    ExpectValue("Key_With_Marks", "keep");
}

TEST_F(ConfigStateReferenceTest, SectionDeletionUsesTheSameLiteralNamesAndCaseRules) {
    Store("Parent\\x.y", "value", "remove");
    Store("Parent\\x_y", "value", "keep");
    EXPECT_TRUE(Config.DeleteSection("PARENT\\X.Y"));
    Config.OpenSection("Parent\\x.y", VxConfig::READ);
    EXPECT_FALSE(Config.EntryExists("value"));
    Config.OpenSection("Parent\\x_y", VxConfig::READ); ExpectValue("value", "keep");
}

TEST_F(ConfigStateReferenceTest, ExistingSafeNamesRemainReadableWithoutRewritingTheFile) {
    const char *text = "<CurrentUser>\n<Software>\n<Virtools>\n<UserConfig>\n<MiXeD>\nVaLuE = legacy\nVaLuE.type = string\n</MiXeD>\n</UserConfig>\n</Virtools>\n</Software>\n</CurrentUser>\n";
    WriteFile(text);
    Config.OpenSection("mixed", VxConfig::READ); ExpectValue("value", "legacy");
    EXPECT_STREQ(text, ReadFile().CStr());
}

TEST_F(ConfigStateReferenceTest, MalformedStorageCannotBeOpenedForWritingOrOverwritten) {
    const char *text = "good=prefix\ninvalid\n";
    WriteFile(text);
    Config.OpenSection("new", VxConfig::WRITE);
    Config.WriteStringEntry("value", "denied");
    EXPECT_STREQ(text, ReadFile().CStr());
}

TEST_F(ConfigStateReferenceTest, ChangingRootClosesTheCurrentSelection) {
    Store("section", "value", "old root");
    Config.OpenSection("section", ConfigReadWrite);
    ASSERT_TRUE(Config.OpenRoot(VXCONFIG_ROOT_CURRENT_USER, "OtherBase"));
    Config.WriteStringEntry("value", "denied");
    EXPECT_EQ(static_cast<XDWORD>(-1), Config.ReadStringEntry("value", nullptr));
    Config.OpenSection("section", VxConfig::READ);
    EXPECT_FALSE(Config.EntryExists("value"));
}

#ifdef _WIN32
TEST_F(ConfigStateReferenceTest, DbcsTrailBackslashIsPartOfTheCharacterNotAPathSeparator) {
    char name[3] = {};
    for (int lead = 0x81; lead <= 0xfe && !name[0]; ++lead) {
        char bytes[2] = {static_cast<char>(lead), '\\'};
        wchar_t wide, other;
        char underscore[2] = {static_cast<char>(lead), '_'};
        if (IsDBCSLeadByteEx(GetACP(), lead) && MultiByteToWideChar(GetACP(), MB_ERR_INVALID_CHARS, bytes, 2, &wide, 1) == 1 &&
            MultiByteToWideChar(GetACP(), MB_ERR_INVALID_CHARS, underscore, 2, &other, 1) == 1 && wide != other) {
            name[0] = bytes[0]; name[1] = bytes[1];
        }
    }
    if (!name[0]) { SUCCEED() << "The active ANSI code page has no DBCS trail backslash"; return; }
    XString section = XString(name) + "\\child";
    Store(section.CStr(), name, "dbcs");
    char other[] = {name[0], '_', 0};
    Store(section.CStr(), other, "different character");
    Config.OpenSection(section.CStr(), VxConfig::READ);
    ExpectValue(name, "dbcs");
    ExpectValue(other, "different character");
}

TEST_F(ConfigStateReferenceTest, UnicodeNamesUseOrdinalCaseInsensitiveComparisonOnWindows) {
    char upper[16] = {}, lower[16] = {};
    BOOL substituted = FALSE;
    DWORD flags = GetACP() == CP_UTF8 ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS;
    BOOL *used = GetACP() == CP_UTF8 ? nullptr : &substituted;
    int a = WideCharToMultiByte(GetACP(), flags, L"\u0410", -1, upper, sizeof(upper), nullptr, used);
    int b = WideCharToMultiByte(GetACP(), flags, L"\u0430", -1, lower, sizeof(lower), nullptr, used);
    if (!a || !b || substituted) { SUCCEED() << "Cyrillic pair is not representable in the active ANSI code page"; return; }
    Store(upper, upper, "unicode");
    Config.OpenSection(lower, VxConfig::READ); ExpectValue(lower, "unicode");
}
#endif
