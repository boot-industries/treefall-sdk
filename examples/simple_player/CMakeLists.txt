# SPDX-License-Identifier: MIT
# examples/simple_player/CMakeLists.txt
#
# Simple Clip Player Example

cmake_minimum_required(VERSION 3.20)

add_executable(simple_player simple_player.cpp)
target_compile_features(simple_player PRIVATE cxx_std_20)

# Link against Orpheus SDK
target_link_libraries(simple_player PRIVATE
  orpheus_transport
  orpheus_audio_io
)

# Platform-specific audio drivers
if(APPLE)
  target_link_libraries(simple_player PRIVATE orpheus_audio_driver_coreaudio)
elseif(WIN32)
  if(TARGET orpheus_audio_driver_wasapi)
    target_link_libraries(simple_player PRIVATE orpheus_audio_driver_wasapi)
  endif()
endif()

# Compiler warnings
if(MSVC)
  target_compile_options(simple_player PRIVATE /W4)
else()
  target_compile_options(simple_player PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install to bin directory
install(TARGETS simple_player DESTINATION bin)
