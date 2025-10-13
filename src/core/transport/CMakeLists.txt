# SPDX-License-Identifier: MIT

add_library(orpheus_transport STATIC
    transport_controller.cpp
)

target_compile_features(orpheus_transport PUBLIC cxx_std_20)

target_include_directories(orpheus_transport
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/core
)

target_link_libraries(orpheus_transport
    PUBLIC
        orpheus_session  # For SessionGraph
        orpheus_audio_io # For IAudioFileReader
        orpheus_routing  # For IRoutingMatrix
)

set_target_properties(orpheus_transport PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Apply compiler warnings
include(${CMAKE_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
orpheus_enable_warnings(orpheus_transport)

# Install public headers are handled by root CMakeLists.txt
