# SPDX-License-Identifier: MIT
# examples/simple_player/CMakeLists.txt
#
# Simple Clip Player Example

cmake_minimum_required(VERSION 3.20)

add_executable(simple_player simple_player.cpp)
target_compile_features(simple_player PRIVATE cxx_std_20)

# Link against the Treefall compatibility targets. These aliases resolve to
# the canonical Orpheus targets in both source and installed-package builds.
target_link_libraries(simple_player PRIVATE
  Treefall::transport
  Treefall::audio_io
)

# Platform-specific audio drivers
if(TARGET Treefall::audio_driver_coreaudio)
  target_link_libraries(simple_player PRIVATE Treefall::audio_driver_coreaudio)
  target_compile_definitions(simple_player PRIVATE ORPHEUS_ENABLE_COREAUDIO)
elseif(WIN32 AND TARGET Treefall::audio_driver_wasapi)
  target_link_libraries(simple_player PRIVATE Treefall::audio_driver_wasapi)
endif()

# Compiler warnings
if(MSVC)
  target_compile_options(simple_player PRIVATE /W4)
else()
  target_compile_options(simple_player PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install to bin directory
install(TARGETS simple_player DESTINATION bin)
