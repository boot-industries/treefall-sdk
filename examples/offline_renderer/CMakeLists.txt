# SPDX-License-Identifier: MIT
# examples/offline_renderer/CMakeLists.txt
#
# Offline Renderer Example

cmake_minimum_required(VERSION 3.20)

add_executable(offline_renderer offline_renderer.cpp)
target_compile_features(offline_renderer PRIVATE cxx_std_20)

# Link against Orpheus SDK
target_link_libraries(offline_renderer PRIVATE
  orpheus_transport
  orpheus_audio_io
  orpheus_render
)

# Compiler warnings
if(MSVC)
  target_compile_options(offline_renderer PRIVATE /W4)
else()
  target_compile_options(offline_renderer PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install to bin directory
install(TARGETS offline_renderer DESTINATION bin)
