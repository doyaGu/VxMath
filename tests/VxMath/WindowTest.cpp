#include "gtest/gtest.h"
#include "VxMath.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

static constexpr size_t kLegacyWindowsPathLimit = 260;

// A test fixture to manage temporary resources like files and windows.
class VxWindowFunctionsTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_originalDir = VxGetCurrentDirectory();
        // Create a temporary directory for filesystem tests
        m_tempDir = std::filesystem::temp_directory_path() / "VxMath_Test_Temp";
        std::filesystem::create_directories(m_tempDir);

    }

    void TearDown() override {
        if (!m_originalDir.IsEmpty())
            VxSetCurrentDirectory(m_originalDir.CStr());
        // Clean up the temporary directory
        std::filesystem::remove_all(m_tempDir);

    }

    // Helper to create a file with specific content in our temp directory
    std::string CreateTempFile(const std::string &filename, const std::string &content) {
        std::filesystem::path filePath = m_tempDir / filename;
        std::ofstream file(filePath);
        file << content;
        file.close();
        return filePath.string();
    }

    std::filesystem::path m_tempDir;
    XString m_originalDir;
};

// --- Keyboard and Input Tests ---

TEST_F(VxWindowFunctionsTest, ScanCodeToAscii) {
    // Test without shift key
    unsigned char keyState[256] = {0};
    XDWORD scanCodeA = 0x1E; // CKKEY_A / DirectInput-style A
    EXPECT_EQ(VxScanCodeToAscii(scanCodeA, keyState), 'a');

    keyState[0x2A] = 0x80; // CKKEY_LSHIFT
    EXPECT_EQ(VxScanCodeToAscii(scanCodeA, keyState), 'A');

    keyState[0x2A] = 0;
    keyState[0x3A] = 0x01; // CKKEY_CAPITAL toggle bit
    EXPECT_EQ(VxScanCodeToAscii(scanCodeA, keyState), 'A');

    keyState[0x2A] = 0x80;
    EXPECT_EQ(VxScanCodeToAscii(scanCodeA, keyState), 'a');

    keyState[0x3A] = 0;
    EXPECT_EQ(VxScanCodeToAscii(0x02, keyState), '!');

    // Test a non-printable key
    keyState[0x2A] = 0;
    XDWORD scanCodeF1 = 0x3B; // CKKEY_F1 / DirectInput-style F1
    EXPECT_EQ(VxScanCodeToAscii(scanCodeF1, keyState), 0);
}

TEST_F(VxWindowFunctionsTest, ScanCodeToName) {
    char keyName[256];
    // This is highly layout-dependent, but we can test special keys
    XDWORD scanCodeLeft = 0xCB; // CKKEY_LEFT / DirectInput-style Left
    EXPECT_GT(VxScanCodeToName(scanCodeLeft, keyName), 0);
    EXPECT_STREQ(keyName, "Left Arrow");

    EXPECT_GT(VxScanCodeToName(0xC9, keyName), 0);
    EXPECT_STREQ(keyName, "PREVIOUS");

    EXPECT_GT(VxScanCodeToName(0xD1, keyName), 0);
    EXPECT_STREQ(keyName, "NEXT");

    EXPECT_GT(VxScanCodeToName(0x4A, keyName), 0);
    EXPECT_STREQ(keyName, "Numpad -");

    EXPECT_GT(VxScanCodeToName(0x4E, keyName), 0);
    EXPECT_STREQ(keyName, "Numpad +");
}

// --- Cursor Management Tests ---

TEST_F(VxWindowFunctionsTest, CursorFunctions) {
    // These functions' effects are visual and hard to automate a check for.
    // The test ensures they can be called without crashing.
    EXPECT_NO_THROW(VxShowCursor(FALSE));
    EXPECT_NO_THROW(VxShowCursor(TRUE));

    EXPECT_TRUE(VxSetCursor(VXCURSOR_NORMALSELECT));
    EXPECT_TRUE(VxSetCursor(VXCURSOR_BUSY));
    EXPECT_TRUE(VxSetCursor(VXCURSOR_MOVE));
    EXPECT_TRUE(VxSetCursor(VXCURSOR_LINKSELECT));
}

// --- FPU Control Word Tests ---
TEST_F(VxWindowFunctionsTest, FPUControlWord) {
    // Get the initial FPU control word
    XWORD initialFpu = VxGetFPUControlWord();

    // Set it to a known state (e.g., the base state) and then back
    EXPECT_NO_THROW(VxSetBaseFPUControlWord());
    XWORD baseFpu = VxGetFPUControlWord();
    EXPECT_NE(baseFpu, 0); // Should not be zero

    // Restore the initial state
    EXPECT_NO_THROW(VxSetFPUControlWord(initialFpu));
    EXPECT_EQ(VxGetFPUControlWord(), initialFpu);
}

// --- Filesystem and Path Tests ---

TEST_F(VxWindowFunctionsTest, FilesystemOperations) {
    // Test GetTempPath
    XString tempPath = VxGetTempPath();
    EXPECT_FALSE(tempPath.IsEmpty());
    EXPECT_TRUE(std::filesystem::exists(tempPath.CStr()));

    // Test MakeDirectory
    std::filesystem::path newDir = m_tempDir / "NewDirectory";
    EXPECT_TRUE(VxMakeDirectory(newDir.string().c_str()));
    EXPECT_TRUE(std::filesystem::exists(newDir));

    // Test RemoveDirectory on an empty directory
    EXPECT_TRUE(VxRemoveDirectory(newDir.string().c_str()));
    EXPECT_FALSE(std::filesystem::exists(newDir));

    // Test DeleteDirectory on a non-empty directory
    std::filesystem::path parentDir = m_tempDir / "Parent";
    std::filesystem::path childFile = parentDir / "child.txt";
    VxMakeDirectory(parentDir.string().c_str());
    CreateTempFile("Parent/child.txt", "content");
    EXPECT_TRUE(std::filesystem::exists(childFile));
    EXPECT_TRUE(VxDeleteDirectory(parentDir.string().c_str()));
    EXPECT_FALSE(std::filesystem::exists(parentDir));

    // Test CreateFileTree
    std::filesystem::path treePath = m_tempDir / "a" / "b" / "c.txt";
    EXPECT_TRUE(VxCreateFileTree(treePath.string().c_str()));
    EXPECT_TRUE(std::filesystem::exists(treePath.parent_path()));
}

TEST_F(VxWindowFunctionsTest, CreateFileTreeFailsWhenIntermediateComponentIsAFile) {
    const std::filesystem::path blocker = m_tempDir / "blocker";
    CreateTempFile("blocker", "content");

    const std::filesystem::path blockedTarget = blocker / "child" / "leaf.txt";
    EXPECT_FALSE(VxCreateFileTree(blockedTarget.string().c_str()));
    EXPECT_TRUE(std::filesystem::is_regular_file(blocker));
    EXPECT_FALSE(std::filesystem::exists(blockedTarget.parent_path()));
}

TEST_F(VxWindowFunctionsTest, CurrentDirectory) {
    XString originalDir = VxGetCurrentDirectory();
    ASSERT_FALSE(originalDir.IsEmpty());

    // Set current directory to our temp path
    ASSERT_TRUE(VxSetCurrentDirectory(m_tempDir.string().c_str()));

    // Verify it was set
    const std::string expectedDir = m_tempDir.string();
    std::vector<char> newDir(4096, '\0');
    ASSERT_TRUE(VxGetCurrentDirectory(newDir.data(), newDir.size()));
    EXPECT_TRUE(std::filesystem::equivalent(std::filesystem::path(newDir.data()), m_tempDir));

    // Restore original directory
    ASSERT_TRUE(VxSetCurrentDirectory(originalDir.CStr()));
}

#ifndef _WIN32
TEST_F(VxWindowFunctionsTest, CurrentDirectoryAcceptsWindowsSeparators) {
    XString originalDir = VxGetCurrentDirectory();
    ASSERT_FALSE(originalDir.IsEmpty());

    std::filesystem::path childDir = m_tempDir / "3D Entities" / "Level";
    std::filesystem::create_directories(childDir);

    std::string windowsStylePath = childDir.string();
    for (char &ch : windowsStylePath) {
        if (ch == '/')
            ch = '\\';
    }

    ASSERT_TRUE(VxSetCurrentDirectory(windowsStylePath.c_str()));

    const std::string expectedDir = childDir.string();
    std::vector<char> newDir(4096, '\0');
    ASSERT_TRUE(VxGetCurrentDirectory(newDir.data(), newDir.size()));
    EXPECT_TRUE(std::filesystem::equivalent(std::filesystem::path(newDir.data()), childDir));

    ASSERT_TRUE(VxSetCurrentDirectory(originalDir.CStr()));
}
#endif

TEST_F(VxWindowFunctionsTest, MakePath) {
#ifdef _WIN32
    const char *path = "C:\\Temp";
#else
        const char* path = "/tmp";
#endif
    const char *file = "test.txt";
    std::filesystem::path expectedPath = std::filesystem::path(path) / file;
    const std::string expectedText = expectedPath.string();
    std::vector<char> fullPath(expectedText.size() + 1u, '\0');

    EXPECT_TRUE(VxMakePath(fullPath.data(), fullPath.size(), path, file));
    EXPECT_EQ(std::filesystem::path(fullPath.data()), expectedPath);
}

TEST_F(VxWindowFunctionsTest, MakePathAcceptsExplicitLongBuffer) {
    std::string longDirectory(kLegacyWindowsPathLimit + 16, 'a');
    const char *file = "test.txt";
    const size_t bufferSize = longDirectory.size() + strlen(file) + 2;
    std::vector<char> fullPath(bufferSize, '\0');

    EXPECT_TRUE(VxMakePath(fullPath.data(), fullPath.size(), longDirectory.c_str(), file));
    EXPECT_GT(strlen(fullPath.data()), kLegacyWindowsPathLimit);
}

TEST_F(VxWindowFunctionsTest, MakePathWritesXString) {
#ifdef _WIN32
    const char *path = "C:\\Temp";
#else
    const char *path = "/tmp";
#endif
    const char *file = "test.txt";
    std::filesystem::path expectedPath = std::filesystem::path(path) / file;

    XString fullPath;
    EXPECT_TRUE(VxMakePath(fullPath, path, file));
    EXPECT_EQ(std::filesystem::path(fullPath.CStr()), expectedPath);
}

TEST_F(VxWindowFunctionsTest, TestDiskSpace) {
    // Check for a small amount of space (should always be available)
    EXPECT_TRUE(VxTestDiskSpace(m_tempDir.string().c_str(), 1024)); // 1 KB

    // Check for an impossibly large amount of space (should fail)
    // Using ULONG_MAX which is guaranteed to be larger than available disk space.
    // EXPECT_FALSE(VxTestDiskSpace(m_tempDir.string().c_str(), ULONG_MAX));
}

// --- Environment and Module Tests ---

TEST_F(VxWindowFunctionsTest, EnvironmentVariable) {
    const char *varName = "VX_MATH_TEST_VAR";
    const char *varValue = "HelloWorld123";
    XString readValue;

    // Test setting a variable
    ASSERT_TRUE(VxSetEnvironmentVariable(varName, varValue));

    // Test getting the variable
    ASSERT_TRUE(VxGetEnvironmentVariable(varName, readValue));
    EXPECT_STREQ(readValue.CStr(), varValue);

    // Clean up by unsetting the variable
    ASSERT_TRUE(VxSetEnvironmentVariable(varName, nullptr));
    EXPECT_FALSE(VxGetEnvironmentVariable(varName, readValue));
}

TEST_F(VxWindowFunctionsTest, UrlEscaping) {
    XString escapedUrl;
    const char *originalUrl = "http://example.com/a path?q=a&b=c";
    const char *expectedEscaped = "http%3A%2F%2Fexample.com%2Fa%20path%3Fq%3Da%26b%3Dc";

    VxEscapeURL(originalUrl, escapedUrl);
    EXPECT_STREQ(escapedUrl.CStr(), expectedEscaped);

    VxUnEscapeUrl(escapedUrl);
    EXPECT_STREQ(escapedUrl.CStr(), originalUrl);
}

TEST_F(VxWindowFunctionsTest, ModuleFunctions) {
    // Get handle to the current executable
    INSTANCE_HANDLE hMod = VxGetModuleHandle(nullptr);
    ASSERT_NE(hMod, nullptr);

    // Get the file name of the current executable
    XString modulePath = VxGetModuleFileName(hMod);
    ASSERT_FALSE(modulePath.IsEmpty());

    std::vector<char> moduleBuffer((size_t)modulePath.Length() + 1u, '\0');
    size_t pathLen = VxGetModuleFileName(hMod, moduleBuffer.data(), moduleBuffer.size());
    ASSERT_GT(pathLen, 0);
    EXPECT_STREQ(moduleBuffer.data(), modulePath.CStr());
    EXPECT_TRUE(std::filesystem::exists(modulePath.CStr()));
}

// --- Tests for functions that are not easily automated ---

// A message box test will block execution, so it's disabled by default.
// To run it, rename the test to remove the "DISABLED_" prefix.
TEST_F(VxWindowFunctionsTest, DISABLED_MessageBox) {
    // This test just ensures the function can be called.
    // It will pop up a message box during the test run.
    VxMessageBox(NULL, (char *) "Test Message", (char *) "VxMath Test", 0);
}
