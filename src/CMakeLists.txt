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
            PRIVATE
            simde
            stb
    )

    if (NOT WIN32)
        target_link_libraries(${TARGET_NAME} PRIVATE ${CMAKE_DL_LIBS} pthread)
    endif ()

    set_target_properties(${TARGET_NAME} PROPERTIES
            VERSION ${PROJECT_VERSION}
            SOVERSION ${PROJECT_VERSION_MAJOR}
    )
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

# Export targets to parent scope
set(VXMATH_EXPORTED_TARGETS ${VXMATH_TARGETS} ${VXMATH_DEP_EXPORTED_TARGETS} PARENT_SCOPE)

if (VXMATH_INSTALL)
    # Install libraries to Bin directory
    install(TARGETS ${VXMATH_TARGETS}
            EXPORT VxMathTargets
            RUNTIME DESTINATION Bin
            LIBRARY DESTINATION Bin
            ARCHIVE DESTINATION lib
    )

    # Install headers to include root
    install(FILES ${VXMATH_PUBLIC_HEADERS} DESTINATION include)
endif ()
