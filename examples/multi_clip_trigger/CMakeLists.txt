# SPDX-License-Identifier: MIT
# examples/multi_clip_trigger/CMakeLists.txt
#
# Multi-Clip Trigger Example

cmake_minimum_required(VERSION 3.20)

add_executable(multi_clip_trigger multi_clip_trigger.cpp)
target_compile_features(multi_clip_trigger PRIVATE cxx_std_20)

# Link against the Treefall compatibility targets.
target_link_libraries(multi_clip_trigger PRIVATE
  Treefall::transport
  Treefall::audio_io
)

# Platform-specific audio drivers
if(TARGET Treefall::audio_driver_coreaudio)
  target_link_libraries(multi_clip_trigger PRIVATE Treefall::audio_driver_coreaudio)
  target_compile_definitions(multi_clip_trigger PRIVATE ORPHEUS_ENABLE_COREAUDIO)
elseif(WIN32 AND TARGET Treefall::audio_driver_wasapi)
  target_link_libraries(multi_clip_trigger PRIVATE Treefall::audio_driver_wasapi)
endif()


# Compiler warnings
if(MSVC)
  target_compile_options(multi_clip_trigger PRIVATE /W4)
else()
  target_compile_options(multi_clip_trigger PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install to bin directory
install(TARGETS multi_clip_trigger DESTINATION bin)
