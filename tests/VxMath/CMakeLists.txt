# VxMath Tests

#=============================================================================
# Ground-truth VxMath.dll setup
#=============================================================================
# Option to use ground-truth DLL from bin/ for testing (default: OFF uses built lib)
option(VXMATH_USE_GROUND_TRUTH_DLL "Link tests against ground-truth bin/VxMath.dll" OFF)

set(_VXMATH_DEFAULT_TEST_TARGET "")
if (TARGET VxMath)
    set(_VXMATH_DEFAULT_TEST_TARGET VxMath)
elseif (TARGET VxMathStatic)
    set(_VXMATH_DEFAULT_TEST_TARGET VxMathStatic)
endif ()

if (NOT _VXMATH_DEFAULT_TEST_TARGET)
    message(FATAL_ERROR "VxMath tests require VxMath or VxMathStatic target")
endif ()

if (VXMATH_USE_GROUND_TRUTH_DLL AND WIN32)
    # Create an IMPORTED target for the ground-truth DLL
    set(GROUND_TRUTH_DLL_PATH "${CMAKE_SOURCE_DIR}/bin/VxMath.dll")
    set(GROUND_TRUTH_LIB_PATH "${CMAKE_SOURCE_DIR}/bin/VxMath.lib")

    if (EXISTS "${GROUND_TRUTH_DLL_PATH}" AND EXISTS "${GROUND_TRUTH_LIB_PATH}")
        add_library(VxMath_GroundTruth SHARED IMPORTED)
        set_target_properties(VxMath_GroundTruth PROPERTIES
                IMPORTED_LOCATION "${GROUND_TRUTH_DLL_PATH}"
                IMPORTED_IMPLIB "${GROUND_TRUTH_LIB_PATH}"
                INTERFACE_INCLUDE_DIRECTORIES "${NEMO_INCLUDE_DIR}"
        )
        set(TEST_TARGET VxMath_GroundTruth)
        message(STATUS "Using ground-truth VxMath.dll for tests: ${GROUND_TRUTH_DLL_PATH}")

        # Copy ground-truth DLL to all possible build output directories
        foreach(CONFIG_TYPE Debug Release RelWithDebInfo MinSizeRel)
            file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${CONFIG_TYPE}")
            file(COPY_FILE "${GROUND_TRUTH_DLL_PATH}" "${CMAKE_BINARY_DIR}/bin/${CONFIG_TYPE}/VxMath.dll" ONLY_IF_DIFFERENT)
        endforeach()
        # Also copy to the non-config bin directory
        file(COPY_FILE "${GROUND_TRUTH_DLL_PATH}" "${CMAKE_BINARY_DIR}/bin/VxMath.dll" ONLY_IF_DIFFERENT)
    else()
        message(WARNING "Ground-truth DLL not found at ${GROUND_TRUTH_DLL_PATH}, falling back to built library")
        set(TEST_TARGET ${_VXMATH_DEFAULT_TEST_TARGET})
    endif()
else()
    set(TEST_TARGET ${_VXMATH_DEFAULT_TEST_TARGET})
endif()

# Function to add VxMath tests
function(vx_add_test TEST_NAME)
    cmake_parse_arguments(TEST "NO_GTEST_MAIN" "" "SOURCES;DEPENDENCIES" ${ARGN})

    if (NOT TEST_SOURCES)
        message(FATAL_ERROR "vx_add_test: SOURCES argument is required")
    endif ()

    add_executable(${TEST_NAME} ${TEST_SOURCES})
    target_compile_definitions(${TEST_NAME} PRIVATE VX_TESTING)
    if (TEST_TARGET STREQUAL "VxMath_GroundTruth")
        target_compile_definitions(${TEST_NAME} PRIVATE VX_USING_GROUND_TRUTH)
    endif()
    set_target_properties(${TEST_NAME} PROPERTIES
            FOLDER "Tests"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    target_include_directories(${TEST_NAME} PRIVATE
            ${NEMO_INCLUDE_DIR}
            ${PROJECT_SOURCE_DIR}/src
            ${STB_INCLUDE_DIR}
    )

    if (TEST_NO_GTEST_MAIN)
        target_link_libraries(${TEST_NAME} PRIVATE
                ${TEST_TARGET}
                ${TEST_DEPENDENCIES}
                gmock
        )
    else ()
        target_link_libraries(${TEST_NAME} PRIVATE
                ${TEST_TARGET}
                ${TEST_DEPENDENCIES}
                gtest_main
                gmock
        )
    endif ()

    # Register with CTest
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

set(VXMATH_TEST_SOURCES
        VectorTest.cpp
        QuaternionTest.cpp
        QuaternionReferenceTest.cpp
        QuaternionConversionTest.cpp
        MatrixTest.cpp
        MatrixDecompositionReferenceTest.cpp
        FrustumTest.cpp
        PlaneTest.cpp
        RayTest.cpp
        RectTest.cpp
        SphereTest.cpp
        GeometryTest.cpp
        IntersectionTest.cpp
        DistanceTest.cpp
        PropertyTests.cpp
        FixedSizeAllocatorTest.cpp
        ArrayTest.cpp
        ListTest.cpp
        HashTableTest.cpp
        StringTest.cpp
        ConfigurationTest.cpp
        PlatformCompatibilityTest.cpp
        AnsiPathBoundaryTest.cpp
        PathParsingTest.cpp
        URLTest.cpp
        MemoryMappedFileTest.cpp
        SystemInfoTest.cpp
        TimeProfilerTest.cpp
        WindowTest.cpp
        ImageTest.cpp
        GraphicImageReferenceTest.cpp
        GraphicAlphaReferenceTest.cpp
        GraphicNormalBumpReferenceTest.cpp
        GraphicMaskReferenceTest.cpp
        GraphicFormatReferenceTest.cpp
        GraphicFormatStringReferenceTest.cpp
        QuantizationSamplingReferenceTest.cpp
        QuantizeImageReferenceTest.cpp
        Paletted32ReferenceTest.cpp
        PackedPaletteReferenceTest.cpp
        GenericBlitReferenceTest.cpp
        ByteLayoutBlitReferenceTest.cpp
        Packed16BlitReferenceTest.cpp
        SameFormatBlitReferenceTest.cpp
        PaletteDispatchReferenceTest.cpp
        PackedPaletteCrossByteReferenceTest.cpp
        DXTPayloadReferenceTest.cpp
        IndexedLifecycleReferenceTest.cpp
        BlitClosureReferenceTest.cpp
        ImageDescriptorReferenceTest.cpp
        ColorReferenceTest.cpp
        Mipmap32ReferenceTest.cpp
        NormalRowCacheReferenceTest.cpp
        GraphicDispatchReferenceTest.cpp
        RectTransformReferenceTest.cpp
        RectTransformScalarReferenceTest.cpp
        RectClipReferenceTest.cpp
        RectCoreReferenceTest.cpp
        Vector2DReferenceTest.cpp
        IntegerRegionReferenceTest.cpp
        SphereReferenceTest.cpp
        RayReferenceTest.cpp
        PlaneCreateReferenceTest.cpp
        # SIMD Tests
        SIMDFeatureTest.cpp
        SIMDUtilityTest.cpp
        SIMDVectorTest.cpp
        SIMDMatrixTest.cpp
        SIMDQuaternionTest.cpp
        # BlitEngine Tests
        BlitEngineFormatConversionTest.cpp
        BlitEngineAlphaTest.cpp
        BlitEngineSIMDOverrideTest.cpp
        BlitEngineResizeTest.cpp
        BlitEngineQuantizeTest.cpp
        BlitEnginePalettedTest.cpp
        BlitEngineUpsideDownTest.cpp
        BlitEnginePixelFormatTest.cpp
        # BlitEngine Full Coverage Tests
        BlitEngineEdgeCaseTest.cpp
        BlitEngineContractTest.cpp
        BlitEngineMatrixTest.cpp
)

foreach (TEST_SOURCE ${VXMATH_TEST_SOURCES})
    # Extract test name from source file
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

    # Do NOT add VxMath as a dependency - we use TEST_TARGET (which is VxMath_GroundTruth)
    vx_add_test(${TEST_NAME} SOURCES ${TEST_SOURCE})
    if (TEST_NAME STREQUAL "BlitClosureReferenceTest")
        # NeuQuant is internal; test its real implementation without exporting it.
        target_sources(${TEST_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/src/NeuQuant.cpp)
    endif ()

    # MSVC otherwise reports C++98 and silently skips the hash table move tests.
    if (MSVC AND TEST_NAME STREQUAL "HashTableTest")
        target_compile_options(${TEST_NAME} PRIVATE /Zc:__cplusplus)
    endif ()
    
    # Add special include path for SIMD tests
    if (${TEST_NAME} MATCHES "^SIMD")
        target_include_directories(${TEST_NAME} PRIVATE
                ${PROJECT_SOURCE_DIR}/src
                ${PROJECT_SOURCE_DIR}/deps/simde
                ${CMAKE_CURRENT_SOURCE_DIR}  # For SIMDTestCommon.h
        )
    endif()
    vx_copy_runtime_dependencies(${TEST_NAME})
endforeach ()

target_compile_definitions(AnsiPathBoundaryTest PRIVATE
        VX_PATH_TEST_LIBRARY="$<TARGET_FILE:SDL3::SDL3>")

# Static linkage lets the allocation-failure test intercept the pixel copy's
# operator new[] on Windows as well as Unix.
add_executable(BitmapOwnershipTest BitmapOwnershipTest.cpp)
if (TARGET VxMathStatic AND NOT TEST_TARGET STREQUAL "VxMath_GroundTruth")
    target_link_libraries(BitmapOwnershipTest PRIVATE VxMathStatic gtest_main)
else ()
    target_link_libraries(BitmapOwnershipTest PRIVATE ${TEST_TARGET} gtest_main)
endif ()
set_target_properties(BitmapOwnershipTest PROPERTIES
        FOLDER "Tests"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
vx_copy_runtime_dependencies(BitmapOwnershipTest)
add_test(NAME BitmapOwnershipTest COMMAND BitmapOwnershipTest)

vx_add_test(FontTest SOURCES FontTest.cpp)
vx_copy_runtime_dependencies(FontTest)
if (NOT WIN32)
    vx_prepare_font_assets(_font_directory _font_files)
    target_compile_definitions(FontTest PRIVATE TEST_REGULAR_FONT_FILE="${_font_directory}/NotoSans-Regular.ttf")
    vx_add_test(FontSearchTest SOURCES FontSearchTest.cpp)
    vx_add_font_search(FontSearchTest)
    foreach (_test FontTest FontSearchTest)
        add_custom_command(TARGET ${_test} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${_test}>/data/fonts"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_font_files} "$<TARGET_FILE_DIR:${_test}>/data/fonts"
                COMMAND_EXPAND_LISTS VERBATIM)
        # The font catalog must resolve relative to the executable, not CWD.
        set_tests_properties(${_test} PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
    endforeach ()
    add_executable(FontCollectionTest FontCollectionTest.cpp)
    target_link_libraries(FontCollectionTest PRIVATE ${TEST_TARGET})
    target_compile_definitions(FontCollectionTest PRIVATE TEST_FONT_DIR="${_font_directory}")
    set_target_properties(FontCollectionTest PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/font-collection")
    add_test(NAME FontCollectionReference COMMAND FontCollectionTest --reference)
    add_test(NAME FontCollectionRoundTrip COMMAND FontCollectionTest --collection)
    set_tests_properties(FontCollectionReference PROPERTIES FIXTURES_SETUP FontCollectionPixels)
    set_tests_properties(FontCollectionRoundTrip PROPERTIES FIXTURES_REQUIRED FontCollectionPixels)
endif ()
vx_prepare_font_assets(_font_directory _font_files)
add_test(NAME FontAssetsTest COMMAND ${CMAKE_COMMAND}
        "-DFONT_SOURCE=${_font_directory}"
        "-DFONT_MODULE=${PROJECT_SOURCE_DIR}/cmake/VxFontAssets.cmake"
        "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/font-assets-test"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/FontAssetsTest.cmake")

# Exercise original SDK inline geometry with header SIMD paths disabled.
vx_add_test(SphereScalarReferenceTest SOURCES SphereReferenceTest.cpp)
target_compile_definitions(SphereScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(SphereScalarReferenceTest)
vx_add_test(RayScalarReferenceTest SOURCES RayReferenceTest.cpp)
target_compile_definitions(RayScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(RayScalarReferenceTest)
vx_add_test(PlaneCreateScalarReferenceTest SOURCES PlaneCreateReferenceTest.cpp)
target_compile_definitions(PlaneCreateScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(PlaneCreateScalarReferenceTest)

# Exercise the same original-DLL samples with header SIMD paths disabled.
vx_add_test(QuaternionReferenceScalarTest SOURCES QuaternionReferenceTest.cpp)
target_compile_definitions(QuaternionReferenceScalarTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(QuaternionReferenceScalarTest)
vx_add_test(QuaternionConversionScalarTest SOURCES QuaternionConversionTest.cpp)
target_compile_definitions(QuaternionConversionScalarTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(QuaternionConversionScalarTest)
vx_add_test(MatrixDecompositionReferenceScalarTest SOURCES MatrixDecompositionReferenceTest.cpp)
target_compile_definitions(MatrixDecompositionReferenceScalarTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(MatrixDecompositionReferenceScalarTest)

# Keep registry compatibility tests isolated from the developer's real profile.
set(VXMATH_TEST_HOME "${CMAKE_CURRENT_BINARY_DIR}/test-home")
file(MAKE_DIRECTORY "${VXMATH_TEST_HOME}")
set_tests_properties(PlatformCompatibilityTest PROPERTIES
        ENVIRONMENT "HOME=${VXMATH_TEST_HOME};BALLANCE_USER_CONFIG_HOME=${VXMATH_TEST_HOME}"
)

vx_add_test(
        SIMDDispatchTest
        SOURCES
        SIMDDispatchTest.cpp
        BlitSIMDBackendTestMain.cpp
        NO_GTEST_MAIN
)
target_include_directories(SIMDDispatchTest PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/deps/simde
        ${CMAKE_CURRENT_SOURCE_DIR}
)
vx_copy_runtime_dependencies(SIMDDispatchTest)

vx_add_test(
        BlitEngineSIMDBackendDiffTest
        SOURCES
        BlitEngineSIMDBackendDiffTest.cpp
        BlitSIMDBackendTestMain.cpp
        NO_GTEST_MAIN
)
vx_copy_runtime_dependencies(BlitEngineSIMDBackendDiffTest)

foreach(_backend none sse2 ssse3 sse4_1 avx avx2 auto)
    add_test(
            NAME SIMDDispatchTest_${_backend}
            COMMAND SIMDDispatchTest --simd-backend=${_backend}
    )

    add_test(
            NAME BlitEngineSIMDBackendDiffTest_${_backend}
            COMMAND BlitEngineSIMDBackendDiffTest --simd-backend=${_backend}
    )
endforeach ()
