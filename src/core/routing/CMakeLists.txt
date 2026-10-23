# SPDX-License-Identifier: MIT

add_library(orpheus_routing STATIC
    audio_graph.cpp # ORP134 G3: graph-neutral routing seam
    routing_matrix.cpp
    routing_meter_state.cpp
    gain_smoother.cpp
    clip_routing.cpp
)

target_compile_features(orpheus_routing PUBLIC cxx_std_20)

# Mixing and smoothing participate in the cross-platform render-byte contract.
if(MSVC)
  if(MSVC_VERSION GREATER_EQUAL 1930)
    target_compile_options(orpheus_routing PRIVATE /fp:precise)
  else()
    target_compile_options(orpheus_routing PRIVATE /fp:strict)
  endif()
else()
  target_compile_options(orpheus_routing PRIVATE -ffp-contract=off)
endif()

target_include_directories(orpheus_routing
    PUBLIC
        $<BUILD_INTERFACE:${ORPHEUS_ROOT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${ORPHEUS_ROOT_SOURCE_DIR}/src/core
)

target_link_libraries(orpheus_routing
    PUBLIC
        orpheus_session  # For SessionGraphError
)

set_target_properties(orpheus_routing PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ORP127 T10: namespaced aliases for add_subdirectory (submodule) consumers.
add_library(Orpheus::routing ALIAS orpheus_routing)
add_library(Orpheus::orpheus_routing ALIAS orpheus_routing)

# Apply compiler warnings
include(${ORPHEUS_ROOT_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
orpheus_enable_warnings(orpheus_routing)

# Install public headers are handled by root CMakeLists.txt
