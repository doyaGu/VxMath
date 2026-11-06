cmake_minimum_required(VERSION 3.21)
project(VxMathNumericalTests LANGUAGES CXX)
enable_testing()

get_filename_component(vx_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
add_subdirectory("${vx_root}/deps/softfloat" softfloat)

set(vx_numerical_sources
    "${vx_root}/src/VxFloat80.cpp"
    "${vx_root}/src/VxFloat80Math.cpp"
    "${vx_root}/src/VxFPUControlWord.cpp")
if (MSVC AND CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
    enable_language(ASM_MASM)
    list(APPEND vx_numerical_sources "${vx_root}/src/VxFPUControlWordWin64.asm")
endif ()

function(vx_add_numerical_library target)
    add_library(${target} STATIC ${vx_numerical_sources})
    target_include_directories(${target} PUBLIC "${vx_root}/include")
    target_compile_features(${target} PUBLIC cxx_std_17)
    target_compile_definitions(${target} PUBLIC VX_LIB VXMATH_ENABLE_X87_EMULATION=1)
    target_link_libraries(${target} PRIVATE vx_softfloat)
    vx_enable_float80_ipo(${target})
    if (MSVC)
        target_compile_options(${target} PUBLIC /fp:precise)
    else ()
        target_compile_options(${target} PUBLIC -fno-strict-aliasing -ffp-contract=off)
    endif ()
endfunction()

vx_add_numerical_library(VxMathNumerics)
vx_add_numerical_library(VxMathNumericsScalar)
target_compile_definitions(VxMathNumericsScalar PUBLIC VX_SIMD_NONE=1)

include(FetchContent)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0)
FetchContent_MakeAvailable(googletest)

foreach(variant "" Scalar)
    add_executable(Float80Arithmetic${variant}Test Float80ArithmeticTest.cpp)
    target_link_libraries(Float80Arithmetic${variant}Test PRIVATE VxMathNumerics${variant} gtest_main)
    vx_enable_float80_ipo(Float80Arithmetic${variant}Test)
    add_test(NAME Float80Arithmetic${variant}Test COMMAND Float80Arithmetic${variant}Test)

    add_executable(Float80${variant}Benchmark Float80Benchmark.cpp)
    target_link_libraries(Float80${variant}Benchmark PRIVATE VxMathNumerics${variant})
    vx_enable_float80_ipo(Float80${variant}Benchmark)
endforeach()
