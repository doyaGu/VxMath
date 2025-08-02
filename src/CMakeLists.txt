set(VXMATH_PUBLIC_HEADERS
        ${VX_INCLUDE_DIR}/VxMathCompiler.h
        ${VX_INCLUDE_DIR}/VxMathDefines.h
        ${VX_INCLUDE_DIR}/VxMath.h
        ${VX_INCLUDE_DIR}/VxDefines.h
        ${VX_INCLUDE_DIR}/XP.h
        ${VX_INCLUDE_DIR}/XSmartPtr.h
        ${VX_INCLUDE_DIR}/XString.h
        ${VX_INCLUDE_DIR}/XArray.h
        ${VX_INCLUDE_DIR}/XBitArray.h
        ${VX_INCLUDE_DIR}/XSArray.h
        ${VX_INCLUDE_DIR}/XClassArray.h
        ${VX_INCLUDE_DIR}/XList.h
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
        ${VX_INCLUDE_DIR}/VxColor.h
        ${VX_INCLUDE_DIR}/VxTimeProfiler.h
        ${VX_INCLUDE_DIR}/VxImageDescEx.h
        ${VX_INCLUDE_DIR}/VxMutex.h
        ${VX_INCLUDE_DIR}/VxThread.h
)

set(VXMATH_PRIVATE_HEADERS
        VxEigenMatrix.h
)

set(VXMATH_SOURCES
        VxMath.cpp
        VxInterpolation.cpp
        VxStructureUtils.cpp
        VxGraphicUtils.cpp
        VxProcessor.cpp
        VxSharedLibraryWin32.cpp
        VxMemoryMappedFileWin32.cpp
        VxMemoryPool.cpp
        CKPathSplitter.cpp
        CKDirectoryParser.cpp
        VxWindowFunctionsWin32.cpp
        VxColor.cpp
        VxVector.cpp
        VxBbox.cpp
        VxMatrix.cpp
        VxConfiguration.cpp
        VxQuaternion.cpp
        VxRect.cpp
        VxRay.cpp
        VxPlane.cpp
        VxIntersect_BoxBox.cpp
        VxIntersect_Plane.cpp
        VxIntersect_FaceFace.cpp
        VxIntersect_Frustum.cpp
        VxIntersect_Sphere.cpp
        VxDistance.cpp
        VxFrustum.cpp
        VxTimeProfilerWin32.cpp
        VxMutex.cpp
        VxThread.cpp
        VxMemory.cpp
        XString.cpp
        EscapeURL.cpp
)

function(vx_configure_target TARGET_NAME)
    target_sources(${TARGET_NAME} PRIVATE
            ${VXMATH_SOURCES}
            ${VXMATH_PUBLIC_HEADERS}
            ${VXMATH_PRIVATE_HEADERS}
    )

    target_include_directories(${TARGET_NAME}
            PUBLIC
            $<BUILD_INTERFACE:${VX_INCLUDE_DIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    if(DEFINED STB_INCLUDE_DIR AND EXISTS "${STB_INCLUDE_DIR}")
        target_include_directories(${TARGET_NAME} PRIVATE ${STB_INCLUDE_DIR})
    endif()

    set_target_properties(${TARGET_NAME} PROPERTIES
            VERSION ${PROJECT_VERSION}
            SOVERSION ${PROJECT_VERSION_MAJOR}
    )
endfunction()

# Track created targets for export
set(VX_TARGETS)

if(VX_BUILD_SHARED)
    add_library(VxMath SHARED)
    vx_configure_target(VxMath)

    # Shared library specific settings
    set_target_properties(VxMath PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )

    target_include_directories(VxMath PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    target_compile_definitions(VxMath PRIVATE VX_API)

    # Create alias
    add_library(VxMath::VxMath ALIAS VxMath)
    list(APPEND VX_TARGETS VxMath)
endif()

if(VX_BUILD_STATIC)
    add_library(VxMathStatic STATIC)
    vx_configure_target(VxMathStatic)

    # Static library specific settings
    set_target_properties(VxMathStatic PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
            POSITION_INDEPENDENT_CODE ON
    )

    target_compile_definitions(VxMathStatic PUBLIC VX_LIB)

    # Create alias
    add_library(VxMath::Static ALIAS VxMathStatic)
    list(APPEND VX_TARGETS VxMathStatic)
endif()

# Export targets to parent scope
set(VX_EXPORTED_TARGETS ${VX_TARGETS} PARENT_SCOPE)

if(VX_INSTALL)
    # Install libraries
    install(TARGETS ${VX_TARGETS}
            EXPORT VxMathTargets
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # Install headers
    install(FILES ${VXMATH_PUBLIC_HEADERS}
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()