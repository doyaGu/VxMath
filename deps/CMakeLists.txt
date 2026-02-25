# STB
set(STB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/stb" CACHE INTERNAL "STB include directory")
if(NOT EXISTS "${STB_INCLUDE_DIR}")
    message(WARNING "STB headers not found at ${STB_INCLUDE_DIR}. Please ensure STB is properly downloaded.")
endif()

add_library(stb INTERFACE)
add_library(VxMath::stb ALIAS stb)
target_include_directories(stb INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/stb>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/stb>
)

# Installation
if (VXMATH_INSTALL)
    include(GNUInstallDirs)

    file(GLOB _stb_headers "${CMAKE_CURRENT_SOURCE_DIR}/stb/*.h")
    install(FILES ${_stb_headers}
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/stb"
    )
endif ()

# SIMDE (only needed when SIMD is enabled)
set(VXMATH_DEP_EXPORTED_TARGETS stb)
set(VXMATH_SIMDE_ENABLED OFF)
set(VXMATH_SIMDE_INCLUDE_DIR "")
if (VXMATH_ENABLE_SIMD AND NOT VXMATH_SIMD_LEVEL STREQUAL "NONE")
    add_subdirectory(simde EXCLUDE_FROM_ALL)
    list(APPEND VXMATH_DEP_EXPORTED_TARGETS simde)
    set(VXMATH_SIMDE_ENABLED ON)
    set(VXMATH_SIMDE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/simde")
endif ()

# Export dep targets to parent scope so the top-level CMakeLists.txt can include
# them in the export set.
set(VXMATH_DEP_EXPORTED_TARGETS ${VXMATH_DEP_EXPORTED_TARGETS} PARENT_SCOPE)
set(VXMATH_SIMDE_ENABLED ${VXMATH_SIMDE_ENABLED} PARENT_SCOPE)
set(VXMATH_SIMDE_INCLUDE_DIR "${VXMATH_SIMDE_INCLUDE_DIR}" PARENT_SCOPE)
