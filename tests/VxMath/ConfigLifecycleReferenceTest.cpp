#include "XHashTable.h"
// Inspect the real layout without adding runtime accessors or changing its API.
#define private public
#include "VxConfiguration.h"
#undef private
#include "VxWindowFunctions.h"
#include "XP.h"
#include <gtest/gtest.h>
#include <new>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

static const VxConfig::Mode LifecycleReadWrite = static_cast<VxConfig::Mode>(VxConfig::READ | VxConfig::WRITE);

class ConfigLifecycleReferenceTest : public ::testing::Test {
protected:
    VxConfig Config;
    XString Home, PreviousHome;
    XArray<XString> Homes;
    bool HadHome = false, ChangedHome = false;

    void SetUp() override {
        HadHome = VxGetEnvironmentVariable("BALLANCE_USER_CONFIG_HOME", PreviousHome) != FALSE;
        Home = VxGetCurrentDirectory(); Home += "/config-lifecycle-home";
        ASSERT_FALSE(VxDirectoryExists(Home.CStr())); ASSERT_FALSE(VxFileExists(Home.CStr()));
        SetHome(Home);
    }

    void SetHome(const XString &home) {
        ASSERT_TRUE(VxSetEnvironmentVariable("BALLANCE_USER_CONFIG_HOME", home.CStr()));
        ChangedHome = true; Homes.PushBack(home);
    }

    void TearDown() override {
        Config.CloseSection(nullptr);
        for (int i = Homes.Size() - 1; i >= 0; --i) {
            XString file = Homes[i] + "/Ballance/registry.ini"; remove(file.CStr());
            XString directory = Homes[i] + "/Ballance"; VxRemoveDirectory(directory.CStr());
            remove(Homes[i].CStr()); VxRemoveDirectory(Homes[i].CStr());
        }
        if (ChangedHome)
            EXPECT_TRUE(VxSetEnvironmentVariable("BALLANCE_USER_CONFIG_HOME", HadHome ? PreviousHome.CStr() : nullptr));
    }

    void ExpectValue(VxConfig &config, const char *key, const char *expected) {
        char output[256] = {};
        ASSERT_EQ(strlen(expected) + 1, config.ReadStringEntry(key, output));
        EXPECT_STREQ(expected, output);
    }

    void ExpectClosed(const VxConfig &config) {
        EXPECT_EQ(nullptr, config.m_VirtoolsSection); EXPECT_EQ(nullptr, config.m_CurrentSection);
        EXPECT_EQ(0, static_cast<int>(config.m_CurrentMode)); EXPECT_TRUE(config.m_CurrentSectionName.Empty());
    }
};

TEST_F(ConfigLifecycleReferenceTest, LayoutMatchesThePortableArchitectureAndHasNoVirtualState) {
    const bool wide = sizeof(void *) == 8;
    EXPECT_EQ(wide ? 64u : 32u, sizeof(VxConfig)); EXPECT_EQ(wide ? 8u : 4u, alignof(VxConfig));
    EXPECT_EQ(wide ? 16u : 8u, sizeof(XString));
    EXPECT_EQ(4u, sizeof(VxConfigRoot)); EXPECT_EQ(4u, sizeof(VxConfig::Mode));
    EXPECT_EQ(0u, offsetof(VxConfig, m_VirtoolsSection));
    EXPECT_EQ(wide ? 8u : 4u, offsetof(VxConfig, m_CurrentSection));
    EXPECT_EQ(wide ? 16u : 8u, offsetof(VxConfig, m_Root));
    EXPECT_EQ(wide ? 24u : 12u, offsetof(VxConfig, m_BaseSection));
    EXPECT_EQ(wide ? 40u : 20u, offsetof(VxConfig, m_CurrentSectionName));
    EXPECT_EQ(wide ? 56u : 28u, offsetof(VxConfig, m_CurrentMode));
    EXPECT_TRUE(__is_standard_layout(VxConfig)); EXPECT_FALSE(__is_polymorphic(VxConfig));
    EXPECT_FALSE(__has_virtual_destructor(VxConfig)); EXPECT_FALSE(__is_trivially_copyable(VxConfig));
    VxConfig array[2];
    EXPECT_EQ(sizeof(VxConfig), static_cast<size_t>(reinterpret_cast<char *>(&array[1]) - reinterpret_cast<char *>(&array[0])));
    printf("ConfigABI: {\"size\":%zu,\"alignment\":%zu,\"offsets\":[%zu,%zu,%zu,%zu,%zu,%zu]}\n",
        sizeof(VxConfig), alignof(VxConfig), offsetof(VxConfig, m_VirtoolsSection), offsetof(VxConfig, m_CurrentSection),
        offsetof(VxConfig, m_Root), offsetof(VxConfig, m_BaseSection), offsetof(VxConfig, m_CurrentSectionName), offsetof(VxConfig, m_CurrentMode));
}

TEST_F(ConfigLifecycleReferenceTest, ConstructorAndDestructorStayInsideTheObjectStorage) {
    const size_t guard = alignof(VxConfig);
    alignas(VxConfig) unsigned char bytes[sizeof(VxConfig) + guard * 2]; memset(bytes, 0xa5, sizeof(bytes));
    VxConfig *value = new (bytes + guard) VxConfig;
    ExpectClosed(*value); EXPECT_EQ(VXCONFIG_ROOT_CURRENT_USER, value->m_Root);
    EXPECT_STREQ("Software\\Virtools\\UserConfig", value->m_BaseSection.CStr());
    value->~VxConfig();
    for (size_t i = 0; i < guard; ++i) {
        EXPECT_EQ(0xa5, bytes[i]); EXPECT_EQ(0xa5, bytes[guard + sizeof(VxConfig) + i]);
    }
}

TEST_F(ConfigLifecycleReferenceTest, DefaultConstructionStartsClosedWithoutCreatingStorage) {
    ExpectClosed(Config); EXPECT_EQ(VXCONFIG_ROOT_CURRENT_USER, Config.m_Root);
    Config.WriteStringEntry("key", "denied"); EXPECT_FALSE(Config.EntryExists("key"));
    EXPECT_FALSE(VxDirectoryExists(Home.CStr()));
}

TEST_F(ConfigLifecycleReferenceTest, DefaultConstructionUsesTheOriginalLogicalRoot) {
    Config.OpenSection("values", LifecycleReadWrite); Config.WriteStringEntry("key", "value");
    VxConfig reader(VXCONFIG_ROOT_CURRENT_USER, "Software\\Virtools\\UserConfig");
    reader.OpenSection("VALUES", VxConfig::READ); ExpectValue(reader, "KEY", "value");
}

TEST_F(ConfigLifecycleReferenceTest, CustomConstructorOwnsItsBaseAndStartsClosed) {
    char base[] = "Custom\\Owned";
    VxConfig value(VXCONFIG_ROOT_LOCAL_MACHINE, base); base[0] = 'x';
    ExpectClosed(value); EXPECT_EQ(VXCONFIG_ROOT_LOCAL_MACHINE, value.m_Root);
    EXPECT_STREQ("Custom\\Owned", value.m_BaseSection.CStr());
    EXPECT_FALSE(VxDirectoryExists(Home.CStr()));
}

TEST_F(ConfigLifecycleReferenceTest, NullAndEmptyConstructorBasesHaveDistinctMeanings) {
    VxConfig fallback(VXCONFIG_ROOT_CURRENT_USER, nullptr), empty(VXCONFIG_ROOT_CURRENT_USER, "");
    EXPECT_STREQ("Software\\Virtools\\UserConfig", fallback.m_BaseSection.CStr());
    EXPECT_STREQ("", empty.m_BaseSection.CStr()); ExpectClosed(fallback); ExpectClosed(empty);
}

TEST_F(ConfigLifecycleReferenceTest, RootAndCustomBaseNamespacesDoNotAlias) {
    Config.OpenSection("values", LifecycleReadWrite); Config.WriteStringEntry("key", "default");
    VxConfig machine(VXCONFIG_ROOT_LOCAL_MACHINE, nullptr), custom(VXCONFIG_ROOT_CURRENT_USER, "custom");
    machine.OpenSection("values", LifecycleReadWrite); machine.WriteStringEntry("key", "machine");
    custom.OpenSection("values", LifecycleReadWrite); custom.WriteStringEntry("key", "custom");
    ExpectValue(Config, "key", "default"); ExpectValue(machine, "key", "machine"); ExpectValue(custom, "key", "custom");
}

TEST_F(ConfigLifecycleReferenceTest, OpenRootClosesTheSelectionAndDoesNotCreateStorage) {
    EXPECT_TRUE(Config.OpenRoot(VXCONFIG_ROOT_LOCAL_MACHINE, "custom")); ExpectClosed(Config);
    EXPECT_FALSE(VxDirectoryExists(Home.CStr()));
}

TEST_F(ConfigLifecycleReferenceTest, RootReplacementDoesNotReadOrOverwriteTheOldSelection) {
    Config.OpenSection("values", LifecycleReadWrite); Config.WriteStringEntry("key", "original");
    ASSERT_TRUE(Config.OpenRoot(VXCONFIG_ROOT_LOCAL_MACHINE, "new")); ExpectClosed(Config);
    Config.WriteStringEntry("key", "denied"); EXPECT_FALSE(Config.EntryExists("key"));
    Config.OpenSection("values", LifecycleReadWrite); Config.WriteStringEntry("key", "new");
    VxConfig old; old.OpenSection("values", VxConfig::READ); ExpectValue(old, "key", "original");
    ExpectValue(Config, "key", "new");
}

TEST_F(ConfigLifecycleReferenceTest, OpenRootAcceptsItsExistingOwnedBase) {
    ASSERT_TRUE(Config.OpenRoot(VXCONFIG_ROOT_CURRENT_USER, "custom\\base"));
    ASSERT_TRUE(Config.OpenRoot(VXCONFIG_ROOT_LOCAL_MACHINE, Config.m_BaseSection.CStr()));
    ExpectClosed(Config); EXPECT_STREQ("custom\\base", Config.m_BaseSection.CStr());
    EXPECT_EQ(VXCONFIG_ROOT_LOCAL_MACHINE, Config.m_Root);
}

TEST_F(ConfigLifecycleReferenceTest, OpenRootCopiesACurrentSectionNameBeforeClearingIt) {
    Config.OpenSection("borrowed\\base", LifecycleReadWrite);
    ASSERT_TRUE(Config.OpenRoot(VXCONFIG_ROOT_CURRENT_USER, Config.m_CurrentSectionName.CStr()));
    ExpectClosed(Config); EXPECT_STREQ("borrowed\\base", Config.m_BaseSection.CStr());
}

TEST_F(ConfigLifecycleReferenceTest, OpenRootNullRestoresTheDefaultAndEmptySelectsNoBase) {
    ASSERT_TRUE(Config.OpenRoot(VXCONFIG_ROOT_LOCAL_MACHINE, "")); EXPECT_STREQ("", Config.m_BaseSection.CStr());
    ASSERT_TRUE(Config.OpenRoot(VXCONFIG_ROOT_CURRENT_USER, nullptr));
    EXPECT_STREQ("Software\\Virtools\\UserConfig", Config.m_BaseSection.CStr()); ExpectClosed(Config);
}

TEST_F(ConfigLifecycleReferenceTest, DestructionPreservesStoredValuesAndOtherInstances) {
    {
        VxConfig writer; writer.OpenSection("values", LifecycleReadWrite); writer.WriteStringEntry("key", "persisted");
        Config.OpenSection("values", VxConfig::READ);
    }
    ExpectValue(Config, "key", "persisted");
    VxConfig reader; reader.OpenSection("values", VxConfig::READ); ExpectValue(reader, "key", "persisted");
}

TEST_F(ConfigLifecycleReferenceTest, ImplicitCopyOwnsItsStringsAndItsSelection) {
    Config.OpenRoot(VXCONFIG_ROOT_LOCAL_MACHINE, "custom"); Config.OpenSection("values", LifecycleReadWrite);
    Config.WriteStringEntry("key", "shared storage"); VxConfig copy(Config);
    EXPECT_NE(Config.m_BaseSection.CStr(), copy.m_BaseSection.CStr());
    EXPECT_NE(Config.m_CurrentSectionName.CStr(), copy.m_CurrentSectionName.CStr());
    Config.CloseSection(nullptr); ExpectValue(copy, "key", "shared storage");
    copy.WriteStringEntry("key", "updated"); Config.OpenSection("values", VxConfig::READ); ExpectValue(Config, "key", "updated");
}

TEST_F(ConfigLifecycleReferenceTest, ImplicitAssignmentSurvivesTheSourceLifetime) {
    {
        VxConfig source(VXCONFIG_ROOT_LOCAL_MACHINE, "assigned"); source.OpenSection("values", LifecycleReadWrite);
        source.WriteStringEntry("key", "copied"); Config = source;
    }
    EXPECT_EQ(VXCONFIG_ROOT_LOCAL_MACHINE, Config.m_Root); EXPECT_STREQ("assigned", Config.m_BaseSection.CStr());
    ExpectValue(Config, "key", "copied"); Config = Config; ExpectValue(Config, "key", "copied");
}

TEST_F(ConfigLifecycleReferenceTest, RegistryPathUsesTheOverrideAndDoesNotCreateTheRegistryFile) {
    char path[2048] = {}; ASSERT_TRUE(VxConfig::GetRegistryFilePath(path, sizeof(path)));
    XString expected = Home + "/Ballance/registry.ini"; EXPECT_STREQ(expected.CStr(), path);
    XString directory = Home + "/Ballance"; EXPECT_TRUE(VxDirectoryExists(directory.CStr()));
    EXPECT_FALSE(VxFileExists(path));
}

TEST_F(ConfigLifecycleReferenceTest, RegistryPathHonorsTheTerminatorAndLeavesFailureOutputsUntouched) {
    char path[2048] = {}; ASSERT_TRUE(VxConfig::GetRegistryFilePath(path, sizeof(path)));
    size_t length = strlen(path); XAP<char> output(new char[length + 17]); memset(output, 0x5a, length + 17);
    EXPECT_FALSE(VxConfig::GetRegistryFilePath(output, length));
    for (size_t i = 0; i < length + 17; ++i) EXPECT_EQ(0x5a, static_cast<unsigned char>(output[i]));
    ASSERT_TRUE(VxConfig::GetRegistryFilePath(output, length + 1)); EXPECT_STREQ(path, output);
    for (size_t i = length + 1; i < length + 17; ++i) EXPECT_EQ(0x5a, static_cast<unsigned char>(output[i]));
}

TEST_F(ConfigLifecycleReferenceTest, InvalidPathOutputsFailBeforeCreatingDirectories) {
    char output = 'Z'; EXPECT_FALSE(VxConfig::GetRegistryFilePath(&output, 0)); EXPECT_EQ('Z', output);
    EXPECT_FALSE(VxConfig::GetRegistryFilePath(nullptr, 2048)); EXPECT_FALSE(VxDirectoryExists(Home.CStr()));
}

TEST_F(ConfigLifecycleReferenceTest, RegistryPathIsIndependentOfTheSelectedLogicalRoot) {
    char before[2048] = {}, after[2048] = {};
    ASSERT_TRUE(VxConfig::GetRegistryFilePath(before, sizeof(before)));
    Config.OpenRoot(VXCONFIG_ROOT_LOCAL_MACHINE, "custom"); Config.OpenSection("different", LifecycleReadWrite);
    ASSERT_TRUE(VxConfig::GetRegistryFilePath(after, sizeof(after))); EXPECT_STREQ(before, after);
}

TEST_F(ConfigLifecycleReferenceTest, RegistryPathDoesNotCacheTheEnvironmentOverride) {
    char before[2048] = {}, after[2048] = {};
    ASSERT_TRUE(VxConfig::GetRegistryFilePath(before, sizeof(before))); SetHome(Home + "/second");
    ASSERT_TRUE(VxConfig::GetRegistryFilePath(after, sizeof(after)));
    XString expected = Home + "/second/Ballance/registry.ini";
    EXPECT_STREQ(expected.CStr(), after); EXPECT_STRNE(before, after);
}

TEST_F(ConfigLifecycleReferenceTest, AFileBlockingTheConfigurationDirectoryReportsFailure) {
    FILE *file = fopen(Home.CStr(), "wb"); ASSERT_NE(nullptr, file); EXPECT_EQ(0, fclose(file));
    char output[256]; memset(output, 0x5a, sizeof(output));
    EXPECT_FALSE(VxConfig::GetRegistryFilePath(output, sizeof(output)));
    for (char c : output) EXPECT_EQ(0x5a, static_cast<unsigned char>(c));
    EXPECT_TRUE(VxFileExists(Home.CStr()));
}

TEST_F(ConfigLifecycleReferenceTest, AForwardTrailingSeparatorDoesNotDuplicateTheApplicationPath) {
    SetHome(Home + "/"); char path[2048] = {};
    ASSERT_TRUE(VxConfig::GetRegistryFilePath(path, sizeof(path)));
    XString expected = Home + "/Ballance/registry.ini"; EXPECT_STREQ(expected.CStr(), path);
}

#ifdef _WIN32
TEST_F(ConfigLifecycleReferenceTest, ANativeTrailingSeparatorStillReturnsAUsablePath) {
    SetHome(Home + "\\"); char path[2048] = {};
    ASSERT_TRUE(VxConfig::GetRegistryFilePath(path, sizeof(path)));
    XString expected = Home + "\\Ballance/registry.ini"; EXPECT_STREQ(expected.CStr(), path);
    FILE *file = fopen(path, "wb"); ASSERT_NE(nullptr, file); EXPECT_EQ(0, fclose(file));
}

TEST_F(ConfigLifecycleReferenceTest, RegistryPathReturnsNativeAnsiBytesForRepresentableUnicode) {
    char native[16] = {}; BOOL substituted = FALSE;
    int length = WideCharToMultiByte(GetACP(), GetACP() == CP_UTF8 ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS,
        L"\u4E2D", -1, native, sizeof(native), nullptr, GetACP() == CP_UTF8 ? nullptr : &substituted);
    if (!length || substituted) { SUCCEED() << "Character is unavailable in the active ANSI code page"; return; }
    XString extra = Home + "/" + native; SetHome(extra);
    char path[2048] = {}; ASSERT_TRUE(VxConfig::GetRegistryFilePath(path, sizeof(path)));
    XString expected = extra + "/Ballance/registry.ini"; EXPECT_STREQ(expected.CStr(), path);
    FILE *file = fopen(path, "wb"); ASSERT_NE(nullptr, file); EXPECT_EQ(0, fclose(file));
    printf("ConfigPathACP: %u; UnicodeBytes: %d\n", GetACP(), length - 1);
}

TEST_F(ConfigLifecycleReferenceTest, RepeatedLifetimesDoNotRetainOperatingSystemHandles) {
    Config.OpenSection("values", LifecycleReadWrite); Config.WriteStringEntry("key", "value");
    DWORD before = 0, after = 0; ASSERT_TRUE(GetProcessHandleCount(GetCurrentProcess(), &before));
    for (int i = 0; i < 64; ++i) {
        VxConfig value; value.OpenSection("values", VxConfig::READ); ExpectValue(value, "key", "value");
    }
    ASSERT_TRUE(GetProcessHandleCount(GetCurrentProcess(), &after)); EXPECT_LE(after, before);
}
#endif
