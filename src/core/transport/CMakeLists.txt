# SPDX-License-Identifier: MIT

add_library(orpheus_transport STATIC
    clip_source.cpp # ORP134 G1: prepared/streaming realtime clip sources
    clip_dsp.cpp
    transport_controller.cpp
)

target_compile_features(orpheus_transport PUBLIC cxx_std_20)

target_include_directories(orpheus_transport
    PUBLIC
        $<BUILD_INTERFACE:${ORPHEUS_ROOT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${ORPHEUS_ROOT_SOURCE_DIR}/src/core
)

target_link_libraries(orpheus_transport
    PUBLIC
        orpheus_session  # For SessionGraph
        orpheus_audio_io # For IAudioFileReader
        orpheus_routing  # For IRoutingMatrix
)

set_target_properties(orpheus_transport PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ORP127 T10: namespaced aliases so add_subdirectory (submodule) consumers can
# link Orpheus::transport, matching the names the install/export path exposes to
# find_package consumers. Without these, FourTrack's submodule build could not
# reference the transport target.
add_library(Orpheus::transport ALIAS orpheus_transport)
add_library(Orpheus::orpheus_transport ALIAS orpheus_transport)

# Apply compiler warnings
include(${ORPHEUS_ROOT_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
orpheus_enable_warnings(orpheus_transport)

# Install public headers are handled by root CMakeLists.txt
