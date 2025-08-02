cmake_minimum_required(VERSION 3.15)

project(VxMath
        VERSION 0.1.0
        LANGUAGES CXX
)

# Only allow out-of-source builds
if (PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there.")
endif ()

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

# Add path for custom modules
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Standard CMake modules
include(GenerateExportHeader)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Use folders to organize targets in an IDE
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMakeTargets")

# Platform checks
if (NOT WIN32)
    message(FATAL_ERROR "Currently only Windows is supported.")
endif ()

# Build type handling
get_property(IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (NOT IS_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting build type to 'Release' as no build type was specified")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif ()

# Install prefix
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/install" CACHE PATH "Installation directory" FORCE)
    message(STATUS "Setting install directory to ${CMAKE_INSTALL_PREFIX}")
endif ()

# Generate compile commands for IDEs/tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Project options
option(VX_BUILD_SHARED "Build shared library" ON)
option(VX_BUILD_STATIC "Build static library" ON)
option(VX_INSTALL "Generate install target" ON)

# Validate options
if (NOT VX_BUILD_SHARED AND NOT VX_BUILD_STATIC)
    message(FATAL_ERROR "At least one of VX_BUILD_SHARED or VX_BUILD_STATIC must be enabled")
endif ()

# Cache internal variables
set(VX_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include" CACHE INTERNAL "")
set(VX_SOURCE_DIR "${PROJECT_SOURCE_DIR}/src" CACHE INTERNAL "")

# Configure compiler-specific settings
# MSVC specific settings
if (MSVC)
    # Disable MSVC unsafe warnings
    add_compile_definitions(
            _CRT_SECURE_NO_WARNINGS
            _CRT_NONSTDC_NO_WARNINGS
            _CRT_SECURE_NO_DEPRECATE
            _CRT_NONSTDC_NO_DEPRECATE
    )

    # MSVC specific compile options
    add_compile_options(
            # Disable strict const-qualification conformance for pointers initialized by string literals
            $<$<COMPILE_LANGUAGE:CXX>:/Zc:strictStrings->
            # Disable char8_t type
            $<$<COMPILE_LANGUAGE:CXX>:/Zc:char8_t->
    )
endif ()

# GCC specific settings
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(
            $<$<COMPILE_LANGUAGE:CXX>:-fpermissive>
            $<$<COMPILE_LANGUAGE:CXX>:-Wno-write-strings>
            $<$<COMPILE_LANGUAGE:CXX>:-fno-char8_t>
    )
endif ()

# Clang specific settings
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(
            $<$<COMPILE_LANGUAGE:CXX>:-fno-char8_t>
    )
endif ()

# Add subdirectories
add_subdirectory(deps)
add_subdirectory(src)

# Export and packaging
if (VX_INSTALL)
    # Export targets for build tree
    export(TARGETS ${VX_EXPORTED_TARGETS}
            FILE "${CMAKE_CURRENT_BINARY_DIR}/VxMathTargets.cmake"
            NAMESPACE VxMath::
    )

    # Install targets and export for install tree
    install(EXPORT VxMathTargets
            FILE VxMathTargets.cmake
            NAMESPACE VxMath::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/VxMath
    )

    # Configure package config file
    configure_package_config_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VxMathConfig.cmake.in"
            "${CMAKE_CURRENT_BINARY_DIR}/VxMathConfig.cmake"
            INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/VxMath
    )

    # Configure package version file
    write_basic_package_version_file(
            "${CMAKE_CURRENT_BINARY_DIR}/VxMathConfigVersion.cmake"
            VERSION ${PROJECT_VERSION}
            COMPATIBILITY SameMajorVersion
    )

    # Install config files
    install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/VxMathConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/VxMathConfigVersion.cmake"
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/VxMath
    )

    # Add package to CMake package registry
    export(PACKAGE VxMath)
endif ()