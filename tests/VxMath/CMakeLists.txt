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
        VectorReflectReferenceTest.cpp
        VectorReflectScalarReferenceTest.cpp
        VectorAxisRotationReferenceTest.cpp
        VectorAxisRotationScalarReferenceTest.cpp
        VectorExtremaReferenceTest.cpp
        VectorExtremaScalarReferenceTest.cpp
        VectorAbsoluteReferenceTest.cpp
        VectorAbsoluteScalarReferenceTest.cpp
        VectorCoreReferenceTest.cpp
        VectorCoreScalarReferenceTest.cpp
        VectorInterpolationReferenceTest.cpp
        VectorInterpolationScalarReferenceTest.cpp
        Vector4ReferenceTest.cpp
        Vector4ScalarReferenceTest.cpp
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
        PlaneReferenceTest.cpp
        BboxReferenceTest.cpp
        ObbReferenceTest.cpp
        FrustumReferenceTest.cpp
        FrustumFaceReferenceTest.cpp
        FrustumBoxReferenceTest.cpp
        PlaneIntersectionReferenceTest.cpp
        PlaneSystemReferenceTest.cpp
        SphereIntersectionReferenceTest.cpp
        QuadraticReferenceTest.cpp
        AabbIntersectionReferenceTest.cpp
        AabbObbIntersectionReferenceTest.cpp
        ObbIntersectionReferenceTest.cpp
        AabbFaceReferenceTest.cpp
        RayBoxReferenceTest.cpp
        SegmentBoxReferenceTest.cpp
        LineBoxReferenceTest.cpp
        PointInFaceReferenceTest.cpp
        RayFaceReferenceTest.cpp
        SegmentFaceReferenceTest.cpp
        LineFaceReferenceTest.cpp
        GetPointCoefficientsReferenceTest.cpp
        PointLineDistanceReferenceTest.cpp
        PointRayDistanceReferenceTest.cpp
        PointSegmentDistanceReferenceTest.cpp
        LineLineDistanceReferenceTest.cpp
        LineRayDistanceReferenceTest.cpp
        LineSegmentDistanceReferenceTest.cpp
        RayRayDistanceReferenceTest.cpp
        RaySegmentDistanceReferenceTest.cpp
        SegmentSegmentDistanceReferenceTest.cpp
        DistanceAbiReferenceTest.cpp
        MatrixDeterminantReferenceTest.cpp
        MatrixValueReferenceTest.cpp
        MatrixProjectionReferenceTest.cpp
        MatrixAffineMultiplyReferenceTest.cpp
        MatrixFullMultiplyReferenceTest.cpp
        MatrixInverseReferenceTest.cpp
        MatrixPointTransformReferenceTest.cpp
        MatrixRotateReferenceTest.cpp
        MatrixVector4ReferenceTest.cpp
        MatrixManyReferenceTest.cpp
        MatrixRotateManyReferenceTest.cpp
        MatrixStridedReferenceTest.cpp
        MatrixVector4StridedReferenceTest.cpp
        MatrixRotateStridedReferenceTest.cpp
        RawMatrixVectorReferenceTest.cpp
        MatrixTransposeReferenceTest.cpp
        RawMatrixMultiplyReferenceTest.cpp
        MatrixNormReferenceTest.cpp
        MatrixAdjointReferenceTest.cpp
        MatrixPolarReferenceTest.cpp
        MatrixSpectralReferenceTest.cpp
        MatrixTotalReferenceTest.cpp
        MatrixSimpleReferenceTest.cpp
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
vx_add_test(PlaneScalarReferenceTest SOURCES PlaneReferenceTest.cpp)
target_compile_definitions(PlaneScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(PlaneScalarReferenceTest)
vx_add_test(BboxScalarReferenceTest SOURCES BboxReferenceTest.cpp)
target_compile_definitions(BboxScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(BboxScalarReferenceTest)
vx_add_test(ObbScalarReferenceTest SOURCES ObbReferenceTest.cpp)
target_compile_definitions(ObbScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(ObbScalarReferenceTest)
vx_add_test(FrustumScalarReferenceTest SOURCES FrustumReferenceTest.cpp)
target_compile_definitions(FrustumScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(FrustumScalarReferenceTest)
vx_add_test(FrustumFaceScalarReferenceTest SOURCES FrustumFaceReferenceTest.cpp)
target_compile_definitions(FrustumFaceScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(FrustumFaceScalarReferenceTest)
vx_add_test(FrustumBoxScalarReferenceTest SOURCES FrustumBoxReferenceTest.cpp)
target_compile_definitions(FrustumBoxScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(FrustumBoxScalarReferenceTest)
vx_add_test(PlaneIntersectionScalarReferenceTest SOURCES PlaneIntersectionReferenceTest.cpp)
target_compile_definitions(PlaneIntersectionScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(PlaneIntersectionScalarReferenceTest)
vx_add_test(PlaneSystemScalarReferenceTest SOURCES PlaneSystemReferenceTest.cpp)
target_compile_definitions(PlaneSystemScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(PlaneSystemScalarReferenceTest)
vx_add_test(SphereIntersectionScalarReferenceTest SOURCES SphereIntersectionReferenceTest.cpp)
target_compile_definitions(SphereIntersectionScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(SphereIntersectionScalarReferenceTest)
vx_add_test(AabbIntersectionScalarReferenceTest SOURCES AabbIntersectionReferenceTest.cpp)
target_compile_definitions(AabbIntersectionScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(AabbIntersectionScalarReferenceTest)
vx_add_test(AabbObbIntersectionScalarReferenceTest SOURCES AabbObbIntersectionReferenceTest.cpp)
target_compile_definitions(AabbObbIntersectionScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(AabbObbIntersectionScalarReferenceTest)
vx_add_test(ObbIntersectionScalarReferenceTest SOURCES ObbIntersectionReferenceTest.cpp)
target_compile_definitions(ObbIntersectionScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(ObbIntersectionScalarReferenceTest)
vx_add_test(AabbFaceScalarReferenceTest SOURCES AabbFaceReferenceTest.cpp)
target_compile_definitions(AabbFaceScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(AabbFaceScalarReferenceTest)
vx_add_test(RayBoxScalarReferenceTest SOURCES RayBoxReferenceTest.cpp)
target_compile_definitions(RayBoxScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(RayBoxScalarReferenceTest)
vx_add_test(SegmentBoxScalarReferenceTest SOURCES SegmentBoxReferenceTest.cpp)
target_compile_definitions(SegmentBoxScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(SegmentBoxScalarReferenceTest)
vx_add_test(LineBoxScalarReferenceTest SOURCES LineBoxReferenceTest.cpp)
target_compile_definitions(LineBoxScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(LineBoxScalarReferenceTest)
vx_add_test(PointInFaceScalarReferenceTest SOURCES PointInFaceReferenceTest.cpp)
target_compile_definitions(PointInFaceScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(PointInFaceScalarReferenceTest)
vx_add_test(RayFaceScalarReferenceTest SOURCES RayFaceReferenceTest.cpp)
target_compile_definitions(RayFaceScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(RayFaceScalarReferenceTest)
vx_add_test(SegmentFaceScalarReferenceTest SOURCES SegmentFaceReferenceTest.cpp)
target_compile_definitions(SegmentFaceScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(SegmentFaceScalarReferenceTest)
vx_add_test(LineFaceScalarReferenceTest SOURCES LineFaceReferenceTest.cpp)
target_compile_definitions(LineFaceScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(LineFaceScalarReferenceTest)
vx_add_test(PointSegmentDistanceScalarReferenceTest SOURCES PointSegmentDistanceReferenceTest.cpp)
target_compile_definitions(PointSegmentDistanceScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(PointSegmentDistanceScalarReferenceTest)
vx_add_test(PointRayDistanceScalarReferenceTest SOURCES PointRayDistanceReferenceTest.cpp)
target_compile_definitions(PointRayDistanceScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(PointRayDistanceScalarReferenceTest)
vx_add_test(PointLineDistanceScalarReferenceTest SOURCES PointLineDistanceReferenceTest.cpp)
target_compile_definitions(PointLineDistanceScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(PointLineDistanceScalarReferenceTest)
vx_add_test(FaceFaceReferenceTest SOURCES FaceFaceReferenceTest.cpp)
vx_copy_runtime_dependencies(FaceFaceReferenceTest)
vx_add_test(FaceFaceScalarReferenceTest SOURCES FaceFaceReferenceTest.cpp)
target_compile_definitions(FaceFaceScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(FaceFaceScalarReferenceTest)
vx_add_test(GetPointCoefficientsScalarReferenceTest SOURCES GetPointCoefficientsReferenceTest.cpp)
target_compile_definitions(GetPointCoefficientsScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(GetPointCoefficientsScalarReferenceTest)

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

vx_add_test(LineLineDistanceScalarReferenceTest SOURCES LineLineDistanceReferenceTest.cpp)
target_compile_definitions(LineLineDistanceScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(LineLineDistanceScalarReferenceTest)

vx_add_test(LineRayDistanceScalarReferenceTest SOURCES LineRayDistanceReferenceTest.cpp)
target_compile_definitions(LineRayDistanceScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(LineRayDistanceScalarReferenceTest)

vx_add_test(LineSegmentDistanceScalarReferenceTest SOURCES LineSegmentDistanceReferenceTest.cpp)
target_compile_definitions(LineSegmentDistanceScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(LineSegmentDistanceScalarReferenceTest)

vx_add_test(RayRayDistanceScalarReferenceTest SOURCES RayRayDistanceReferenceTest.cpp)
target_compile_definitions(RayRayDistanceScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(RayRayDistanceScalarReferenceTest)

vx_add_test(RaySegmentDistanceScalarReferenceTest SOURCES RaySegmentDistanceReferenceTest.cpp)
target_compile_definitions(RaySegmentDistanceScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(RaySegmentDistanceScalarReferenceTest)

vx_add_test(SegmentSegmentDistanceScalarReferenceTest SOURCES SegmentSegmentDistanceReferenceTest.cpp)
target_compile_definitions(SegmentSegmentDistanceScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(SegmentSegmentDistanceScalarReferenceTest)

vx_add_test(MatrixDeterminantScalarReferenceTest SOURCES MatrixDeterminantReferenceTest.cpp)
target_compile_definitions(MatrixDeterminantScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(MatrixDeterminantScalarReferenceTest)

vx_add_test(MatrixValueScalarReferenceTest SOURCES MatrixValueReferenceTest.cpp)
target_compile_definitions(MatrixValueScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(MatrixValueScalarReferenceTest)

vx_add_test(MatrixProjectionScalarReferenceTest SOURCES MatrixProjectionReferenceTest.cpp)
target_compile_definitions(MatrixProjectionScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(MatrixProjectionScalarReferenceTest)

vx_add_test(MatrixAffineMultiplyScalarReferenceTest SOURCES MatrixAffineMultiplyReferenceTest.cpp)
target_compile_definitions(MatrixAffineMultiplyScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(MatrixAffineMultiplyScalarReferenceTest)

vx_add_test(MatrixFullMultiplyScalarReferenceTest SOURCES MatrixFullMultiplyReferenceTest.cpp)
target_compile_definitions(MatrixFullMultiplyScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(MatrixFullMultiplyScalarReferenceTest)

vx_add_test(MatrixInverseScalarReferenceTest SOURCES MatrixInverseReferenceTest.cpp)
target_compile_definitions(MatrixInverseScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(MatrixInverseScalarReferenceTest)

vx_add_test(MatrixPointTransformScalarReferenceTest SOURCES MatrixPointTransformReferenceTest.cpp)
target_compile_definitions(MatrixPointTransformScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(MatrixPointTransformScalarReferenceTest)

vx_add_test(MatrixRotateScalarReferenceTest SOURCES MatrixRotateReferenceTest.cpp)
target_compile_definitions(MatrixRotateScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(MatrixRotateScalarReferenceTest)

vx_add_test(MatrixManyScalarReferenceTest SOURCES MatrixManyReferenceTest.cpp)
target_compile_definitions(MatrixManyScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(MatrixManyScalarReferenceTest)

vx_add_test(MatrixRotateManyScalarReferenceTest SOURCES MatrixRotateManyReferenceTest.cpp)
target_compile_definitions(MatrixRotateManyScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(MatrixRotateManyScalarReferenceTest)

vx_add_test(MatrixStridedScalarReferenceTest SOURCES MatrixStridedReferenceTest.cpp)
target_compile_definitions(MatrixStridedScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(MatrixStridedScalarReferenceTest)

vx_add_test(MatrixVector4StridedScalarReferenceTest SOURCES MatrixVector4StridedReferenceTest.cpp)
target_compile_definitions(MatrixVector4StridedScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(MatrixVector4StridedScalarReferenceTest)

vx_add_test(MatrixVector4ScalarReferenceTest SOURCES MatrixVector4ReferenceTest.cpp)
target_compile_definitions(MatrixVector4ScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(MatrixVector4ScalarReferenceTest)

vx_add_test(MatrixRotateStridedScalarReferenceTest SOURCES MatrixRotateStridedReferenceTest.cpp)
target_compile_definitions(MatrixRotateStridedScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(MatrixRotateStridedScalarReferenceTest)

vx_add_test(MatrixTransposeScalarReferenceTest SOURCES MatrixTransposeReferenceTest.cpp)
target_compile_definitions(MatrixTransposeScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(MatrixTransposeScalarReferenceTest)

vx_add_test(MatrixNormScalarReferenceTest SOURCES MatrixNormReferenceTest.cpp)
target_compile_definitions(MatrixNormScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(MatrixNormScalarReferenceTest)

vx_add_test(MatrixAdjointScalarReferenceTest SOURCES MatrixAdjointReferenceTest.cpp)
target_compile_definitions(MatrixAdjointScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(MatrixAdjointScalarReferenceTest)

vx_add_test(MatrixPolarScalarReferenceTest SOURCES MatrixPolarReferenceTest.cpp)
target_compile_definitions(MatrixPolarScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(MatrixPolarScalarReferenceTest)

vx_add_test(MatrixSpectralScalarReferenceTest SOURCES MatrixSpectralReferenceTest.cpp)
target_compile_definitions(MatrixSpectralScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(MatrixSpectralScalarReferenceTest)

vx_add_test(MatrixTotalScalarReferenceTest SOURCES MatrixTotalReferenceTest.cpp)
target_compile_definitions(MatrixTotalScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(MatrixTotalScalarReferenceTest)

vx_add_test(MatrixSimpleScalarReferenceTest SOURCES MatrixSimpleReferenceTest.cpp)
target_compile_definitions(MatrixSimpleScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(MatrixSimpleScalarReferenceTest)

vx_add_test(MatrixEulerReferenceTest SOURCES MatrixEulerReferenceTest.cpp)
vx_copy_runtime_dependencies(MatrixEulerReferenceTest)

vx_add_test(MatrixEulerScalarReferenceTest SOURCES MatrixEulerReferenceTest.cpp)
target_compile_definitions(MatrixEulerScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(MatrixEulerScalarReferenceTest)

vx_add_test(MatrixAxisRotationReferenceTest SOURCES MatrixAxisRotationReferenceTest.cpp)
vx_copy_runtime_dependencies(MatrixAxisRotationReferenceTest)

vx_add_test(MatrixAxisRotationScalarReferenceTest SOURCES MatrixAxisRotationReferenceTest.cpp)
target_compile_definitions(MatrixAxisRotationScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(MatrixAxisRotationScalarReferenceTest)

vx_add_test(MatrixVectorOperatorReferenceTest SOURCES MatrixVectorOperatorReferenceTest.cpp)
vx_copy_runtime_dependencies(MatrixVectorOperatorReferenceTest)

vx_add_test(MatrixVectorOperatorScalarReferenceTest SOURCES MatrixVectorOperatorReferenceTest.cpp)
target_compile_definitions(MatrixVectorOperatorScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(MatrixVectorOperatorScalarReferenceTest)

vx_add_test(FloatArrayInterpolationReferenceTest SOURCES FloatArrayInterpolationReferenceTest.cpp)
vx_copy_runtime_dependencies(FloatArrayInterpolationReferenceTest)

vx_add_test(VectorArrayInterpolationReferenceTest SOURCES VectorArrayInterpolationReferenceTest.cpp)
vx_copy_runtime_dependencies(VectorArrayInterpolationReferenceTest)

vx_add_test(FillStructureReferenceTest SOURCES FillStructureReferenceTest.cpp)
vx_copy_runtime_dependencies(FillStructureReferenceTest)

vx_add_test(CopyStructureReferenceTest SOURCES CopyStructureReferenceTest.cpp)
vx_copy_runtime_dependencies(CopyStructureReferenceTest)

vx_add_test(IndexedCopyReferenceTest SOURCES IndexedCopyReferenceTest.cpp)
vx_copy_runtime_dependencies(IndexedCopyReferenceTest)

vx_add_test(ProjectBoxZExtentsReferenceTest SOURCES ProjectBoxZExtentsReferenceTest.cpp)
vx_copy_runtime_dependencies(ProjectBoxZExtentsReferenceTest)

vx_add_test(TransformBox2DReferenceTest SOURCES TransformBox2DReferenceTest.cpp)
vx_copy_runtime_dependencies(TransformBox2DReferenceTest)

vx_add_test(BestFitBBoxReferenceTest SOURCES BestFitBBoxReferenceTest.cpp)
vx_copy_runtime_dependencies(BestFitBBoxReferenceTest)

vx_add_test(EigenMatrixReferenceTest SOURCES EigenMatrixReferenceTest.cpp)
vx_copy_runtime_dependencies(EigenMatrixReferenceTest)

vx_add_test(EigenMatrixScalarReferenceTest SOURCES EigenMatrixReferenceTest.cpp)
target_compile_definitions(EigenMatrixScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(EigenMatrixScalarReferenceTest)

vx_add_test(EigenMatrixLifetimeTest SOURCES EigenMatrixLifetimeTest.cpp)
vx_copy_runtime_dependencies(EigenMatrixLifetimeTest)

vx_add_test(EigenMatrixScalarLifetimeTest SOURCES EigenMatrixLifetimeTest.cpp)
target_compile_definitions(EigenMatrixScalarLifetimeTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(EigenMatrixScalarLifetimeTest)

vx_add_test(VectorNormalizationReferenceTest SOURCES VectorNormalizationReferenceTest.cpp)
vx_copy_runtime_dependencies(VectorNormalizationReferenceTest)

vx_add_test(VectorNormalizationScalarReferenceTest SOURCES VectorNormalizationReferenceTest.cpp)
target_compile_definitions(VectorNormalizationScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(VectorNormalizationScalarReferenceTest)

vx_add_test(FreeVectorNormalizationReferenceTest SOURCES FreeVectorNormalizationReferenceTest.cpp)
vx_copy_runtime_dependencies(FreeVectorNormalizationReferenceTest)

vx_add_test(FreeVectorNormalizationScalarReferenceTest SOURCES FreeVectorNormalizationReferenceTest.cpp)
target_compile_definitions(FreeVectorNormalizationScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(FreeVectorNormalizationScalarReferenceTest)

vx_add_test(VectorLengthReferenceTest SOURCES VectorLengthReferenceTest.cpp)
vx_copy_runtime_dependencies(VectorLengthReferenceTest)

vx_add_test(VectorLengthScalarReferenceTest SOURCES VectorLengthReferenceTest.cpp)
target_compile_definitions(VectorLengthScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(VectorLengthScalarReferenceTest)

vx_add_test(VectorInverseReferenceTest SOURCES VectorInverseReferenceTest.cpp)
vx_copy_runtime_dependencies(VectorInverseReferenceTest)

vx_add_test(VectorInverseScalarReferenceTest SOURCES VectorInverseReferenceTest.cpp)
target_compile_definitions(VectorInverseScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(VectorInverseScalarReferenceTest)

vx_add_test(VectorDotReferenceTest SOURCES VectorDotReferenceTest.cpp)
vx_copy_runtime_dependencies(VectorDotReferenceTest)

vx_add_test(VectorDotScalarReferenceTest SOURCES VectorDotReferenceTest.cpp)
target_compile_definitions(VectorDotScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(VectorDotScalarReferenceTest)

vx_add_test(VectorCrossReferenceTest SOURCES VectorCrossReferenceTest.cpp)
vx_copy_runtime_dependencies(VectorCrossReferenceTest)

vx_add_test(VectorCrossScalarReferenceTest SOURCES VectorCrossReferenceTest.cpp)
target_compile_definitions(VectorCrossScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(VectorCrossScalarReferenceTest)

vx_add_test(CompressedVectorReferenceTest SOURCES CompressedVectorReferenceTest.cpp)
vx_copy_runtime_dependencies(CompressedVectorReferenceTest)

vx_add_test(CompressedVectorScalarReferenceTest SOURCES CompressedVectorScalarReferenceTest.cpp)
vx_copy_runtime_dependencies(CompressedVectorScalarReferenceTest)

vx_add_test(QuaternionClassReferenceTest SOURCES QuaternionClassReferenceTest.cpp)
vx_copy_runtime_dependencies(QuaternionClassReferenceTest)

vx_add_test(QuaternionClassScalarReferenceTest SOURCES QuaternionClassScalarReferenceTest.cpp)
vx_copy_runtime_dependencies(QuaternionClassScalarReferenceTest)

vx_add_test(QuaternionFunctionsReferenceTest SOURCES QuaternionFunctionsReferenceTest.cpp)
vx_copy_runtime_dependencies(QuaternionFunctionsReferenceTest)

vx_add_test(QuaternionFunctionsScalarReferenceTest SOURCES QuaternionFunctionsScalarReferenceTest.cpp)
vx_copy_runtime_dependencies(QuaternionFunctionsScalarReferenceTest)

vx_add_test(QuaternionPhaseReferenceTest SOURCES QuaternionPhaseReferenceTest.cpp)
vx_copy_runtime_dependencies(QuaternionPhaseReferenceTest)

vx_add_test(QuaternionPhaseScalarReferenceTest SOURCES QuaternionPhaseScalarReferenceTest.cpp)
vx_copy_runtime_dependencies(QuaternionPhaseScalarReferenceTest)

vx_add_test(RawQuaternionNormalizeReferenceTest SOURCES RawQuaternionNormalizeReferenceTest.cpp)
vx_copy_runtime_dependencies(RawQuaternionNormalizeReferenceTest)

vx_add_test(StridedDataReferenceTest SOURCES StridedDataReferenceTest.cpp)
vx_copy_runtime_dependencies(StridedDataReferenceTest)

vx_add_test(StridedDataScalarReferenceTest SOURCES StridedDataReferenceTest.cpp)
target_compile_definitions(StridedDataScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(StridedDataScalarReferenceTest)

vx_add_test(UVReferenceTest SOURCES UVReferenceTest.cpp)
vx_copy_runtime_dependencies(UVReferenceTest)

vx_add_test(UVScalarReferenceTest SOURCES UVReferenceTest.cpp)
target_compile_definitions(UVScalarReferenceTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(UVScalarReferenceTest)

vx_add_test(PathSplitterReferenceTest SOURCES PathSplitterReferenceTest.cpp)
vx_copy_runtime_dependencies(PathSplitterReferenceTest)

vx_add_test(PathMakerReferenceTest SOURCES PathMakerReferenceTest.cpp)
vx_copy_runtime_dependencies(PathMakerReferenceTest)

vx_add_test(FileExtensionReferenceTest SOURCES FileExtensionReferenceTest.cpp)
vx_copy_runtime_dependencies(FileExtensionReferenceTest)

vx_add_test(DirectoryParserReferenceTest SOURCES DirectoryParserReferenceTest.cpp)
vx_copy_runtime_dependencies(DirectoryParserReferenceTest)

vx_add_test(MemoryMappedFileFailureTest SOURCES MemoryMappedFileFailureTest.cpp)
target_include_directories(MemoryMappedFileFailureTest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../src")
vx_copy_runtime_dependencies(MemoryMappedFileFailureTest)

vx_add_test(MemoryMappedFileReferenceTest SOURCES MemoryMappedFileReferenceTest.cpp)
vx_copy_runtime_dependencies(MemoryMappedFileReferenceTest)

if (WIN32)
    foreach(_variant A B Reject Switch)
        add_library(VxSharedLibraryTest${_variant} SHARED SharedLibraryTestModule.cpp SharedLibraryTestModule.def)
        set_target_properties(VxSharedLibraryTest${_variant} PROPERTIES FOLDER "Tests/Fixtures")
    endforeach()
    target_compile_definitions(VxSharedLibraryTestA PRIVATE VX_LIBRARY_DIRTY_FP=1)
    target_compile_definitions(VxSharedLibraryTestB PRIVATE VX_LIBRARY_TEST_VALUE=29)
    target_compile_definitions(VxSharedLibraryTestReject PRIVATE VX_LIBRARY_DIRTY_FP=1 VX_LIBRARY_REJECT_LOAD=1)
    target_compile_definitions(VxSharedLibraryTestSwitch PRIVATE VX_LIBRARY_DIRTY_DETACH=1)
    vx_add_test(SharedLibraryReferenceTest SOURCES SharedLibraryReferenceTest.cpp)
    target_compile_definitions(SharedLibraryReferenceTest PRIVATE
            VX_SHARED_TEST_A="$<TARGET_FILE:VxSharedLibraryTestA>"
            VX_SHARED_TEST_B="$<TARGET_FILE:VxSharedLibraryTestB>"
            VX_SHARED_TEST_REJECT="$<TARGET_FILE:VxSharedLibraryTestReject>")
    add_dependencies(SharedLibraryReferenceTest VxSharedLibraryTestA VxSharedLibraryTestB VxSharedLibraryTestReject)
    vx_copy_runtime_dependencies(SharedLibraryReferenceTest)
    vx_add_test(SharedLibraryReloadTest SOURCES SharedLibraryReloadTest.cpp)
    target_compile_definitions(SharedLibraryReloadTest PRIVATE
            VX_SHARED_TEST_SWITCH=\"$<TARGET_FILE:VxSharedLibraryTestSwitch>\"
            VX_SHARED_TEST_B=\"$<TARGET_FILE:VxSharedLibraryTestB>\"
            VX_SHARED_TEST_REJECT=\"$<TARGET_FILE:VxSharedLibraryTestReject>\")
    add_dependencies(SharedLibraryReloadTest VxSharedLibraryTestSwitch VxSharedLibraryTestB VxSharedLibraryTestReject)
    vx_copy_runtime_dependencies(SharedLibraryReloadTest)
endif()

vx_add_test(QuaternionRangeRegressionTest SOURCES QuaternionRangeRegressionTest.cpp)
vx_copy_runtime_dependencies(QuaternionRangeRegressionTest)
vx_add_test(QuaternionRangeScalarRegressionTest SOURCES QuaternionRangeRegressionTest.cpp)
target_compile_definitions(QuaternionRangeScalarRegressionTest PRIVATE VX_SIMD_NONE=1)
vx_copy_runtime_dependencies(QuaternionRangeScalarRegressionTest)

vx_add_test(ConfigurationEntryReferenceTest SOURCES ConfigurationEntryReferenceTest.cpp)
vx_copy_runtime_dependencies(ConfigurationEntryReferenceTest)

vx_add_test(ConfigurationSectionReferenceTest SOURCES ConfigurationSectionReferenceTest.cpp)
vx_copy_runtime_dependencies(ConfigurationSectionReferenceTest)

vx_add_test(ConfigurationReferenceTest SOURCES ConfigurationReferenceTest.cpp)
vx_copy_runtime_dependencies(ConfigurationReferenceTest)
vx_add_test(ConfigurationParserHelperTest SOURCES ConfigurationParserHelperTest.cpp)
vx_copy_runtime_dependencies(ConfigurationParserHelperTest)

vx_add_test(ConfigStringReferenceTest SOURCES ConfigStringReferenceTest.cpp)
vx_copy_runtime_dependencies(ConfigStringReferenceTest)

vx_add_test(ConfigStateReferenceTest SOURCES ConfigStateReferenceTest.cpp)
vx_copy_runtime_dependencies(ConfigStateReferenceTest)

vx_add_test(ConfigValueReferenceTest SOURCES ConfigValueReferenceTest.cpp)
vx_copy_runtime_dependencies(ConfigValueReferenceTest)

vx_add_test(ConfigTypeDeleteReferenceTest SOURCES ConfigTypeDeleteReferenceTest.cpp)
vx_copy_runtime_dependencies(ConfigTypeDeleteReferenceTest)

vx_add_test(ConfigLifecycleReferenceTest SOURCES ConfigLifecycleReferenceTest.cpp)
vx_copy_runtime_dependencies(ConfigLifecycleReferenceTest)

vx_add_test(ConfigurationCompatibilityTest SOURCES ConfigurationCompatibilityTest.cpp)
vx_copy_runtime_dependencies(ConfigurationCompatibilityTest)
