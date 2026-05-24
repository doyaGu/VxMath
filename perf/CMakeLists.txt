set(VXMATH_PERF_LINK_TARGET VxMath)
if (NOT TARGET VxMath AND TARGET VxMathStatic)
    set(VXMATH_PERF_LINK_TARGET VxMathStatic)
endif ()

if (NOT TARGET ${VXMATH_PERF_LINK_TARGET})
    message(FATAL_ERROR "VxMathPerf requires VxMath or VxMathStatic target")
endif ()

add_executable(VxMathPerf
        VxMathPerf.cpp
)

set_target_properties(VxMathPerf PROPERTIES
        FOLDER "Tools"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_include_directories(VxMathPerf PRIVATE
        ${NEMO_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(VxMathPerf PRIVATE
        ${VXMATH_PERF_LINK_TARGET}
)

if (VXMATH_ENABLE_PERF_GATES)
    enable_testing()
    add_test(NAME VxMathImagePerfGate
            COMMAND VxMathPerf --gate-image-auto --gate-runs=3
    )
    set_tests_properties(VxMathImagePerfGate PROPERTIES
            ENVIRONMENT "VXMATH_PERF_WARMUP=5;VXMATH_PERF_MEASURE=30"
            LABELS "perf"
    )
endif ()
