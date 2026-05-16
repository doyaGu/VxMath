#include "gtest/gtest.h"
#include "VxMath.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// A test fixture to manage temporary resources like files and windows.
class VxWindowFunctionsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory for filesystem tests
        m_tempDir = std::filesystem::temp_directory_path() / "VxMath_Test_Temp";
        std::filesystem::create_directories(m_tempDir);

    }

    void TearDown() override {
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
};

// --- Keyboard and Input Tests ---

TEST_F(VxWindowFunctionsTest, ScanCodeToAscii) {
    // Test without shift key
    unsigned char keyState[256] = {0};
    XDWORD scanCodeA = 4; // SDL_SCANCODE_A
    EXPECT_EQ(VxScanCodeToAscii(scanCodeA, keyState), 'a');

    keyState[42] = 0x80; // SDL_SCANCODE_LSHIFT
    EXPECT_EQ(VxScanCodeToAscii(scanCodeA, keyState), 'a');

    // Test a non-printable key
    keyState[42] = 0;
    XDWORD scanCodeF1 = 58; // SDL_SCANCODE_F1
    EXPECT_EQ(VxScanCodeToAscii(scanCodeF1, keyState), 0);
}

TEST_F(VxWindowFunctionsTest, ScanCodeToName) {
    char keyName[256];
    // This is highly layout-dependent, but we can test special keys
    XDWORD scanCodeLeft = 80; // SDL_SCANCODE_LEFT
    EXPECT_GT(VxScanCodeToName(scanCodeLeft, keyName), 0);
    // The exact name can vary, but it should be something like "Left" or "Left Arrow"
    std::string name(keyName);
    EXPECT_NE(name.find("Left"), std::string::npos);
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
    char originalDir[MAX_PATH];
    ASSERT_TRUE(VxGetCurrentDirectory(originalDir));

    // Set current directory to our temp path
    ASSERT_TRUE(VxSetCurrentDirectory(m_tempDir.string().c_str()));

    // Verify it was set
    char newDir[MAX_PATH];
    ASSERT_TRUE(VxGetCurrentDirectory(newDir));
    EXPECT_EQ(std::filesystem::path(newDir), m_tempDir);

    // Restore original directory
    ASSERT_TRUE(VxSetCurrentDirectory(originalDir));
}

TEST_F(VxWindowFunctionsTest, MakePath) {
    char fullPath[MAX_PATH];
#ifdef _WIN32
    const char *path = "C:\\Temp";
#else
        const char* path = "/tmp";
#endif
    const char *file = "test.txt";

    EXPECT_TRUE(VxMakePath(fullPath, path, file));
    std::filesystem::path expectedPath = std::filesystem::path(path) / file;
    EXPECT_EQ(std::filesystem::path(fullPath), expectedPath);
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
    char modulePath[MAX_PATH];
    size_t pathLen = VxGetModuleFileName(hMod, modulePath, MAX_PATH);
    ASSERT_GT(pathLen, 0);
    EXPECT_TRUE(std::filesystem::exists(modulePath));
}

// --- Tests for functions that are not easily automated ---

// A message box test will block execution, so it's disabled by default.
// To run it, rename the test to remove the "DISABLED_" prefix.
TEST_F(VxWindowFunctionsTest, DISABLED_MessageBox) {
    // This test just ensures the function can be called.
    // It will pop up a message box during the test run.
    VxMessageBox(NULL, (char *) "Test Message", (char *) "VxMath Test", 0);
}
