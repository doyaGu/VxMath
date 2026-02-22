include(GNUInstallDirs)

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

# SIMDE
add_subdirectory(simde EXCLUDE_FROM_ALL)

# Export dep targets to parent scope so the top-level CMakeLists.txt can include them in the export set
set(VXMATH_DEP_EXPORTED_TARGETS stb simde PARENT_SCOPE)

# Installation
if (VXMATH_INSTALL)
    install(TARGETS stb simde
            EXPORT VxMathTargets
    )
    file(GLOB _stb_headers "${CMAKE_CURRENT_SOURCE_DIR}/stb/*.h")
    install(FILES ${_stb_headers}
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/stb"
    )
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/simde/simde"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )
endif ()