# SPDX-License-Identifier: MIT

add_library(orpheus_routing STATIC
    routing_matrix.cpp
    gain_smoother.cpp
)

target_compile_features(orpheus_routing PUBLIC cxx_std_20)

target_include_directories(orpheus_routing
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/core
)

target_link_libraries(orpheus_routing
    PUBLIC
        orpheus_session  # For SessionGraphError
)

set_target_properties(orpheus_routing PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Apply compiler warnings
include(${CMAKE_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
orpheus_enable_warnings(orpheus_routing)

# Install public headers are handled by root CMakeLists.txt
