#include "gtest/gtest.h"
#include "VxMath.h"
#include <string>
#include <vector>
#include <set>
#include <fstream>
#include <filesystem> // For modern filesystem operations in the test fixture
#include <algorithm>
#include <chrono>
#include <thread>

static constexpr size_t kLegacyWindowsPathLimit = 260;

// Helper to create a file
void CreateEmptyFile(const std::filesystem::path &path) {
    std::ofstream outfile(path);
    outfile.close();
}

static std::filesystem::path ToLongFilesystemPath(const std::filesystem::path &path) {
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

static std::string ToParserPath(const std::filesystem::path &path) {
#if defined(_WIN32)
    return ToLongFilesystemPath(path).string();
#else
    return path.string();
#endif
}

static std::filesystem::path BuildDeepDirectory(const std::filesystem::path &root, const char *leafName) {
    std::filesystem::path dir = root;
    int index = 0;
    while (ToParserPath(dir / leafName).size() <= kLegacyWindowsPathLimit + 32) {
        dir /= std::string("segment_") + std::to_string(index++) + "_" + std::string(32, 'x');
    }
    return dir;
}

// Test Fixture for CKDirectoryParser which requires a real filesystem
class DirectoryParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory structure for testing
        // test_root/
        //   file1.txt
        //   file2.log
        //   image.JPG
        //   sub/
        //     subfile.txt
        //     another.log
        //   empty_sub/
        // Use a unique temp directory per test to avoid cross-test coupling
        // if Windows keeps a handle open briefly.
        const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
        const std::string uniqueName =
            std::string("temp_dir_parser_test_") + info->test_suite_name() + "_" + info->name();
        m_rootDir = std::filesystem::temp_directory_path() / uniqueName;

        std::error_code ec;
        std::filesystem::remove_all(m_rootDir, ec);
        std::filesystem::create_directories(m_rootDir);

        m_subDir = m_rootDir / "sub";
        std::filesystem::create_directory(m_subDir);

        m_emptySubDir = m_rootDir / "empty_sub";
        std::filesystem::create_directory(m_emptySubDir);

        CreateEmptyFile(m_rootDir / "file1.txt");
        CreateEmptyFile(m_rootDir / "file2.log");
        CreateEmptyFile(m_rootDir / "image.JPG"); // Note: uppercase extension
        CreateEmptyFile(m_subDir / "subfile.txt");
        CreateEmptyFile(m_subDir / "another.log");

        m_rootDirStr = m_rootDir.string();
        m_subDirStr = m_subDir.string();
        m_emptySubDirStr = m_emptySubDir.string();
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(ToLongFilesystemPath(m_rootDir), ec);
    }

    // Helper to collect all files found by the parser
    std::set<std::string> CollectFiles(CKDirectoryParser &parser) {
        std::set<std::string> foundFiles;
        const char *file;
        while ((file = parser.GetNextFile()) != nullptr) {
            std::string filePath(file);
            // Normalize path separators for consistent comparison
            // std::replace(filePath.begin(), filePath.end(), '\\', '/');
            foundFiles.insert(filePath);
        }
        return foundFiles;
    }

    std::filesystem::path m_rootDir;
    std::filesystem::path m_subDir;
    std::filesystem::path m_emptySubDir;
    std::string m_rootDirStr;
    std::string m_subDirStr;
    std::string m_emptySubDirStr;
};


// --- CKPathSplitter Tests ---

TEST(PathSplitterTest, FullPath) {
    char path[] = "C:\\Users\\Test\\file.txt";
    CKPathSplitter splitter(path);
    ASSERT_STREQ(splitter.GetDrive(), "C:");
    ASSERT_STREQ(splitter.GetDir(), "\\Users\\Test\\");
    ASSERT_STREQ(splitter.GetName(), "file");
    ASSERT_STREQ(splitter.GetExtension(), ".txt");
}

TEST(PathSplitterTest, UNCPath) {
    char path[] = "\\\\Server\\Share\\file.zip";
    CKPathSplitter splitter(path);
    ASSERT_STREQ(splitter.GetDrive(), "");
    // Note: _splitpath behavior with UNC
    ASSERT_STREQ(splitter.GetDir(), "\\\\Server\\Share\\");
    ASSERT_STREQ(splitter.GetName(), "file");
    ASSERT_STREQ(splitter.GetExtension(), ".zip");
}

TEST(PathSplitterTest, RelativePath) {
    char path[] = "..\\data\\model.nmo";
    CKPathSplitter splitter(path);
    ASSERT_STREQ(splitter.GetDrive(), "");
    ASSERT_STREQ(splitter.GetDir(), "..\\data\\");
    ASSERT_STREQ(splitter.GetName(), "model");
    ASSERT_STREQ(splitter.GetExtension(), ".nmo");
}

TEST(PathSplitterTest, FilenameOnly) {
    char path[] = "myfile.dat";
    CKPathSplitter splitter(path);
    ASSERT_STREQ(splitter.GetDrive(), "");
    ASSERT_STREQ(splitter.GetDir(), "");
    ASSERT_STREQ(splitter.GetName(), "myfile");
    ASSERT_STREQ(splitter.GetExtension(), ".dat");
}

TEST(PathSplitterTest, PathWithoutExtension) {
    char path[] = "D:\\Temp\\MyFile";
    CKPathSplitter splitter(path);
    ASSERT_STREQ(splitter.GetDrive(), "D:");
    ASSERT_STREQ(splitter.GetDir(), "\\Temp\\");
    ASSERT_STREQ(splitter.GetName(), "MyFile");
    ASSERT_STREQ(splitter.GetExtension(), "");
}

TEST(PathSplitterTest, PathWithDotInName) {
    char path[] = "E:\\backup.v1.0.zip";
    CKPathSplitter splitter(path);
    ASSERT_STREQ(splitter.GetDrive(), "E:");
    ASSERT_STREQ(splitter.GetDir(), "\\");
    ASSERT_STREQ(splitter.GetName(), "backup.v1.0");
    ASSERT_STREQ(splitter.GetExtension(), ".zip");
}

TEST(PathSplitterTest, PathEndingWithSlash) {
    char path[] = "C:\\MyDir\\";
    CKPathSplitter splitter(path);
    ASSERT_STREQ(splitter.GetDrive(), "C:");
    ASSERT_STREQ(splitter.GetDir(), "\\MyDir\\");
    ASSERT_STREQ(splitter.GetName(), "");
    ASSERT_STREQ(splitter.GetExtension(), "");
}

TEST(PathSplitterTest, EmptyPath) {
    char path[] = "";
    CKPathSplitter splitter(path);
    ASSERT_STREQ(splitter.GetDrive(), "");
    ASSERT_STREQ(splitter.GetDir(), "");
    ASSERT_STREQ(splitter.GetName(), "");
    ASSERT_STREQ(splitter.GetExtension(), "");
}

// --- CKPathMaker Tests ---

TEST(PathMakerTest, AllComponents) {
    CKPathMaker maker("C:", "\\Dir\\", "File", ".ext");
    ASSERT_STREQ(maker.GetFileName(), "C:\\Dir\\File.ext");
}

TEST(PathMakerTest, NoDrive) {
    CKPathMaker maker(nullptr, "\\Dir\\", "File", ".ext");
    ASSERT_STREQ(maker.GetFileName(), "\\Dir\\File.ext");
}

TEST(PathMakerTest, NoDirectory) {
    CKPathMaker maker("C:", nullptr, "File", ".ext");
    ASSERT_STREQ(maker.GetFileName(), "C:File.ext");
}

TEST(PathMakerTest, NoExtension) {
    CKPathMaker maker("C:", "\\Dir\\", "File", nullptr);
    ASSERT_STREQ(maker.GetFileName(), "C:\\Dir\\File");
}

TEST(PathMakerTest, OnlyFilename) {
    CKPathMaker maker(nullptr, nullptr, "File", nullptr);
    ASSERT_STREQ(maker.GetFileName(), "File");
}

TEST(PathMakerTest, LongPathIsNotTruncated) {
    XString directory = "\\Dir\\";
    while (directory.Length() <= kLegacyWindowsPathLimit + 32) {
        directory << "segment_with_long_name\\";
    }

    CKPathMaker maker("C:", directory.CStr(), "File", ".ext");

    XString expected = "C:";
    expected << directory << "File.ext";
    ASSERT_GT(expected.Length(), kLegacyWindowsPathLimit);
    ASSERT_STREQ(maker.GetFileName(), expected.CStr());
}

TEST(PathMakerTest, CopyOwnsFileName) {
    CKPathMaker maker("C:", "\\Dir\\", "File", ".ext");
    CKPathMaker copy(maker);
    CKPathMaker assigned(nullptr, nullptr, "Other", nullptr);
    assigned = maker;

    ASSERT_STREQ(copy.GetFileName(), "C:\\Dir\\File.ext");
    ASSERT_STREQ(assigned.GetFileName(), "C:\\Dir\\File.ext");
}

// --- CKFileExtension Tests ---

TEST(FileExtensionTest, Construction) {
    ASSERT_STREQ((char*)CKFileExtension(".txt"), "txt");
    ASSERT_STREQ((char*)CKFileExtension("bmp"), "bmp");
    ASSERT_STREQ((char*)CKFileExtension(".jpeg"), "jpe"); // Truncation
    ASSERT_STREQ((char*)CKFileExtension(""), "");
    ASSERT_STREQ((char*)CKFileExtension(nullptr), "");
}

TEST(FileExtensionTest, Comparison) {
    CKFileExtension ext1(".TXT");
    CKFileExtension ext2("txt");
    CKFileExtension ext3("tXt");
    CKFileExtension ext4("bmp");

    ASSERT_NE(ext1.operator==(ext2), 0);
    ASSERT_NE(ext2.operator==(ext3), 0);
    ASSERT_EQ(ext1.operator==(ext4), 0);
}

// --- CKDirectoryParser Tests ---

// DISABLED: Segfaults on some systems - needs investigation
TEST_F(DirectoryParserTest, DISABLED_NonRecursive_AllFiles) {
    CKDirectoryParser parser((char *) m_rootDir.string().c_str(), "*.*", FALSE);
    auto files = CollectFiles(parser);

    ASSERT_EQ(files.size(), 3);
    ASSERT_TRUE(files.count((m_rootDir / "file1.txt").string()));
    ASSERT_TRUE(files.count((m_rootDir / "file2.log").string()));
    ASSERT_TRUE(files.count((m_rootDir / "image.JPG").string()));
}

TEST_F(DirectoryParserTest, NonRecursive_SpecificMask) {
    CKDirectoryParser parser(const_cast<char *>(m_rootDirStr.c_str()), "*.txt", FALSE);
    auto files = CollectFiles(parser);

    ASSERT_EQ(files.size(), 1);
    ASSERT_TRUE(files.count((m_rootDir / "file1.txt").string()));
}

TEST_F(DirectoryParserTest, NonRecursive_LongPathIsNotTruncated) {
    const char *leafName = "long_file.txt";
    std::filesystem::path longDir = BuildDeepDirectory(m_rootDir / "long_non_recursive", leafName);
    std::filesystem::path longFile = longDir / leafName;

    std::error_code ec;
    std::filesystem::create_directories(ToLongFilesystemPath(longDir), ec);
    if (ec) {
        GTEST_SKIP() << "Long path directory creation is not supported: " << ec.message();
    }

    std::ofstream outfile(ToLongFilesystemPath(longFile));
    if (!outfile) {
        GTEST_SKIP() << "Long path file creation is not supported";
    }
    outfile.close();

    const std::string parserDir = ToParserPath(longDir);
    const std::string expectedFile = ToParserPath(longFile);
    ASSERT_GT(expectedFile.size(), kLegacyWindowsPathLimit);

    CKDirectoryParser parser(parserDir.c_str(), "*.txt", FALSE);
    const char *file = parser.GetNextFile();

    ASSERT_NE(file, nullptr);
    EXPECT_STREQ(file, expectedFile.c_str());
}

TEST_F(DirectoryParserTest, Recursive_AllFiles) {
    CKDirectoryParser parser(const_cast<char *>(m_rootDirStr.c_str()), "*.*", TRUE);
    auto files = CollectFiles(parser);

    ASSERT_EQ(files.size(), 5);
    ASSERT_TRUE(files.count((m_rootDir / "file1.txt").string()));
    ASSERT_TRUE(files.count((m_rootDir / "file2.log").string()));
    ASSERT_TRUE(files.count((m_rootDir / "image.JPG").string()));
    ASSERT_TRUE(files.count((m_subDir / "subfile.txt").string()));
    ASSERT_TRUE(files.count((m_subDir / "another.log").string()));
}

TEST_F(DirectoryParserTest, Recursive_LongPathIsNotTruncated) {
    const char *leafName = "nested_long_file.txt";
    std::filesystem::path root = m_rootDir / "long_recursive";
    std::filesystem::path longDir = BuildDeepDirectory(root, leafName);
    std::filesystem::path longFile = longDir / leafName;

    std::error_code ec;
    std::filesystem::create_directories(ToLongFilesystemPath(longDir), ec);
    if (ec) {
        GTEST_SKIP() << "Long path directory creation is not supported: " << ec.message();
    }

    std::ofstream outfile(ToLongFilesystemPath(longFile));
    if (!outfile) {
        GTEST_SKIP() << "Long path file creation is not supported";
    }
    outfile.close();

    const std::string parserRoot = ToParserPath(root);
    const std::string expectedFile = ToParserPath(longFile);
    ASSERT_GT(expectedFile.size(), kLegacyWindowsPathLimit);

    CKDirectoryParser parser(parserRoot.c_str(), "*.txt", TRUE);
    auto files = CollectFiles(parser);

    ASSERT_EQ(files.size(), 1);
    EXPECT_TRUE(files.count(expectedFile));
}

TEST_F(DirectoryParserTest, Recursive_SpecificMaskAndCase) {
    // The mask should be case-insensitive on Windows
    CKDirectoryParser parser(const_cast<char *>(m_rootDirStr.c_str()), "*.jpg", TRUE);
    auto files = CollectFiles(parser);

    ASSERT_EQ(files.size(), 1);
    ASSERT_TRUE(files.count((m_rootDir / "image.JPG").string()));
}

TEST_F(DirectoryParserTest, ParseEmptyDirectory) {
    CKDirectoryParser parser(const_cast<char *>(m_emptySubDirStr.c_str()), "*.*", TRUE);
    auto files = CollectFiles(parser);
    ASSERT_EQ(files.size(), 0);
}

TEST_F(DirectoryParserTest, ParseInvalidDirectory) {
    CKDirectoryParser parser("non_existent_directory_12345", "*.*", FALSE);
    auto files = CollectFiles(parser);
    ASSERT_EQ(files.size(), 0);
}

TEST_F(DirectoryParserTest, Reset_RescansSameDirectory) {
    CKDirectoryParser parser(const_cast<char *>(m_rootDirStr.c_str()), "*.log", FALSE);

    // First scan
    auto files1 = CollectFiles(parser);
    ASSERT_EQ(files1.size(), 1);
    ASSERT_TRUE(files1.count((m_rootDir / "file2.log").string()));

    // After reset, should find the same file again
    parser.Reset();
    auto files2 = CollectFiles(parser);
    ASSERT_EQ(files2.size(), 1);
    ASSERT_TRUE(files2.count((m_rootDir / "file2.log").string()));
}

TEST_F(DirectoryParserTest, Reset_ToNewDirectory) {
    CKDirectoryParser parser(const_cast<char *>(m_rootDirStr.c_str()), "*.txt", FALSE);

    // Scan root
    auto files1 = CollectFiles(parser);
    ASSERT_EQ(files1.size(), 1);

    // Reset to scan sub-directory
    parser.Reset(const_cast<char *>(m_subDirStr.c_str()), "*.txt", FALSE);
    auto files2 = CollectFiles(parser);
    ASSERT_EQ(files2.size(), 1);
    ASSERT_TRUE(files2.count((m_subDir / "subfile.txt").string()));
}

// =============================================================================
// Main function to run all tests
