# =============================================================================
# VxMath - Virtools Math Library
# =============================================================================
cmake_minimum_required(VERSION 3.16)

project(VxMath
        VERSION 0.1.0
        DESCRIPTION "Virtools Math Library"
        HOMEPAGE_URL "https://github.com/doyaGu/VxMath"
        LANGUAGES CXX
)

# =============================================================================
# Determine if top-level project
# =============================================================================
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.21")
    if (PROJECT_IS_TOP_LEVEL)
        set(VXMATH_IS_TOP_LEVEL ON)
    else ()
        set(VXMATH_IS_TOP_LEVEL OFF)
    endif ()
else ()
    if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
        set(VXMATH_IS_TOP_LEVEL ON)
    else ()
        set(VXMATH_IS_TOP_LEVEL OFF)
    endif ()
endif ()

# =============================================================================
# Platform and build checks
# =============================================================================
if (VXMATH_IS_TOP_LEVEL AND PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds not allowed. Run CMake from a separate directory: cmake -B build")
endif ()

if (WIN32)
    set(VXMATH_PLATFORM "Windows")
else ()
    set(VXMATH_PLATFORM "POSIX")
endif ()

# =============================================================================
# C++ standard
# =============================================================================
if (NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif ()
if (NOT DEFINED CMAKE_CXX_STANDARD_REQUIRED)
    set(CMAKE_CXX_STANDARD_REQUIRED YES)
endif ()
if (NOT DEFINED CMAKE_CXX_EXTENSIONS)
    set(CMAKE_CXX_EXTENSIONS NO)
endif ()

# =============================================================================
# Project options
# =============================================================================
option(VXMATH_BUILD_TESTS "Build the test suite" OFF)
option(VXMATH_BUILD_SHARED "Build shared library" ON)
option(VXMATH_BUILD_STATIC "Build static library" OFF)
option(VXMATH_INSTALL "Generate install target" ${VXMATH_IS_TOP_LEVEL})

if (NOT VXMATH_BUILD_SHARED AND NOT VXMATH_BUILD_STATIC)
    message(FATAL_ERROR "At least one of VXMATH_BUILD_SHARED or VXMATH_BUILD_STATIC must be enabled")
endif ()

# =============================================================================
# CMake modules
# =============================================================================
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(GenerateExportHeader)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# =============================================================================
# Top-level project settings
# =============================================================================
if (VXMATH_IS_TOP_LEVEL)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMakeTargets")

    get_property(IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if (NOT IS_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
        message(STATUS "[VxMath] Setting build type to 'Release'")
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the build type" FORCE)
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
    endif ()

    if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
        set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/install" CACHE PATH "Installation directory" FORCE)
        message(STATUS "[VxMath] Setting install directory to ${CMAKE_INSTALL_PREFIX}")
    endif ()

    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif ()

# =============================================================================
# Compiler settings
# =============================================================================
if (MSVC)
    add_compile_definitions(
            _CRT_SECURE_NO_WARNINGS
            _CRT_NONSTDC_NO_WARNINGS
            _CRT_SECURE_NO_DEPRECATE
            _CRT_NONSTDC_NO_DEPRECATE
    )
endif ()

# =============================================================================
# Cache internal variables
# =============================================================================
set(VXMATH_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include" CACHE INTERNAL "VxMath include directory")
set(VXMATH_SOURCE_DIR "${PROJECT_SOURCE_DIR}/src" CACHE INTERNAL "VxMath source directory")

# Backward compatibility
set(VX_INCLUDE_DIR "${VXMATH_INCLUDE_DIR}" CACHE INTERNAL "")
set(VX_SOURCE_DIR "${VXMATH_SOURCE_DIR}" CACHE INTERNAL "")

# =============================================================================
# Subdirectories
# =============================================================================
add_subdirectory(deps)
add_subdirectory(src)

if (VXMATH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

# =============================================================================
# Export and packaging
# =============================================================================
if (VXMATH_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    export(TARGETS ${VXMATH_EXPORTED_TARGETS}
            FILE "${CMAKE_CURRENT_BINARY_DIR}/VxMathTargets.cmake"
    )

    install(EXPORT VxMathTargets
            FILE VxMathTargets.cmake
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/VxMath
    )

    configure_package_config_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VxMathConfig.cmake.in"
            "${CMAKE_CURRENT_BINARY_DIR}/VxMathConfig.cmake"
            INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/VxMath
    )

    write_basic_package_version_file(
            "${CMAKE_CURRENT_BINARY_DIR}/VxMathConfigVersion.cmake"
            VERSION ${PROJECT_VERSION}
            COMPATIBILITY SameMajorVersion
    )

    install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/VxMathConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/VxMathConfigVersion.cmake"
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/VxMath
    )

    if (VXMATH_IS_TOP_LEVEL)
        export(PACKAGE VxMath)
    endif ()
endif ()

# =============================================================================
# Configuration summary
# =============================================================================
if (VXMATH_IS_TOP_LEVEL)
    message(STATUS "")
    message(STATUS "============================================================")
    message(STATUS "VxMath Configuration Summary")
    message(STATUS "============================================================")
    message(STATUS "  Version:              ${PROJECT_VERSION}")
    message(STATUS "  Build Type:           ${CMAKE_BUILD_TYPE}")
    message(STATUS "  Build Shared:         ${VXMATH_BUILD_SHARED}")
    message(STATUS "  Build Static:         ${VXMATH_BUILD_STATIC}")
    message(STATUS "  Build Tests:          ${VXMATH_BUILD_TESTS}")
    message(STATUS "  Platform:             ${VXMATH_PLATFORM}")
    message(STATUS "  Install:              ${VXMATH_INSTALL}")
    message(STATUS "  Install Prefix:       ${CMAKE_INSTALL_PREFIX}")
    message(STATUS "============================================================")
    message(STATUS "")
endif ()
