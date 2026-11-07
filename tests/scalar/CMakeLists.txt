cmake_minimum_required(VERSION 3.21)
project(VxMathScalarProviderTests LANGUAGES CXX)
enable_testing()

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/VxScalar.cmake")
vx_add_scalar_arithmetic(ScalarProvider)

add_executable(ScalarProviderTest ScalarProviderTest.cpp)
target_link_libraries(ScalarProviderTest PRIVATE ScalarProvider)
target_compile_definitions(ScalarProviderTest PRIVATE VX_LIB)
target_compile_features(ScalarProviderTest PRIVATE cxx_std_11)
add_test(NAME ScalarProviderTest COMMAND ScalarProviderTest)
