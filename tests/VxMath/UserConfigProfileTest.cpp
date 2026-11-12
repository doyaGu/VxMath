#include "VxConfiguration.h"
#include "VxWindowFunctions.h"
#include "VxUserConfig.h"
#include "TemporaryDirectory.h"
#include <gtest/gtest.h>
#include <string>

class UserConfigProfileTest : public ::testing::Test {
protected:
    TemporaryDirectory Directory;
    XString PreviousHome, PreviousCustomHome;
    bool HadHome = false, HadCustomHome = false;

    void SetUp() override {
        ASSERT_TRUE(Directory.IsValid());
        HadHome = VxGetEnvironmentVariable("NEMO_USER_CONFIG_HOME", PreviousHome) != FALSE;
        HadCustomHome = VxGetEnvironmentVariable("SAMPLE_CONFIG_HOME", PreviousCustomHome) != FALSE;
        ASSERT_TRUE(VxSetUserConfigProfile(NULL));
        ASSERT_TRUE(VxSetEnvironmentVariable("NEMO_USER_CONFIG_HOME", Directory.Path()));
        XString custom = XString(Directory.Path()) + "/custom";
        ASSERT_TRUE(VxSetEnvironmentVariable("SAMPLE_CONFIG_HOME", custom.CStr()));
    }

    void TearDown() override {
        EXPECT_TRUE(VxSetUserConfigProfile(NULL));
        EXPECT_TRUE(VxSetEnvironmentVariable("NEMO_USER_CONFIG_HOME", HadHome ? PreviousHome.CStr() : NULL));
        EXPECT_TRUE(VxSetEnvironmentVariable("SAMPLE_CONFIG_HOME", HadCustomHome ? PreviousCustomHome.CStr() : NULL));
        EXPECT_TRUE(VxDeleteDirectory(Directory.Path()));
    }

    void ExpectPath(const char *relative) {
        XString path;
        ASSERT_TRUE(VxGetUserConfigPath(NULL, path));
        XString expected = XString(Directory.Path()) + relative;
        EXPECT_EQ(expected, path);
        EXPECT_TRUE(VxDirectoryExists(path.CStr()));
    }
};

TEST_F(UserConfigProfileTest, DefaultProfileUsesTheEngineNamespace) {
    ExpectPath("/NeMo2/");
    char path[2048] = {};
    ASSERT_TRUE(VxConfig::GetRegistryFilePath(path, sizeof(path)));
    XString expected = XString(Directory.Path()) + "/NeMo2/registry.ini";
    EXPECT_STREQ(expected.CStr(), path);
}

TEST_F(UserConfigProfileTest, ProfileCopiesNamesAndReadsTheSelectedEnvironmentOnEachLookup) {
    char app[] = "Sample", variable[] = "SAMPLE_CONFIG_HOME";
    ASSERT_TRUE(VxSetUserConfigProfile(app, variable));
    app[0] = 'X'; variable[0] = 'X';
    ExpectPath("/custom/Sample/");
    ASSERT_TRUE(VxSetEnvironmentVariable("SAMPLE_CONFIG_HOME", Directory.Path()));
    ExpectPath("/Sample/");
}

TEST_F(UserConfigProfileTest, ExplicitApplicationNameOverridesTheProfileName) {
    ASSERT_TRUE(VxSetUserConfigProfile("Sample", "SAMPLE_CONFIG_HOME"));
    XString path;
    ASSERT_TRUE(VxGetUserConfigPath("Other", path));
    XString expected = XString(Directory.Path()) + "/custom/Other/";
    EXPECT_EQ(expected, path);
}

TEST_F(UserConfigProfileTest, EmptyValuesRestoreTheEngineDefaults) {
    ASSERT_TRUE(VxSetUserConfigProfile("Sample", "SAMPLE_CONFIG_HOME"));
    ASSERT_TRUE(VxSetUserConfigProfile("", ""));
    ExpectPath("/NeMo2/");
}

TEST_F(UserConfigProfileTest, RejectedNamesLeaveTheCurrentProfileIntact) {
    ASSERT_TRUE(VxSetUserConfigProfile("Sample", "SAMPLE_CONFIG_HOME"));
    std::string oversized(XString::MAX_LENGTH + 1, 'x');
    EXPECT_FALSE(VxSetUserConfigProfile(oversized.c_str()));
    EXPECT_FALSE(VxSetUserConfigProfile("Other", oversized.c_str()));
    ExpectPath("/custom/Sample/");
}

TEST_F(UserConfigProfileTest, ApplicationsKeepSeparateRegistryValues) {
    ASSERT_TRUE(VxSetUserConfigProfile("First"));
    {
        VxConfig config;
        config.OpenSection("Settings", VxConfig::WRITE);
        config.WriteIntegerEntry("Value", 42);
    }
    ASSERT_TRUE(VxSetUserConfigProfile("Second"));
    {
        VxConfig config;
        config.OpenSection("Settings", VxConfig::READ);
        EXPECT_FALSE(config.EntryExists("Value"));
    }
    ASSERT_TRUE(VxSetUserConfigProfile("First"));
    VxConfig config;
    config.OpenSection("Settings", VxConfig::READ);
    int value = 0;
    ASSERT_TRUE(config.ReadIntegerEntry("Value", &value));
    EXPECT_EQ(42, value);
}
