set(VXMATH_PUBLIC_HEADERS
        ${VX_INCLUDE_DIR}/VxMathCompiler.h
        ${VX_INCLUDE_DIR}/FixedSizeAllocator.h
        ${VX_INCLUDE_DIR}/VxMemory.h
        ${VX_INCLUDE_DIR}/VxMathDefines.h
        ${VX_INCLUDE_DIR}/VxMath.h
        ${VX_INCLUDE_DIR}/VxDefines.h
        ${VX_INCLUDE_DIR}/XUtil.h
        ${VX_INCLUDE_DIR}/XP.h
        ${VX_INCLUDE_DIR}/XSmartPtr.h
        ${VX_INCLUDE_DIR}/XString.h
        ${VX_INCLUDE_DIR}/XArray.h
        ${VX_INCLUDE_DIR}/XBitArray.h
        ${VX_INCLUDE_DIR}/XSArray.h
        ${VX_INCLUDE_DIR}/XClassArray.h
        ${VX_INCLUDE_DIR}/XList.h
        ${VX_INCLUDE_DIR}/XHashFun.h
        ${VX_INCLUDE_DIR}/XHashTable.h
        ${VX_INCLUDE_DIR}/XSHashTable.h
        ${VX_INCLUDE_DIR}/XNHashTable.h
        ${VX_INCLUDE_DIR}/XMatrix.h
        ${VX_INCLUDE_DIR}/VxSharedLibrary.h
        ${VX_INCLUDE_DIR}/VxMemoryMappedFile.h
        ${VX_INCLUDE_DIR}/VxMemoryPool.h
        ${VX_INCLUDE_DIR}/CKPathSplitter.h
        ${VX_INCLUDE_DIR}/CKDirectoryParser.h
        ${VX_INCLUDE_DIR}/VxWindowFunctions.h
        ${VX_INCLUDE_DIR}/VxVector.h
        ${VX_INCLUDE_DIR}/Vx2dVector.h
        ${VX_INCLUDE_DIR}/VxMatrix.h
        ${VX_INCLUDE_DIR}/VxBbox.h
        ${VX_INCLUDE_DIR}/VxConfiguration.h
        ${VX_INCLUDE_DIR}/VxQuaternion.h
        ${VX_INCLUDE_DIR}/VxRect.h
        ${VX_INCLUDE_DIR}/VxOBB.h
        ${VX_INCLUDE_DIR}/VxRay.h
        ${VX_INCLUDE_DIR}/VxSphere.h
        ${VX_INCLUDE_DIR}/VxPlane.h
        ${VX_INCLUDE_DIR}/VxIntersect.h
        ${VX_INCLUDE_DIR}/VxDistance.h
        ${VX_INCLUDE_DIR}/VxFrustum.h
        ${VX_INCLUDE_DIR}/VxSIMD.h
        ${VX_INCLUDE_DIR}/VxSIMD.inl
        ${VX_INCLUDE_DIR}/VxVector.inl
        ${VX_INCLUDE_DIR}/VxMatrix.inl
        ${VX_INCLUDE_DIR}/VxQuaternion.inl
        ${VX_INCLUDE_DIR}/VxRay.inl
        ${VX_INCLUDE_DIR}/VxPlane.inl
        ${VX_INCLUDE_DIR}/VxRect.inl
        ${VX_INCLUDE_DIR}/VxBbox.inl
        ${VX_INCLUDE_DIR}/VxFrustum.inl
        ${VX_INCLUDE_DIR}/VxColor.h
        ${VX_INCLUDE_DIR}/VxTimeProfiler.h
        ${VX_INCLUDE_DIR}/VxImageDescEx.h
        ${VX_INCLUDE_DIR}/VxMutex.h
        ${VX_INCLUDE_DIR}/VxThread.h
)

set(VXMATH_PRIVATE_HEADERS
        VxEigenMatrix.h
        VxBlitEngine.h
        NeuQuant.h
)

set(VXMATH_COMMON_SOURCES
        VxMath.cpp
        VxGraphicUtils.cpp
        VxBlitEngine.cpp
        VxProcessor.cpp
        CKPathSplitter.cpp
        CKDirectoryParser.cpp
        VxVector.cpp
        VxConfiguration.cpp
        VxQuaternion.cpp
        VxIntersect_BoxBox.cpp
        VxIntersect_Plane.cpp
        VxIntersect_FaceFace.cpp
        VxIntersect_Frustum.cpp
        VxIntersect_Sphere.cpp
        VxDistance.cpp
        VxMemory.cpp
        XString.cpp
        EscapeURL.cpp
        NeuQuant.cpp
)

if (WIN32)
    set(VXMATH_PLATFORM_SOURCES
            VxSharedLibraryWin32.cpp
            VxMemoryMappedFileWin32.cpp
            VxWindowFunctionsWin32.cpp
            VxTimeProfilerWin32.cpp
            VxMutexWin32.cpp
            VxThreadWin32.cpp
    )
else ()
    set(VXMATH_PLATFORM_SOURCES
            VxSharedLibraryPosix.cpp
            VxMemoryMappedFilePosix.cpp
            VxWindowFunctionsPosix.cpp
            VxTimeProfilerPosix.cpp
            VxMutexPosix.cpp
            VxThreadPosix.cpp
    )
endif ()

set(VXMATH_SOURCES
        ${VXMATH_COMMON_SOURCES}
        ${VXMATH_PLATFORM_SOURCES}
)

function(vx_configure_target TARGET_NAME)
    target_sources(${TARGET_NAME} PRIVATE
            ${VXMATH_SOURCES}
            ${VXMATH_PUBLIC_HEADERS}
            ${VXMATH_PRIVATE_HEADERS}
    )

    target_include_directories(${TARGET_NAME}
            PUBLIC
            $<BUILD_INTERFACE:${VXMATH_INCLUDE_DIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(${TARGET_NAME}
            PUBLIC
            stb
    )
    if (VXMATH_ENABLE_SIMD AND NOT VXMATH_SIMD_LEVEL STREQUAL "NONE" AND TARGET simde)
        target_link_libraries(${TARGET_NAME} PUBLIC simde)
    endif ()

    if (NOT WIN32)
        target_link_libraries(${TARGET_NAME} PRIVATE ${CMAKE_DL_LIBS} pthread)
    endif ()

    set_target_properties(${TARGET_NAME} PROPERTIES
            VERSION ${PROJECT_VERSION}
            SOVERSION ${PROJECT_VERSION_MAJOR}
    )

    # -- SIMD compile flags ----------------------------------------------------
    vx_apply_simd_flags(${TARGET_NAME})
endfunction()

# ===========================================================================
# SIMD flag helper - maps VXMATH_ENABLE_SIMD / VXMATH_SIMD_LEVEL to
# per-target compile definitions and /arch:-mXXX flags.
# ===========================================================================
function(vx_apply_simd_flags TARGET_NAME)
    if (NOT VXMATH_ENABLE_SIMD OR VXMATH_SIMD_LEVEL STREQUAL "NONE")
        target_compile_definitions(${TARGET_NAME} PUBLIC VX_SIMD_NONE=1)
        return()
    endif ()

    set(_level "${VXMATH_SIMD_LEVEL}")

    # AUTO: pick a safe default - SSE2 (baseline for x86/x64)
    if (_level STREQUAL "AUTO")
        set(_level "SSE2")
    endif ()

    # Map level -> MSVC /arch flag, GCC/Clang -m flag, compile definition
    if (_level STREQUAL "AVX2")
        set(_msvc_flag  "/arch:AVX2")
        set(_gcc_flags  "-mavx2;-mfma")
        target_compile_definitions(${TARGET_NAME} PRIVATE VX_SIMD_LEVEL_AVX2=1)
    elseif (_level STREQUAL "AVX")
        set(_msvc_flag  "/arch:AVX")
        set(_gcc_flags  "-mavx")
        target_compile_definitions(${TARGET_NAME} PRIVATE VX_SIMD_LEVEL_AVX=1)
    elseif (_level STREQUAL "SSE4_1")
        # MSVC doesn't have /arch:SSE4.1 - AVX implies SSE4.1
        if (MSVC)
            set(_msvc_flag "")
        endif ()
        set(_gcc_flags  "-msse4.1")
        target_compile_definitions(${TARGET_NAME} PRIVATE VX_SIMD_LEVEL_SSE4_1=1)
    else () # SSE2 (baseline)
        if (MSVC AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(_msvc_flag "/arch:SSE2")
        else ()
            set(_msvc_flag "")
        endif ()
        set(_gcc_flags  "-msse2")
        target_compile_definitions(${TARGET_NAME} PRIVATE VX_SIMD_LEVEL_SSE2=1)
    endif ()

    # Apply flags (PRIVATE - consumers set their own /arch if they want intrinsics)
    if (MSVC AND _msvc_flag)
        target_compile_options(${TARGET_NAME} PRIVATE ${_msvc_flag})
    elseif (NOT MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE ${_gcc_flags})
    endif ()
endfunction()

# Track created targets for export
set(VXMATH_TARGETS)

if (VXMATH_BUILD_SHARED)
    add_library(VxMath SHARED)
    vx_configure_target(VxMath)

    # Shared library specific settings
    set_target_properties(VxMath PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )

    # Ensure the DLL build is not treated as a static build even if VX_LIB is
    # globally defined by a parent project/toolchain.
    target_compile_definitions(VxMath PRIVATE VX_API)
    target_compile_options(VxMath PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/UVX_LIB>
            $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UVX_LIB>
    )

    list(APPEND VXMATH_TARGETS VxMath)
endif ()

if (VXMATH_BUILD_STATIC)
    add_library(VxMathStatic STATIC)
    vx_configure_target(VxMathStatic)

    # Static library specific settings
    set_target_properties(VxMathStatic PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
            POSITION_INDEPENDENT_CODE ON
    )

    # Static library usage: suppress dllimport/dllexport in headers.
    target_compile_definitions(VxMathStatic PUBLIC VX_LIB)

    list(APPEND VXMATH_TARGETS VxMathStatic)
endif ()

# Build the exported-targets list locally *and* propagate to the parent scope.
# The local copy is needed for the install() call below; PARENT_SCOPE alone
# does not set the variable in the current scope.
set(VXMATH_EXPORTED_TARGETS ${VXMATH_TARGETS} ${VXMATH_DEP_EXPORTED_TARGETS})
set(VXMATH_EXPORTED_TARGETS ${VXMATH_EXPORTED_TARGETS} PARENT_SCOPE)

if (VXMATH_INSTALL)
    # Install libraries to Bin directory
    install(TARGETS ${VXMATH_EXPORTED_TARGETS}
            EXPORT VxMathTargets
            RUNTIME DESTINATION Bin
            LIBRARY DESTINATION Bin
            ARCHIVE DESTINATION lib
    )

    # Install headers to include root
    install(FILES ${VXMATH_PUBLIC_HEADERS} DESTINATION include)
endif ()
