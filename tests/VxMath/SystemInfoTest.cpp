#include "gtest/gtest.h"
#include "VxMath.h"
#include <atomic>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

// Test Fixture for System and Info tests
class SystemInfoTest : public ::testing::Test {};

// Test to ensure processor description is not empty.
TEST_F(SystemInfoTest, GetProcessorDescription_ReturnsNonEmptyString) {
    const char *description = GetProcessorDescription();
    ASSERT_NE(description, nullptr);
    // The description string should not be empty.
    EXPECT_GT(strlen(description), 0);
    // It's a good sign if it contains a known CPU vendor name.
    // This is not a strict requirement but a good heuristic.
    std::string desc_str(description);
    bool has_known_vendor = (desc_str.find("Intel") != std::string::npos) ||
        (desc_str.find("AMD") != std::string::npos) ||
        (desc_str.find("ARM") != std::string::npos);
    // This may fail on very obscure CPUs, but is a reasonable check.
    // We print it for manual verification if the test fails.
    std::cout << "Processor Description: " << description << std::endl;
    // We don't strictly assert this, but it's good to know.
    // ASSERT_TRUE(has_known_vendor);
}

// Test to ensure processor frequency is a plausible positive number.
TEST_F(SystemInfoTest, GetProcessorFrequency_ReturnsPlausibleValue) {
    int frequency = GetProcessorFrequency();
    // The frequency should be a positive number, likely over 100 MHz.
    EXPECT_GT(frequency, 100);
    std::cout << "Processor Frequency: " << frequency << " MHz" << std::endl;
}

// Test to ensure GetProcessorFeatures returns a set of flags.
TEST_F(SystemInfoTest, GetProcessorFeatures_ReturnsNonZeroFlags) {
    XDWORD features = GetProcessorFeatures();
    // A modern processor should have at least some basic features detected.
    // PROC_HASFPU is almost guaranteed on any desktop/server platform.
    EXPECT_NE(features, 0);
    EXPECT_TRUE(features & PROC_HASFPU);
    std::cout << "Processor Features: 0x" << std::hex << features << std::dec << std::endl;
}

// Test for modifying and reading processor features.
TEST_F(SystemInfoTest, ModifyProcessorFeatures_AddsAndRemovesFlags) {
    // Save the original features to restore them later.
    XDWORD original_features = GetProcessorFeatures();

    // --- Test adding a feature ---
    // Let's pretend our CPU has a feature it likely doesn't (PROC_TM - Thermal Monitor)
    // First, ensure it's not already set (or remove it to start clean).
    ModifyProcessorFeatures(0, PROC_TM);
    ASSERT_FALSE(GetProcessorFeatures() & PROC_TM);

    // Now, add the feature.
    ModifyProcessorFeatures(PROC_TM, 0);
    EXPECT_TRUE(GetProcessorFeatures() & PROC_TM);

    // --- Test removing a feature ---
    // Remove the feature we just added.
    ModifyProcessorFeatures(0, PROC_TM);
    EXPECT_FALSE(GetProcessorFeatures() & PROC_TM);

    // --- Test removing a feature that is likely present (like MMX) ---
    // Note: This modifies the global state, which might affect other tests
    // if they run in parallel and depend on this, but for this library it's a global singleton state.
    if (original_features & PROC_MMX) {
        ModifyProcessorFeatures(0, PROC_MMX);
        EXPECT_FALSE(GetProcessorFeatures() & PROC_MMX);
    }

    // --- Test adding and removing at the same time ---
    // Add TM back and remove FPU (if present)
    ModifyProcessorFeatures(PROC_TM, PROC_HASFPU);
    EXPECT_TRUE(GetProcessorFeatures() & PROC_TM);
    EXPECT_FALSE(GetProcessorFeatures() & PROC_HASFPU);

    // Restore original features to not affect other tests.
    ModifyProcessorFeatures(original_features, ~original_features);
    ASSERT_EQ(GetProcessorFeatures(), original_features);
}

// Test to ensure GetProcessorType returns a valid enum value.
TEST_F(SystemInfoTest, GetProcessorType_ReturnsValidEnum) {
    ProcessorsType type = GetProcessorType();
    // The type should not be PROC_UNKNOWN on any machine this test is likely to run on.
    EXPECT_NE(type, PROC_UNKNOWN);
    // It should be within the legacy Virtools processor enum range.
    EXPECT_GE(type, PROC_PENTIUM);
    EXPECT_LE(type, PROC_PSP);

    // Print the architecture type
    const char* archName = "Unknown";
    switch (type) {
        case PROC_PENTIUM:         archName = "Pentium"; break;
        case PROC_PENTIUMMMX:      archName = "Pentium MMX"; break;
        case PROC_PENTIUMPRO:      archName = "Pentium Pro"; break;
        case PROC_K63DNOW:         archName = "K6 3DNow"; break;
        case PROC_PENTIUM2:        archName = "Pentium II"; break;
        case PROC_PENTIUM2XEON:    archName = "Pentium II Xeon"; break;
        case PROC_PENTIUM2CELERON: archName = "Pentium II Celeron"; break;
        case PROC_PENTIUM3:        archName = "Pentium III"; break;
        case PROC_ATHLON:          archName = "Athlon"; break;
        case PROC_PENTIUM4:        archName = "Pentium 4 / x64-class"; break;
        case PROC_PPC_ARM:         archName = "PPC ARM"; break;
        case PROC_PPC_MIPS:        archName = "PPC MIPS"; break;
        case PROC_PPC_G3:          archName = "PowerPC G3"; break;
        case PROC_PPC_G4:          archName = "PowerPC G4"; break;
        case PROC_PSX2:            archName = "PS2"; break;
        case PROC_XBOX2:           archName = "Xbox 2"; break;
        case PROC_PSP:             archName = "PSP"; break;
        default: break;
    }
    std::cout << "Processor Architecture: " << archName << " (enum " << type << ")" << std::endl;
}

TEST_F(SystemInfoTest, GetInstructionSetExtensions_RespectsRuntimeSIMDState) {
    const VxSIMDFeatures &features = VxGetSIMDFeatures();
    const XDWORD extensions = GetInstructionSetExtensions();

    if (!features.AVX) {
        EXPECT_EQ(extensions & (ISEX_AVX | ISEX_AVX2 | ISEX_FMA3 | ISEX_F16C | ISEX_AVXVNNI), 0u);
    }
    if (!features.AVX2) {
        EXPECT_EQ(extensions & ISEX_AVX2, 0u);
    }
    if (!features.AVX512F) {
        EXPECT_EQ(extensions & (ISEX_AVX512F | ISEX_AVX512DQ | ISEX_AVX512BW | ISEX_AVX512VL | ISEX_AVX512VNNI | ISEX_AVX512CD), 0u);
    }
}

// Test OS and Platform detection.
TEST_F(SystemInfoTest, GetOsAndPlatform_ReturnsValidEnums) {
    VX_OSINFO os = VxGetOs();
    // Should not be unknown.
    EXPECT_NE(os, VXOS_UNKNOWN);
    // Should be within the valid range.
    EXPECT_GE(os, VXOS_UNKNOWN);
    EXPECT_LE(os, VXOS_WINSEVEN);

    // Print the OS type
    const char* osName = "Unknown";
    switch (os) {
        case VXOS_WIN31:      osName = "Windows 3.1"; break;
        case VXOS_WIN95:      osName = "Windows 95"; break;
        case VXOS_WIN98:      osName = "Windows 98"; break;
        case VXOS_WINME:      osName = "Windows ME"; break;
        case VXOS_WINNT4:     osName = "Windows NT 4.0"; break;
        case VXOS_WIN2K:      osName = "Windows 2000"; break;
        case VXOS_WINXP:      osName = "Windows XP"; break;
        case VXOS_WINVISTA:   osName = "Windows Vista"; break;
        case VXOS_MACOS9:     osName = "Mac OS 9"; break;
        case VXOS_MACOSX:     osName = "Mac OS X"; break;
        case VXOS_XBOX:       osName = "Xbox"; break;
        case VXOS_LINUXX86:   osName = "Linux x86"; break;
        case VXOS_WINCE1:     osName = "Windows CE 1.0"; break;
        case VXOS_WINCE2:     osName = "Windows CE 2.0"; break;
        case VXOS_WINCE3:     osName = "Windows CE 3.0"; break;
        case VXOS_PSX2:       osName = "PlayStation 2"; break;
        case VXOS_XBOX2:      osName = "Xbox 2"; break;
        case VXOS_PSP:        osName = "PlayStation Portable"; break;
        case VXOS_XBOX360:    osName = "Xbox 360"; break;
        case VXOS_WII:        osName = "Nintendo Wii"; break;
        case VXOS_WINSEVEN:   osName = "Windows 7"; break;
        default: break;
    }
    std::cout << "OS Info: " << osName << " (enum " << os << ")" << std::endl;
}

// Test bit count and shift helper functions.
TEST_F(SystemInfoTest, BitCountAndShiftHelpers) {
    // Test GetBitCount
    EXPECT_EQ(GetBitCount(0x000000FF), 8);  // 8 bits
    EXPECT_EQ(GetBitCount(0x0000F000), 4);  // 4 bits
    EXPECT_EQ(GetBitCount(0x00000000), 0);  // 0 bits
    EXPECT_EQ(GetBitCount(0xFFFFFFFF), 32); // 32 bits

    // Test GetBitShift
    EXPECT_EQ(GetBitShift(0x000000FF), 0);  // Shift 0
    EXPECT_EQ(GetBitShift(0x0000FF00), 8);  // Shift 8
    EXPECT_EQ(GetBitShift(0xFF000000), 24); // Shift 24
    EXPECT_EQ(GetBitShift(0x00F00000), 20); // Shift 20
    EXPECT_EQ(GetBitShift(0x00000000), 0);  // Shift for 0 mask is undefined, but likely 0.
}

// Test the full conversion cycle for all pixel formats.
TEST_F(SystemInfoTest, PixelFormat_ConversionCycle) {
    for (int i = 1; i <= _32_X8L8V8U8; ++i) {
        VX_PIXELFORMAT pf_original = static_cast<VX_PIXELFORMAT>(i);

        // Step 1: Get a description from the format
        VxImageDescEx desc;
        VxPixelFormat2ImageDesc(pf_original, desc);

        // For non-compressed formats, VxBppToMask might be called internally.
        // Let's ensure the masks are plausible.
        if (pf_original < _DXT1) {
            EXPECT_GT(desc.BitsPerPixel, 0);
            if (desc.BitsPerPixel > 8) {
                EXPECT_NE(desc.RedMask | desc.GreenMask | desc.BlueMask, 0);
            }
        }

        // Step 2: Convert the description back to a format
        VX_PIXELFORMAT pf_converted = VxImageDesc2PixelFormat(desc);

        // Step 3: Verify they match
        EXPECT_EQ(pf_original, pf_converted) << "Mismatch for format " << VxPixelFormat2String(pf_original);

        // Step 4: Verify string conversion
        const char *pf_string = VxPixelFormat2String(pf_original);
        printf("pixel format %d: %s\n", pf_original, pf_string);

        ASSERT_NE(pf_string, nullptr);
        EXPECT_GT(strlen(pf_string), 0);
        EXPECT_STRNE(pf_string, "");
    }
}

// Test case for an unknown pixel format string.
TEST_F(SystemInfoTest, VxPixelFormat2String_UnknownFormat) {
    // Use an integer value that is not a valid VX_PIXELFORMAT enum
    const char *empty_str = VxPixelFormat2String(static_cast<VX_PIXELFORMAT>(9999));
    ASSERT_NE(empty_str, nullptr);
    EXPECT_STREQ(empty_str, "");
}

// Test VxPtInRect function
TEST_F(SystemInfoTest, PtInRect) {
    CKRECT rect = {10, 10, 100, 100};

    // Point inside
    CKPOINT pt_inside = {50, 50};
    EXPECT_TRUE(VxPtInRect(&rect, &pt_inside));

    // Point on left edge (inclusive)
    CKPOINT pt_on_left = {10, 50};
    EXPECT_TRUE(VxPtInRect(&rect, &pt_on_left));

    // Point on top edge (inclusive)
    CKPOINT pt_on_top = {50, 10};
    EXPECT_TRUE(VxPtInRect(&rect, &pt_on_top));

    // Point on right edge (inclusive)
    CKPOINT pt_on_right = {100, 50};
    EXPECT_TRUE(VxPtInRect(&rect, &pt_on_right));

    // Point on bottom edge (inclusive)
    CKPOINT pt_on_bottom = {50, 100};
    EXPECT_TRUE(VxPtInRect(&rect, &pt_on_bottom));

    // Point outside
    CKPOINT pt_outside = {200, 200};
    EXPECT_FALSE(VxPtInRect(&rect, &pt_outside));
}

#ifdef _WIN32
namespace {

struct ThreadCaptureContext {
    std::atomic<bool> started{false};
    std::atomic<bool> release{false};
    std::atomic<VxThread *> observed{nullptr};
    std::atomic<XUINTPTR> observedId{0};
};

unsigned int CaptureCurrentThread(void *args) {
    ThreadCaptureContext *context = static_cast<ThreadCaptureContext *>(args);
    context->observed.store(VxThread::GetCurrentVxThread(), std::memory_order_relaxed);
    context->observedId.store(VxThread::GetCurrentVxThreadId(), std::memory_order_relaxed);
    context->started.store(true, std::memory_order_release);

    while (!context->release.load(std::memory_order_acquire)) {
        ::Sleep(1);
    }

    return 123u;
}

bool WaitForStart(const ThreadCaptureContext &context, DWORD timeoutMs) {
    const DWORD deadline = ::GetTickCount() + timeoutMs;
    while (!context.started.load(std::memory_order_acquire)) {
        if (::GetTickCount() >= deadline) {
            return false;
        }
        ::Sleep(1);
    }
    return true;
}

} // namespace

TEST_F(SystemInfoTest, ThreadCreationMarksCreatedAndAllowsPostCreateUpdates) {
    ThreadCaptureContext context;
    VxThread thread;

    EXPECT_FALSE(thread.IsCreated());
    ASSERT_TRUE(thread.CreateThread(&CaptureCurrentThread, &context));
    ASSERT_TRUE(WaitForStart(context, 5000));

    EXPECT_TRUE(thread.IsCreated());

    thread.SetPriority(VXTP_HIGHLEVEL);
    EXPECT_EQ(thread.GetPriority(), static_cast<unsigned int>(VXTP_HIGHLEVEL));

    thread.SetName("worker-thread");
    EXPECT_STREQ(thread.GetName().CStr(), "worker-thread");

    context.release.store(true, std::memory_order_release);

    unsigned int status = 0;
    ASSERT_EQ(thread.Wait(&status, 5000), VXT_OK);
    EXPECT_EQ(status, 123u);

    thread.Close();
    EXPECT_FALSE(thread.IsCreated());
}

TEST_F(SystemInfoTest, GetCurrentVxThreadReturnsWorkerWrapper) {
    ThreadCaptureContext context;
    VxThread thread;

    ASSERT_TRUE(thread.CreateThread(&CaptureCurrentThread, &context));
    ASSERT_TRUE(WaitForStart(context, 5000));

    EXPECT_EQ(context.observed.load(std::memory_order_relaxed), &thread);
    EXPECT_EQ(context.observedId.load(std::memory_order_relaxed), thread.GetID());

    context.release.store(true, std::memory_order_release);

    unsigned int status = 0;
    ASSERT_EQ(thread.Wait(&status, 5000), VXT_OK);
    EXPECT_EQ(status, 123u);
    thread.Close();
}
#endif

// =============================================================================
// Main function to run all tests
