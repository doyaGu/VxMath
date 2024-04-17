set(VXMATH_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)

set(VXMATH_PUBLIC_HEADERS
        ${VXMATH_INCLUDE_DIR}/VxMathCompiler.h
        ${VXMATH_INCLUDE_DIR}/VxMathDefines.h
        ${VXMATH_INCLUDE_DIR}/VxMath.h
        ${VXMATH_INCLUDE_DIR}/VxDefines.h

        ${VXMATH_INCLUDE_DIR}/XP.h
        ${VXMATH_INCLUDE_DIR}/XSmartPtr.h
        ${VXMATH_INCLUDE_DIR}/XRandomGenerator.h
        ${VXMATH_INCLUDE_DIR}/XString.h
        ${VXMATH_INCLUDE_DIR}/XArray.h
        ${VXMATH_INCLUDE_DIR}/XBitArray.h
        ${VXMATH_INCLUDE_DIR}/XSArray.h
        ${VXMATH_INCLUDE_DIR}/XClassArray.h
        ${VXMATH_INCLUDE_DIR}/XList.h
        ${VXMATH_INCLUDE_DIR}/XBinaryTree.h
        ${VXMATH_INCLUDE_DIR}/XHashTable.h
        ${VXMATH_INCLUDE_DIR}/XSHashTable.h
        ${VXMATH_INCLUDE_DIR}/XNHashTable.h
        ${VXMATH_INCLUDE_DIR}/XPriorityQueue.h
        ${VXMATH_INCLUDE_DIR}/XMatrix.h

        ${VXMATH_INCLUDE_DIR}/VxSharedLibrary.h
        ${VXMATH_INCLUDE_DIR}/VxMemoryMappedFile.h
        ${VXMATH_INCLUDE_DIR}/VxMemoryPool.h
        ${VXMATH_INCLUDE_DIR}/CKPathSplitter.h
        ${VXMATH_INCLUDE_DIR}/CKDirectoryParser.h
        ${VXMATH_INCLUDE_DIR}/VxWindowFunctions.h
        ${VXMATH_INCLUDE_DIR}/VxVector.h
        ${VXMATH_INCLUDE_DIR}/Vx2dVector.h
        ${VXMATH_INCLUDE_DIR}/VxMatrix.h
        ${VXMATH_INCLUDE_DIR}/VxConfiguration.h
        ${VXMATH_INCLUDE_DIR}/VxQuaternion.h
        ${VXMATH_INCLUDE_DIR}/VxRect.h
        ${VXMATH_INCLUDE_DIR}/VxOBB.h
        ${VXMATH_INCLUDE_DIR}/VxRay.h
        ${VXMATH_INCLUDE_DIR}/VxSphere.h
        ${VXMATH_INCLUDE_DIR}/VxPlane.h
        ${VXMATH_INCLUDE_DIR}/VxIntersect.h
        ${VXMATH_INCLUDE_DIR}/VxDistance.h
        ${VXMATH_INCLUDE_DIR}/VxFrustum.h
        ${VXMATH_INCLUDE_DIR}/VxColor.h
        ${VXMATH_INCLUDE_DIR}/VxMemoryPool.h
        ${VXMATH_INCLUDE_DIR}/VxTimeProfiler.h
        ${VXMATH_INCLUDE_DIR}/VxImageDescEx.h

        ${VXMATH_INCLUDE_DIR}/VxMutex.h
        ${VXMATH_INCLUDE_DIR}/VxThread.h
        )

set(VXMATH_PRIVATE_HEADERS
        )

set(VXMATH_SOURCES
        VxMath.cpp
        XString.cpp
        VxSharedLibraryWin32.cpp
        VxMemoryMappedFileWin32.cpp
        VxMemoryPool.cpp
        CKPathSplitter.cpp
        CKDirectoryParser.cpp
        VxWindowFunctionsWin32.cpp
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
        EscapeURL.cpp
        )

add_library(VxMath SHARED ${VXMATH_SOURCES} ${VXMATH_PUBLIC_HEADERS} ${VXMATH_PRIVATE_HEADERS})
set_target_properties(VxMath PROPERTIES DEFINE_SYMBOL VX_API)

add_library(VxMathStatic STATIC ${VXMATH_SOURCES} ${VXMATH_PUBLIC_HEADERS} ${VXMATH_PRIVATE_HEADERS})

foreach (LIB IN ITEMS VxMath VxMathStatic)
    target_include_directories(${LIB}
            PUBLIC
            $<BUILD_INTERFACE:${VXMATH_INCLUDE_DIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
            PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            )

    target_link_libraries(${LIB} PRIVATE cpuinfo)

    # Disable strict const-qualification conformance for pointers initialized by using string literals
    target_compile_options(${LIB} PUBLIC
            $<$<COMPILE_LANGUAGE:CXX>:$<$<CXX_COMPILER_ID:MSVC>:/Zc:strictStrings->>
            $<$<COMPILE_LANGUAGE:CXX>:$<$<CXX_COMPILER_ID:GNU>:-fpermissive>>
            $<$<COMPILE_LANGUAGE:CXX>:$<$<CXX_COMPILER_ID:GNU>:-Wno-write-strings>>
            )

    # Disable msvc unsafe warnings
    target_compile_definitions(${LIB} PRIVATE
            $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
            $<$<C_COMPILER_ID:MSVC>:_CRT_NONSTDC_NO_WARNINGS>
            $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_DEPRECATE>
            $<$<C_COMPILER_ID:MSVC>:_CRT_NONSTDC_NO_DEPRECATE>
            )
endforeach ()
