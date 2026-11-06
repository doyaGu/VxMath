include_guard(GLOBAL)
include(CheckIPOSupported)
check_ipo_supported(RESULT _vx_float80_ipo_supported LANGUAGES CXX)
set(VXMATH_FLOAT80_IPO_SUPPORTED "${_vx_float80_ipo_supported}" CACHE INTERNAL
    "Compiler support for numerical interprocedural optimization")

function(vx_enable_float80_ipo target)
    if (VXMATH_FLOAT80_IPO_SUPPORTED)
        set_target_properties(${target} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    endif ()
endfunction()
