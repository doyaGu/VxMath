#include <gtest/gtest.h>

#include "VxMath.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

constexpr size_t kLegacyWindowsPathLimit = 260;

struct ListContext {
    std::vector<std::string> names;
};

XBOOL CollectEntry(const VxDirectoryEntry *entry, void *userData) {
    ListContext *context = static_cast<ListContext *>(userData);
    context->names.push_back(entry->Name.CStr());
    return TRUE;
}

std::filesystem::path ToLongFilesystemPath(const std::filesystem::path &path) {
#if defined(_WIN32)
    std::string absolute = std::filesystem::absolute(path).string();
    std::replace(absolute.begin(), absolute.end(), '/', '\\');
    if (absolute.rfind("\\\\?\\", 0) != 0)
        absolute = "\\\\?\\" + absolute;
    return std::filesystem::path(absolute);
#else
    return path;
#endif
}

std::string ToRuntimePath(const std::filesystem::path &path) {
#if defined(_WIN32)
    return ToLongFilesystemPath(path).string();
#else
    return path.string();
#endif
}

std::filesystem::path BuildLongDirectory(const std::filesystem::path &root, const char *leafName) {
    std::filesystem::path dir = root;
    int index = 0;
    while (ToRuntimePath(dir / leafName).size() <= kLegacyWindowsPathLimit + 32) {
        dir /= std::string("segment ") + std::to_string(index++) + "_" + std::string(32, 'x');
    }
    return dir;
}

const char *kVxConfigTestBase = "Software\\Virtools\\UserConfig\\BallanceTest";

std::vector<char> MakeRegistryFilePathBuffer() {
    XString configPath;
    if (!VxGetUserConfigPath("Ballance", configPath))
        return std::vector<char>();

    XString registryPath = configPath;
    registryPath << "registry.ini";
    return std::vector<char>((size_t)registryPath.Length() + 1u, '\0');
}

XBOOL LoadRegistryConfig(VxConfiguration &config) {
    std::vector<char> path = MakeRegistryFilePathBuffer();
    if (path.empty() || !VxConfig::GetRegistryFilePath(path.data(), path.size()))
        return FALSE;
    XString error;
    int line = 0;
    return config.BuildFromFile(path.data(), line, error);
}

} // namespace

TEST(VxConfigRegistryTest, TypedRoundTripAndDelete) {
    VxConfig config(VXCONFIG_ROOT_CURRENT_USER, kVxConfigTestBase);
    config.DeleteSection("Unit/Registry");
    config.OpenSection("Unit/Registry", VxConfig::WRITE);
    config.WriteStringEntry("StringValue", "Ballance");
    config.WriteIntegerEntry("IntegerValue", 42);
    config.WriteFloatEntry("FloatValue", 3.5f);
    config.WriteBooleanEntry("BooleanValue", TRUE);
    config.CloseSection("Unit/Registry");

    config.OpenSection("Unit\\Registry", VxConfig::READ);
    char stringValue[64] = {0};
    int integerValue = 0;
    float floatValue = 0.0f;
    XBOOL booleanValue = FALSE;
    EXPECT_TRUE(config.ReadStringEntry("StringValue", stringValue, sizeof(stringValue)));
    EXPECT_STREQ(stringValue, "Ballance");
    EXPECT_TRUE(config.ReadIntegerEntry("IntegerValue", &integerValue));
    EXPECT_EQ(integerValue, 42);
    EXPECT_TRUE(config.ReadFloatEntry("FloatValue", &floatValue));
    EXPECT_FLOAT_EQ(floatValue, 3.5f);
    EXPECT_TRUE(config.ReadBooleanEntry("BooleanValue", &booleanValue));
    EXPECT_TRUE(booleanValue);
    EXPECT_TRUE(config.EntryExists("StringValue"));

    VxConfigValueType type = VXCONFIG_VALUE_STRING;
    ASSERT_TRUE(config.GetEntryType("StringValue", &type));
    EXPECT_EQ(type, VXCONFIG_VALUE_STRING);
    ASSERT_TRUE(config.GetEntryType("IntegerValue", &type));
    EXPECT_EQ(type, VXCONFIG_VALUE_INTEGER);
    ASSERT_TRUE(config.GetEntryType("FloatValue", &type));
    EXPECT_EQ(type, VXCONFIG_VALUE_FLOAT);
    ASSERT_TRUE(config.GetEntryType("BooleanValue", &type));
    EXPECT_EQ(type, VXCONFIG_VALUE_BOOLEAN);

    EXPECT_TRUE(config.DeleteEntry("StringValue"));
    EXPECT_FALSE(config.EntryExists("StringValue"));
    config.CloseSection("Unit\\Registry");
    config.DeleteSection("Unit/Registry");
}

TEST(VxConfigRegistryTest, ValuesPersistAcrossInstances) {
    {
        VxConfig writer(VXCONFIG_ROOT_CURRENT_USER, kVxConfigTestBase);
        writer.DeleteSection("Unit/CrossInstance");
        writer.OpenSection("Unit/CrossInstance", VxConfig::WRITE);
        writer.WriteStringEntry("PersistedString", "stored");
        writer.WriteIntegerEntry("PersistedInteger", 1234);
    }

    VxConfig reader(VXCONFIG_ROOT_CURRENT_USER, kVxConfigTestBase);
    reader.OpenSection("Unit/CrossInstance", VxConfig::READ);
    char stringValue[64] = {0};
    int integerValue = 0;
    EXPECT_TRUE(reader.ReadStringEntry("PersistedString", stringValue, sizeof(stringValue)));
    EXPECT_STREQ(stringValue, "stored");
    EXPECT_TRUE(reader.ReadIntegerEntry("PersistedInteger", &integerValue));
    EXPECT_EQ(integerValue, 1234);
    reader.DeleteSection("Unit/CrossInstance");
}

TEST(VxConfigRegistryTest, SlashAndBackslashSectionsAreEquivalent) {
    VxConfig config(VXCONFIG_ROOT_CURRENT_USER, kVxConfigTestBase);
    config.DeleteSection("Unit/SlashBackslash");
    config.OpenSection("Unit/SlashBackslash", VxConfig::WRITE);
    config.WriteBooleanEntry("SharedSectionValue", TRUE);
    config.CloseSection("Unit/SlashBackslash");

    config.OpenSection("Unit\\SlashBackslash", VxConfig::READ);
    XBOOL value = FALSE;
    EXPECT_TRUE(config.ReadBooleanEntry("SharedSectionValue", &value));
    EXPECT_TRUE(value);
    config.DeleteSection("Unit/SlashBackslash");
}

TEST(VxConfigRegistryTest, DeletingEntryRemovesTypeMetadata) {
    VxConfig config(VXCONFIG_ROOT_CURRENT_USER, kVxConfigTestBase);
    config.DeleteSection("Unit/DeleteMetadata");
    config.OpenSection("Unit/DeleteMetadata", VxConfig::WRITE);
    config.WriteIntegerEntry("DeleteMetadataValue", 7);

    VxConfiguration registry;
    ASSERT_TRUE(LoadRegistryConfig(registry));
    VxConfigurationSection *section = registry.GetSubSection(
        "CurrentUser.Software.Virtools.UserConfig.BallanceTest.Unit.DeleteMetadata", TRUE);
    ASSERT_NE(section, nullptr);
    ASSERT_NE(section->GetEntry("DeleteMetadataValue"), nullptr);
    ASSERT_NE(section->GetEntry("DeleteMetadataValue.type"), nullptr);

    EXPECT_TRUE(config.DeleteEntry("DeleteMetadataValue"));
    ASSERT_TRUE(LoadRegistryConfig(registry));
    section = registry.GetSubSection(
        "CurrentUser.Software.Virtools.UserConfig.BallanceTest.Unit.DeleteMetadata", TRUE);
    ASSERT_NE(section, nullptr);
    EXPECT_EQ(section->GetEntry("DeleteMetadataValue"), nullptr);
    EXPECT_EQ(section->GetEntry("DeleteMetadataValue.type"), nullptr);
    config.DeleteSection("Unit/DeleteMetadata");
}

TEST(VxConfigRegistryTest, RegistryFilePathUsesBallanceName) {
    std::vector<char> path = MakeRegistryFilePathBuffer();
    ASSERT_FALSE(path.empty());
    ASSERT_TRUE(VxConfig::GetRegistryFilePath(path.data(), path.size()));
    const std::string registryPath(path.data());
    EXPECT_NE(registryPath.find("Ballance"), std::string::npos);
    EXPECT_NE(registryPath.find("registry.ini"), std::string::npos);
}

TEST(VxConfigRegistryTest, RegistryFilePathRejectsSmallBufferWithoutOverflow) {
    std::vector<char> storage(256, 'Z');
    char *path = storage.data() + 32;

    EXPECT_FALSE(VxConfig::GetRegistryFilePath(path, 4));
    for (size_t i = 0; i < storage.size(); ++i)
        EXPECT_EQ(storage[i], 'Z') << "Unexpected write at offset " << i;
}

TEST(VxPlatformFunctionsTest, CurrentDirectoryAndApplicationBasePathAreSeparate) {
    XString currentDirectory = VxGetCurrentDirectory();
    ASSERT_FALSE(currentDirectory.IsEmpty());

    XString modulePath = VxGetModuleFileName(nullptr);
    ASSERT_FALSE(modulePath.IsEmpty());

    std::vector<char> base((size_t)modulePath.Length() + 1u, '\0');
    ASSERT_TRUE(VxGetApplicationBasePath(base.data(), base.size()));
    EXPECT_TRUE(std::filesystem::exists(currentDirectory.CStr()));
    EXPECT_TRUE(std::filesystem::exists(base.data()));
}

TEST(VxPlatformFunctionsTest, FileDirectoryCopyDeleteAndList) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "VxPlatformFunctionsTest";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    const std::filesystem::path src = root / "source.txt";
    const std::filesystem::path dst = root / "copy.txt";
    {
        std::ofstream file(src);
        file << "content";
    }

    ASSERT_TRUE(VxDirectoryExists(root.string().c_str()));
    ASSERT_TRUE(VxFileExists(src.string().c_str()));
    ASSERT_TRUE(VxCopyFile(src.string().c_str(), dst.string().c_str(), TRUE));
    ASSERT_TRUE(VxFileExists(dst.string().c_str()));

    ListContext context;
    ASSERT_TRUE(VxListDirectory(root.string().c_str(), "*.txt", FALSE, CollectEntry, &context));
    EXPECT_GE(context.names.size(), 2u);

    EXPECT_TRUE(VxDeleteFile(dst.string().c_str()));
    EXPECT_FALSE(VxFileExists(dst.string().c_str()));
    std::filesystem::remove_all(root);
}

TEST(VxPlatformFunctionsTest, ListDirectoryHandlesLongDirectoryPath) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "VxPlatformFunctionsLongList";
    const std::filesystem::path longDir = BuildLongDirectory(root, "listed.txt");
    std::error_code error;
    std::filesystem::remove_all(ToLongFilesystemPath(root), error);
    std::filesystem::create_directories(ToLongFilesystemPath(longDir), error);
    if (error)
        GTEST_SKIP() << "Long directory path is not supported by this filesystem: " << error.message();

    const std::filesystem::path file = longDir / "listed.txt";
    {
        std::ofstream stream(ToLongFilesystemPath(file), std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(stream);
        stream << "content";
    }

    ListContext context;
    ASSERT_TRUE(VxListDirectory(ToRuntimePath(longDir).c_str(), "*.txt", FALSE, CollectEntry, &context));
    EXPECT_NE(std::find(context.names.begin(), context.names.end(), "listed.txt"), context.names.end());

    std::filesystem::remove_all(ToLongFilesystemPath(root), error);
}

TEST(VxPlatformFunctionsTest, MemoryStatusAvailable) {
    VxMemoryStatus status;
    ASSERT_TRUE(VxGetMemoryStatus(&status));
    EXPECT_GT(status.TotalPhysical, 0u);
}
