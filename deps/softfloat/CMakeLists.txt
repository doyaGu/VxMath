cmake_minimum_required(VERSION 3.12)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(vx_softfloat LANGUAGES CXX)
endif()

include(TestBigEndian)
test_big_endian(VX_SOFTFLOAT_BIG_ENDIAN)
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("int main() { return __builtin_clzll(1ULL); }"
    VX_SOFTFLOAT_BUILTIN_CLZ)

configure_file(config.h.in generated/config.h)

# The vendored source set is immutable and recorded in UPSTREAM.json. Keep the
# generic integer implementation on every target; no native floating-point ABI
# or CPU emulator is part of this target.
file(GLOB arithmetic_sources CONFIGURE_DEPENDS
    cpu/softfloat3e/*.cc cpu/softfloat3e/8086-SSE/*.cc)
# These primitives are defined inline by primitives.h. Compiling their legacy
# out-of-line copies as well creates duplicate definitions in static archives.
foreach(primitive
    add128 sub128 eq128 le128 lt128 countLeadingZeros16 countLeadingZeros32
    mul128By32 mul64ByShifted32To128 shiftRightJam32 shiftRightJam64
    shiftRightJam64Extra shortShiftLeft128 shortShiftRight128
    shortShiftRightJam128 shortShiftRightJam128Extra shortShiftRightJam64
    shortShiftRightJam64Extra)
    list(REMOVE_ITEM arithmetic_sources
        "${CMAKE_CURRENT_SOURCE_DIR}/cpu/softfloat3e/s_${primitive}.cc")
endforeach()
if(VX_SOFTFLOAT_BUILTIN_CLZ)
    list(REMOVE_ITEM arithmetic_sources
        "${CMAKE_CURRENT_SOURCE_DIR}/cpu/softfloat3e/s_countLeadingZeros64.cc")
endif()
set(numerical_sources ${arithmetic_sources}
    cpu/fpu/f128_67.cc
    cpu/fpu/f2xm1.cc
    cpu/fpu/fpatan.cc
    cpu/fpu/fprem.cc
    cpu/fpu/fsincos.cc
    cpu/fpu/fyl2x.cc
)
add_library(vx_softfloat STATIC ${numerical_sources})
include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/VxFloat80.cmake")
vx_enable_float80_ipo(vx_softfloat)
target_compile_features(vx_softfloat PRIVATE cxx_std_11)
target_include_directories(vx_softfloat PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cpu>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cpu/softfloat3e/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cpu/fpu>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
)
target_compile_definitions(vx_softfloat PRIVATE
    SOFTFLOAT_FAST_INT64 SOFTFLOAT_FAST_DIV32TO16 SOFTFLOAT_FAST_DIV64TO32 INLINE_LEVEL=5)
set_target_properties(vx_softfloat PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)
if(MSVC)
    target_compile_options(vx_softfloat PRIVATE /W3 /wd4146 /wd4244)
endif()
