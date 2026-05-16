#include <gtest/gtest.h>

#include "VxMath.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

struct ListContext {
    std::vector<std::string> names;
};

XBOOL CollectEntry(const VxDirectoryEntry *entry, void *userData) {
    ListContext *context = static_cast<ListContext *>(userData);
    context->names.push_back(entry->Name);
    return TRUE;
}

} // namespace

TEST(VxConfigRegistryTest, TypedRoundTripAndDelete) {
    VxConfig config(VXCONFIG_ROOT_CURRENT_USER, "Software\\Virtools\\UserConfig\\BallanceTest");
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
    EXPECT_TRUE(config.DeleteEntry("StringValue"));
    EXPECT_FALSE(config.EntryExists("StringValue"));
    config.CloseSection("Unit\\Registry");
    config.DeleteSection("Unit/Registry");
}

TEST(VxConfigRegistryTest, RegistryFilePathUsesBallanceName) {
    char path[_MAX_PATH] = {0};
    ASSERT_TRUE(VxConfig::GetRegistryFilePath(path, sizeof(path)));
    const std::string registryPath(path);
    EXPECT_NE(registryPath.find("Ballance"), std::string::npos);
    EXPECT_NE(registryPath.find("registry.ini"), std::string::npos);
}

TEST(VxPlatformFunctionsTest, CurrentDirectoryAndApplicationBasePathAreSeparate) {
    char cwd[_MAX_PATH] = {0};
    char base[_MAX_PATH] = {0};
    ASSERT_TRUE(VxGetCurrentDirectory(cwd));
    ASSERT_TRUE(VxGetApplicationBasePath(base));
    EXPECT_TRUE(std::filesystem::exists(cwd));
    EXPECT_TRUE(std::filesystem::exists(base));
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

TEST(VxPlatformFunctionsTest, MemoryStatusAvailable) {
    VxMemoryStatus status;
    ASSERT_TRUE(VxGetMemoryStatus(&status));
    EXPECT_GT(status.TotalPhysical, 0u);
}
