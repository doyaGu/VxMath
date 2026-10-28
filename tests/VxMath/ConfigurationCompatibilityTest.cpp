#include "VxConfiguration.h"
#include "VxWindowFunctions.h"
#include <gtest/gtest.h>
#include <stdio.h>
#include <string.h>

static void ExpectEntry(VxConfiguration &config, const char *name, const char *expected, XBOOL dotted = FALSE) {
    VxConfigurationEntry *entry = config.GetEntry(name, dotted);
    ASSERT_NE(nullptr, entry);
    EXPECT_STREQ(expected, entry->GetValue());
}

TEST(ConfigurationCompatibilityTest, DataFileReadRetainsItsTextFileFallback) {
    FILE *file = fopen("readable.cfg", "wb");
    ASSERT_NE(nullptr, file);
    ASSERT_EQ(10u, fwrite("key=value\n", 1, 10, file));
    ASSERT_EQ(0, fclose(file));
    VxConfiguration config;
    XString error;
    ASSERT_TRUE(config.BuildFromDataFile("readable.cfg", error));
    ExpectEntry(config, "key", "value");
}

TEST(ConfigurationCompatibilityTest, DataFileSaveRetainsItsTextFileFallback) {
    VxConfiguration config;
    ASSERT_TRUE(config.AddEntry(nullptr, "key", "value"));
    ASSERT_TRUE(config.SaveToDataFile("written.cfg"));
    FILE *file = fopen("written.cfg", "rb");
    ASSERT_NE(nullptr, file);
    char text[128] = {};
    fread(text, 1, sizeof(text) - 1, file);
    fclose(file);
    EXPECT_NE(nullptr, strstr(text, "key = value"));
}

TEST(ConfigurationCompatibilityTest, SemicolonCommentsStillLoad) {
    VxConfiguration config;
    int line = 0;
    XString error;
    ASSERT_TRUE(config.BuildFromMemory("; ordinary comment\nkey=value\n", line, error));
    ExpectEntry(config, "key", "value");
}

TEST(ConfigurationCompatibilityTest, ReopenedSectionsStillMergeTheirEntries) {
    VxConfiguration config;
    int line = 0;
    XString error;
    ASSERT_TRUE(config.BuildFromMemory("<A>\nx=one\n</A>\n<A>\ny=two\n</A>\n", line, error));
    ExpectEntry(config, "A.x", "one", TRUE);
    ExpectEntry(config, "A.y", "two", TRUE);
}

TEST(ConfigurationCompatibilityTest, DottedSectionTagsStillSupportHierarchicalLookup) {
    VxConfiguration config;
    int line = 0;
    XString error;
    ASSERT_TRUE(config.BuildFromMemory("<A.B>\nkey=value\n</A.B>\n", line, error));
    ExpectEntry(config, "A.B.key", "value", TRUE);
}

TEST(ConfigurationCompatibilityTest, FailedFileOpenRetainsTheExistingConfiguration) {
    remove("missing.cfg");
    VxConfiguration config;
    ASSERT_TRUE(config.AddEntry(nullptr, "keep", "value"));
    int line = 123;
    XString error;
    ASSERT_FALSE(config.BuildFromFile("missing.cfg", line, error));
    ExpectEntry(config, "keep", "value");
}

TEST(ConfigurationCompatibilityTest, NullMemoryInputRetainsTheExistingConfiguration) {
    VxConfiguration config;
    ASSERT_TRUE(config.AddEntry(nullptr, "keep", "value"));
    int line = 123;
    XString error;
    ASSERT_FALSE(config.BuildFromMemory(nullptr, line, error));
    ExpectEntry(config, "keep", "value");
}

TEST(ConfigurationCompatibilityTest, MemoryInputMayBorrowAnExistingEntryValue) {
    VxConfiguration config;
    ASSERT_TRUE(config.AddEntry(nullptr, "input", "loaded=value\n"));
    const char *input = config.GetEntry("input", FALSE)->GetValue();
    int line = 0;
    XString error;
    ASSERT_TRUE(config.BuildFromMemory(input, line, error));
    ExpectEntry(config, "loaded", "value");
}

TEST(ConfigurationCompatibilityTest, RealHierarchyPrecedesLiteralDottedNames) {
    VxConfiguration config;
    int line = 0;
    XString error;
    ASSERT_TRUE(config.BuildFromMemory("<A.B>\nkey=literal\n</A.B>\n<A>\n<B>\nkey=hierarchy\n</B>\n</A>\n", line, error));
    ExpectEntry(config, "A.B.key", "hierarchy", TRUE);
    ASSERT_NE(nullptr, config.GetSubSection("A.B", FALSE));
    EXPECT_STREQ("literal", config.GetSubSection("A.B", FALSE)->GetEntry("key")->GetValue());
}

TEST(ConfigurationCompatibilityTest, FullDottedClosingTagClosesTheNestedHierarchy) {
    VxConfiguration config;
    int line = 0;
    XString error;
    ASSERT_TRUE(config.BuildFromMemory("<A>\n<B>\nkey=value\n</A.B>\nroot=next\n", line, error));
    ExpectEntry(config, "A.B.key", "value", TRUE);
    ExpectEntry(config, "root", "next");
}

// The old spelling is an independent fixture from the Pass365 legacy DLL output.
static const char *LegacyRegistry =
    "<CurrentUser>\n<RegressionReview>\n<Options>\n<Video>\n"
    "plain = legacy-section\nplain.type = string\n"
    "<Child>\nplain = legacy-child\nplain.type = string\n</Child>\n"
    "</Video>\n</Options>\n<Simple>\n"
    "plain = legacy-plain\nplain.type = string\n"
    "volume_level = 75\nvolume_level.type = integer\n"
    "</Simple>\n</RegressionReview>\n</CurrentUser>\n";

class ConfigCompatibilityTest : public ::testing::Test {
protected:
    VxConfig Config{VXCONFIG_ROOT_CURRENT_USER, "RegressionReview"};
    XString Home, PreviousHome;
    char File[2048] = {};
    bool HadHome = false, ChangedHome = false, OwnFile = false;

    void SetUp() override {
        HadHome = VxGetEnvironmentVariable("BALLANCE_USER_CONFIG_HOME", PreviousHome) != FALSE;
        Home = VxGetCurrentDirectory();
        Home += "/config-compatibility-home";
        ASSERT_TRUE(VxSetEnvironmentVariable("BALLANCE_USER_CONFIG_HOME", Home.CStr()));
        ChangedHome = true;
        ASSERT_TRUE(VxConfig::GetRegistryFilePath(File, sizeof(File)));
        FILE *existing = fopen(File, "rb");
        if (existing) {
            fclose(existing);
            FAIL() << "The isolated registry file already exists";
        }
        OwnFile = true;
        VxCreateFileTree(File);
        Store(LegacyRegistry);
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

    void Store(const char *text) {
        FILE *file = fopen(File, "wb");
        ASSERT_NE(nullptr, file);
        EXPECT_EQ(strlen(text), fwrite(text, 1, strlen(text), file));
        EXPECT_EQ(0, fclose(file));
    }

    XString Contents() {
        FILE *file = fopen(File, "rb");
        if (!file) return "";
        char text[16384];
        size_t length = fread(text, 1, sizeof(text) - 1, file);
        text[length] = 0;
        fclose(file);
        return text;
    }

    void Read(const char *section, const char *key, const char *expected) {
        VxConfig config(VXCONFIG_ROOT_CURRENT_USER, "RegressionReview");
        config.OpenSection(section, VxConfig::READ);
        char text[128] = {};
        ASSERT_TRUE(config.ReadStringEntry(key, text, sizeof(text)));
        EXPECT_STREQ(expected, text);
    }

    void Missing(const char *section, const char *key) {
        VxConfig config(VXCONFIG_ROOT_CURRENT_USER, "RegressionReview");
        config.OpenSection(section, VxConfig::READ);
        char text[128] = "unchanged";
        EXPECT_FALSE(config.ReadStringEntry(key, text, sizeof(text)));
        EXPECT_STREQ("unchanged", text);
        EXPECT_FALSE(config.EntryExists(key));
    }

    void Write(const char *section, const char *key, const char *value) {
        Config.OpenSection(section, VxConfig::WRITE);
        Config.WriteStringEntry(key, value);
        Config.CloseSection(section);
    }
};

TEST_F(ConfigCompatibilityTest, LegacyValueNamesAndTheirTypesAreRecovered) {
    Read("Simple", "volume.level", "75");
    Config.OpenSection("Simple", VxConfig::READ);
    int value = 0;
    ASSERT_TRUE(Config.ReadIntegerEntry("volume.level", &value));
    EXPECT_EQ(75, value);
    VxConfigValueType type = VXCONFIG_VALUE_STRING;
    ASSERT_TRUE(Config.GetEntryType("volume.level", &type));
    EXPECT_EQ(VXCONFIG_VALUE_INTEGER, type);
}

TEST_F(ConfigCompatibilityTest, LegacyDottedSectionsAndDescendantsAreRecovered) {
    Read("Options.Video", "plain", "legacy-section");
    Read("Options.Video\\Child", "plain", "legacy-child");
}

TEST_F(ConfigCompatibilityTest, ReadOnlyLegacyLookupDoesNotRewriteTheFile) {
    XString before = Contents();
    Read("Options.Video", "plain", "legacy-section");
    Read("Simple", "volume.level", "75");
    EXPECT_STREQ(before.CStr(), Contents().CStr());
}

TEST_F(ConfigCompatibilityTest, ExplicitNewKeyWinsAndUnrelatedLegacyValuesRemain) {
    Write("Simple", "volume.level", "new");
    Read("Simple", "volume.level", "new");
    Read("Simple", "plain", "legacy-plain");
}

TEST_F(ConfigCompatibilityTest, ExplicitNewSectionWinsAndLegacyKeysRemainReadable) {
    Write("Options.Video", "plain", "new-section");
    Read("Options.Video", "plain", "new-section");
    Read("Options.Video\\Child", "plain", "legacy-child");
}

TEST_F(ConfigCompatibilityTest, NewKeysDoNotBecomeLossyLegacyAliases) {
    Write("Simple", "a_b", "underscore");
    Missing("Simple", "a.b");
    Write("Simple", "a.b", "dot");
    Read("Simple", "a_b", "underscore");
    Read("Simple", "a.b", "dot");
}

TEST_F(ConfigCompatibilityTest, NewSectionsDoNotBecomeLossyLegacyAliases) {
    Write("Options\\Video", "modern", "nested");
    Missing("Options.Video", "modern");
    Read("Options\\Video", "modern", "nested");
}

TEST_F(ConfigCompatibilityTest, LegacyDeleteDoesNotReappearAfterReopen) {
    Config.OpenSection("Simple", VxConfig::WRITE);
    ASSERT_TRUE(Config.DeleteEntry("volume.level"));
    Config.CloseSection(nullptr);
    Missing("Simple", "volume.level");
    Read("Simple", "plain", "legacy-plain");
}

TEST_F(ConfigCompatibilityTest, DeletingNewValueDoesNotExposeAnOlderAlias) {
    Write("Simple", "volume.level", "new");
    Config.OpenSection("Simple", VxConfig::WRITE);
    ASSERT_TRUE(Config.DeleteEntry("volume.level"));
    Config.CloseSection(nullptr);
    Missing("Simple", "volume.level");
    Write("Simple", "volume.level", "restored");
    Read("Simple", "volume.level", "restored");
}

TEST_F(ConfigCompatibilityTest, DeletingLegacySectionBlocksItsOldSubtree) {
    Config.OpenSection("Options.Video", VxConfig::WRITE);
    ASSERT_TRUE(Config.DeleteSection(nullptr));
    Missing("Options.Video", "plain");
    Missing("Options.Video\\Child", "plain");
    Read("Simple", "plain", "legacy-plain");
}

TEST_F(ConfigCompatibilityTest, DeletingNewSectionDoesNotExposeTheOldSection) {
    Write("Options.Video", "plain", "new");
    Config.OpenSection("Options.Video", VxConfig::WRITE);
    ASSERT_TRUE(Config.DeleteSection(nullptr));
    Missing("Options.Video", "plain");
    Missing("Options.Video\\Child", "plain");
}

TEST_F(ConfigCompatibilityTest, RestoringOneValueDoesNotRestoreDeletedLegacySiblings) {
    Config.OpenSection("Options.Video", VxConfig::WRITE);
    ASSERT_TRUE(Config.DeleteSection(nullptr));
    Write("Options.Video", "restored", "value");
    Read("Options.Video", "restored", "value");
    Missing("Options.Video", "plain");
    Missing("Options.Video\\Child", "plain");
}

TEST_F(ConfigCompatibilityTest, LegacyBaseNamesAreResolved) {
    Store("<CurrentUser>\n<Regression>\n<Review>\n<Simple>\nkey=legacy\n</Simple>\n</Review>\n</Regression>\n</CurrentUser>\n");
    VxConfig config(VXCONFIG_ROOT_CURRENT_USER, "Regression.Review");
    config.OpenSection("Simple", VxConfig::READ);
    char text[128] = {};
    ASSERT_TRUE(config.ReadStringEntry("key", text, sizeof(text)));
    EXPECT_STREQ("legacy", text);
}
