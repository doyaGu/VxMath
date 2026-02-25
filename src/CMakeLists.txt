include(CheckCXXCompilerFlag)

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
        ${VX_INCLUDE_DIR}/VxSIMDMode.h
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
        VxBlitInternal.h
        NeuQuant.h
)

set(VXMATH_COMMON_SOURCES
        VxMath.cpp
        VxSIMDDispatch.cpp
        VxBlitDispatchBridge.cpp
        VxGraphicUtils.cpp
        VxBlitKernels.cpp
        VxBlitEngineSSE2.cpp
        VxBlitEngineSSSE3.cpp
        VxBlitEngineClass.cpp
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

set(VX_SIMD_AVX2 OFF)
set(VXMATH_RUNTIME_DISPATCH_SOURCES)
if (VXMATH_ENABLE_SIMD AND NOT VXMATH_SIMD_LEVEL STREQUAL "NONE")
    if (NOT CMAKE_SYSTEM_PROCESSOR MATCHES "ARM|arm|aarch64|AARCH64")
        list(APPEND VXMATH_RUNTIME_DISPATCH_SOURCES
                VxBlitEngineAVX2.cpp
        )
        set(VX_SIMD_AVX2 ON)

        if (MSVC)
            set_source_files_properties(
                    VxBlitEngineAVX2.cpp
                    PROPERTIES COMPILE_OPTIONS "/arch:AVX2"
            )
        else ()
            set_source_files_properties(
                    VxBlitEngineAVX2.cpp
                    PROPERTIES COMPILE_OPTIONS "-mavx2;-mfma"
            )
        endif ()
    endif ()

    # VxBlitEngineSSSE3.cpp uses SSSE3 intrinsics directly.
    # Add -mssse3 only when:
    #  1) target architecture is x86-family, and
    #  2) the compiler front-end actually accepts -mssse3.
    # Use APPEND so we don't clobber other per-file compile options.
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _vxmath_processor_lc)
    if (NOT MSVC AND _vxmath_processor_lc MATCHES "^(x86_64|amd64|x64|i[3-6]86|x86)$")
        check_cxx_compiler_flag("-mssse3" VXMATH_HAS_MSSSE3_FLAG)
        if (VXMATH_HAS_MSSSE3_FLAG)
            set_property(
                    SOURCE VxBlitEngineSSSE3.cpp
                    APPEND PROPERTY COMPILE_OPTIONS "-mssse3"
            )
        endif ()
    endif ()
endif ()

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
        ${VXMATH_RUNTIME_DISPATCH_SOURCES}
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

    if (VX_SIMD_AVX2)
        target_compile_definitions(${TARGET_NAME} PRIVATE VX_SIMD_AVX2=1)
    endif ()

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

    # AUTO: pick a safe default.
    # MinGW builds include source files using SSSE3/SSE4.1 intrinsics directly.
    if (_level STREQUAL "AUTO")
        if (MINGW)
            set(_level "SSSE3")
        else ()
            set(_level "SSE2")
        endif ()
    endif ()

    # Map level -> MSVC /arch flag, GCC/Clang -m flag, and the cumulative set of
    # VX_SIMD_* feature macros that the build system must advertise when the
    # compiler does not predefine the equivalent __SSEx__ / __FMA__ macros
    # (e.g. MSVC never emits __SSE4_1__ even with /arch:AVX2).
    #
    # Each level promotes all macros from every lower level (cumulative).
    if (_level STREQUAL "AVX2")
        set(_msvc_flag  "/arch:AVX2")
        set(_gcc_flags  "-mavx2;-mfma")
        # AVX2 implies the full SSE family + FMA3.
        target_compile_definitions(${TARGET_NAME} PRIVATE
                VX_SIMD_SSE3=1
                VX_SIMD_SSSE3=1
                VX_SIMD_SSE4_1=1
                VX_SIMD_SSE4_2=1
                VX_SIMD_FMA=1
        )
    elseif (_level STREQUAL "AVX")
        set(_msvc_flag  "/arch:AVX")
        set(_gcc_flags  "-mavx")
        # AVX implies SSE4.2 and the full SSE family; FMA is a separate extension.
        target_compile_definitions(${TARGET_NAME} PRIVATE
                VX_SIMD_SSE3=1
                VX_SIMD_SSSE3=1
                VX_SIMD_SSE4_1=1
                VX_SIMD_SSE4_2=1
        )
    elseif (_level STREQUAL "SSE4_1")
        # MSVC has no /arch:SSE4.1 flag; higher /arch or none is needed.
        if (MSVC)
            set(_msvc_flag "")
        endif ()
        set(_gcc_flags  "-msse4.1")
        target_compile_definitions(${TARGET_NAME} PRIVATE
                VX_SIMD_SSE3=1
                VX_SIMD_SSSE3=1
                VX_SIMD_SSE4_1=1
        )
    elseif (_level STREQUAL "SSSE3")
        # MSVC has no /arch:SSSE3; intrinsics are always available via SIMDe.
        if (MSVC)
            set(_msvc_flag "")
        endif ()
        set(_gcc_flags  "-mssse3")
        target_compile_definitions(${TARGET_NAME} PRIVATE
                VX_SIMD_SSE3=1
                VX_SIMD_SSSE3=1
        )
    else () # SSE2 (baseline) - no extra definitions; SSE/SSE2 are arch-detected.
        if (MSVC AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(_msvc_flag "/arch:SSE2")
        else ()
            set(_msvc_flag "")
        endif ()
        set(_gcc_flags  "-msse2")
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
