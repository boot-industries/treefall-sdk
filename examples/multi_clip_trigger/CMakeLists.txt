# SPDX-License-Identifier: MIT
# examples/multi_clip_trigger/CMakeLists.txt
#
# Multi-Clip Trigger Example

cmake_minimum_required(VERSION 3.20)

add_executable(multi_clip_trigger multi_clip_trigger.cpp)
target_compile_features(multi_clip_trigger PRIVATE cxx_std_20)

# Link against Orpheus SDK
target_link_libraries(multi_clip_trigger PRIVATE
  orpheus_transport
  orpheus_audio_io
)

# Platform-specific audio drivers
if(APPLE)
  target_link_libraries(multi_clip_trigger PRIVATE orpheus_audio_driver_coreaudio)
elseif(WIN32)
  if(TARGET orpheus_audio_driver_wasapi)
    target_link_libraries(multi_clip_trigger PRIVATE orpheus_audio_driver_wasapi)
  endif()
endif()

# Compiler warnings
if(MSVC)
  target_compile_options(multi_clip_trigger PRIVATE /W4)
else()
  target_compile_options(multi_clip_trigger PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install to bin directory
install(TARGETS multi_clip_trigger DESTINATION bin)
