#include "VxConfiguration.h"
#include "VxWindowFunctions.h"
#include <gtest/gtest.h>
#include <stdio.h>
#include <string.h>

class ConfigStringReferenceTest : public ::testing::Test {
protected:
    VxConfig Config;
    XString Home, PreviousHome;
    char File[2048] = {};
    bool HadHome = false, ChangedHome = false, OwnFile = false;
    const char *Section = "BallanceStringReference";

    void SetUp() override {
        HadHome = VxGetEnvironmentVariable("BALLANCE_USER_CONFIG_HOME", PreviousHome) != FALSE;
        Home = VxGetCurrentDirectory();
        Home += "/config-string-reference-home";
        ASSERT_TRUE(VxSetEnvironmentVariable("BALLANCE_USER_CONFIG_HOME", Home.CStr()));
        ChangedHome = true;
        ASSERT_TRUE(VxConfig::GetRegistryFilePath(File, sizeof(File)));
        FILE *existing = fopen(File, "rb");
        if (existing) {
            fclose(existing);
            FAIL() << "The isolated fixture registry file already exists";
        }
        OwnFile = true;
        Config.OpenSection(Section, VxConfig::WRITE);
    }

    void TearDown() override {
        Config.CloseSection(Section);
        if (OwnFile) {
            remove(File);
            XString directory = Home + "/Ballance";
            VxRemoveDirectory(directory.CStr());
            VxRemoveDirectory(Home.CStr());
        }
        if (ChangedHome)
            EXPECT_TRUE(VxSetEnvironmentVariable("BALLANCE_USER_CONFIG_HOME", HadHome ? PreviousHome.CStr() : nullptr));
    }

    void Write(const char *value, const char *key = "value") {
        Config.OpenSection(Section, VxConfig::WRITE);
        Config.WriteStringEntry(key, value);
        Config.CloseSection(Section);
        Config.OpenSection(Section, VxConfig::READ);
    }
};

struct ConfigStringOutput {
    unsigned char Before[16];
    char Text[256];
    unsigned char After[16];
    ConfigStringOutput() { memset(this, 0xa5, sizeof(*this)); }
    void CheckBounds() const {
        for (int i = 0; i < 16; ++i) {
            EXPECT_EQ(0xa5, Before[i]);
            EXPECT_EQ(0xa5, After[i]);
        }
    }
};

TEST_F(ConfigStringReferenceTest, LegacyEmptyStringReturnsItsTerminatorByte) {
    Write("");
    ConfigStringOutput output;
    EXPECT_EQ(1u, Config.ReadStringEntry("value", output.Text));
    EXPECT_STREQ("", output.Text);
    output.CheckBounds();
}

TEST_F(ConfigStringReferenceTest, LegacyReadReturnsThePayloadAndTerminatorSize) {
    Write("ordinary value=tail # marker");
    ConfigStringOutput output;
    EXPECT_EQ(sizeof("ordinary value=tail # marker"), Config.ReadStringEntry("value", output.Text));
    EXPECT_STREQ("ordinary value=tail # marker", output.Text);
    output.CheckBounds();
}

TEST_F(ConfigStringReferenceTest, LegacyReadAccepts255PayloadBytes) {
    char value[256]; memset(value, 'x', 255); value[255] = 0;
    Write(value);
    ConfigStringOutput output;
    EXPECT_EQ(256u, Config.ReadStringEntry("value", output.Text));
    EXPECT_STREQ(value, output.Text);
    output.CheckBounds();
}

TEST_F(ConfigStringReferenceTest, LegacyReadRejects256PayloadBytesInsteadOfReportingTruncatedSuccess) {
    char value[257]; memset(value, 'x', 256); value[256] = 0;
    Write(value);
    ConfigStringOutput output;
    EXPECT_EQ(static_cast<XDWORD>(-1), Config.ReadStringEntry("value", output.Text));
    output.CheckBounds();
}

TEST_F(ConfigStringReferenceTest, LegacyReadRejectsLongValuesWithoutOverrunningTheFixedOutput) {
    char value[1025]; memset(value, 'x', 1024); value[1024] = 0;
    Write(value);
    ConfigStringOutput output;
    EXPECT_EQ(static_cast<XDWORD>(-1), Config.ReadStringEntry("value", output.Text));
    output.CheckBounds();
}

TEST_F(ConfigStringReferenceTest, NullOutputQueriesTheCompleteSizeIncludingTheTerminator) {
    const int sizes[] = {0, 1, 254, 255, 256, 257, 1024};
    for (int size : sizes) {
        char value[1025]; memset(value, 'x', size); value[size] = 0;
        Write(value);
        EXPECT_EQ(static_cast<XDWORD>(size + 1), Config.ReadStringEntry("value", nullptr));
    }
}

TEST_F(ConfigStringReferenceTest, MissingValueFailsBothReadAndSizeQuery) {
    Write("existing");
    ConfigStringOutput output;
    EXPECT_EQ(static_cast<XDWORD>(-1), Config.ReadStringEntry("missing", output.Text));
    EXPECT_EQ(static_cast<XDWORD>(-1), Config.ReadStringEntry("missing", nullptr));
    output.CheckBounds();
}

TEST_F(ConfigStringReferenceTest, NullAndEmptyNamesReferToTheSameDefaultValue) {
    Write("unnamed", nullptr);
    ConfigStringOutput output;
    EXPECT_EQ(8u, Config.ReadStringEntry(nullptr, output.Text));
    EXPECT_STREQ("unnamed", output.Text);
    EXPECT_EQ(8u, Config.ReadStringEntry("", nullptr));
    Write("replacement", "");
    EXPECT_EQ(12u, Config.ReadStringEntry(nullptr, output.Text));
    EXPECT_STREQ("replacement", output.Text);
    output.CheckBounds();
}

TEST_F(ConfigStringReferenceTest, ValueAndLengthPersistAcrossInstancesAndReopening) {
    Write("first");
    Write("replacement");
    VxConfig reader;
    reader.OpenSection(Section, VxConfig::READ);
    ConfigStringOutput output;
    EXPECT_EQ(12u, reader.ReadStringEntry("value", nullptr));
    EXPECT_EQ(12u, reader.ReadStringEntry("value", output.Text));
    EXPECT_STREQ("replacement", output.Text);
    reader.CloseSection(Section);
    output.CheckBounds();
}

TEST_F(ConfigStringReferenceTest, SizedOverloadRetainsItsExplicitTruncationBehavior) {
    Write("longer value");
    char output[5] = {};
    EXPECT_TRUE(Config.ReadStringEntry("value", output, sizeof(output)));
    EXPECT_STREQ("long", output);
    EXPECT_FALSE(Config.ReadStringEntry("value", output, 0));
    EXPECT_FALSE(Config.ReadStringEntry("value", nullptr, sizeof(output)));
}

TEST_F(ConfigStringReferenceTest, NullStringValueRemainsAnEmptyStringInThePortableAdapter) {
    Write(nullptr);
    ConfigStringOutput output;
    EXPECT_EQ(1u, Config.ReadStringEntry("value", output.Text));
    EXPECT_STREQ("", output.Text);
    EXPECT_EQ(1u, Config.ReadStringEntry("value", nullptr));
    output.CheckBounds();
}
