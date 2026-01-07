cmake_minimum_required(VERSION 3.12)

project(VxMath
        VERSION 0.1.0
        LANGUAGES CXX
)

# Determine if VxMath is being used as a subproject
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.21")
    # Use the built-in variable for CMake 3.21+
    if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
        set(VXMATH_IS_TOP_LEVEL ON)
    else ()
        set(VXMATH_IS_TOP_LEVEL OFF)
    endif ()
else ()
    # Fallback for older CMake versions
    if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
        set(VXMATH_IS_TOP_LEVEL ON)
    else ()
        set(VXMATH_IS_TOP_LEVEL OFF)
    endif ()
endif ()

# Only enforce out-of-source builds when VxMath is the top-level project
if (VXMATH_IS_TOP_LEVEL AND PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there.")
endif ()

# Set C++ standard only if not already set by parent project
if (NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif ()
if (NOT DEFINED CMAKE_CXX_STANDARD_REQUIRED)
    set(CMAKE_CXX_STANDARD_REQUIRED YES)
endif ()
if (NOT DEFINED CMAKE_CXX_EXTENSIONS)
    set(CMAKE_CXX_EXTENSIONS NO)
endif ()

# Project options - defaults depend on whether this is a top-level project
option(VXMATH_BUILD_TESTS "Build the test suite" ${VXMATH_IS_TOP_LEVEL})
option(VXMATH_BUILD_SHARED "Build shared library" ON)
option(VXMATH_BUILD_STATIC "Build static library" ON)
option(VXMATH_INSTALL "Generate install target" ${VXMATH_IS_TOP_LEVEL})

# Add path for custom modules
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Standard CMake modules
include(GenerateExportHeader)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Use folders to organize targets in an IDE (only when top-level)
if (VXMATH_IS_TOP_LEVEL)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMakeTargets")
endif ()

# Platform checks
if (NOT WIN32)
    message(FATAL_ERROR "Currently only Windows is supported.")
endif ()

# Build type handling (only when top-level)
if (VXMATH_IS_TOP_LEVEL)
    get_property(IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if (NOT IS_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
        message(STATUS "Setting build type to 'Release' as no build type was specified")
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the build type" FORCE)
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
    endif ()
endif ()

# Install prefix (only when top-level)
if (VXMATH_IS_TOP_LEVEL AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/install" CACHE PATH "Installation directory" FORCE)
    message(STATUS "Setting install directory to ${CMAKE_INSTALL_PREFIX}")
endif ()

# Generate compile commands for IDEs/tools (only when top-level)
if (VXMATH_IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif ()

# Validate options
if (NOT VXMATH_BUILD_SHARED AND NOT VXMATH_BUILD_STATIC)
    message(FATAL_ERROR "At least one of VXMATH_BUILD_SHARED or VXMATH_BUILD_STATIC must be enabled")
endif ()

# Cache internal variables
set(VXMATH_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include" CACHE INTERNAL "")
set(VXMATH_SOURCE_DIR "${PROJECT_SOURCE_DIR}/src" CACHE INTERNAL "")

# Backward compatibility aliases for old variable names
set(VX_INCLUDE_DIR "${VXMATH_INCLUDE_DIR}" CACHE INTERNAL "")
set(VX_SOURCE_DIR "${VXMATH_SOURCE_DIR}" CACHE INTERNAL "")

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
if (VXMATH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

# Export and packaging
if (VXMATH_INSTALL)
    # Export targets for build tree
    export(TARGETS ${VXMATH_EXPORTED_TARGETS}
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

    # Add package to CMake package registry (only when top-level)
    if (VXMATH_IS_TOP_LEVEL)
        export(PACKAGE VxMath)
    endif ()
endif ()