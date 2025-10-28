# SPDX-License-Identifier: MIT
# tools/cli/CMakeLists.txt
#
# Session Inspection CLI Tool

cmake_minimum_required(VERSION 3.20)

add_executable(inspect_session inspect_session.cpp)
target_compile_features(inspect_session PRIVATE cxx_std_20)

# Compiler warnings
if(MSVC)
  target_compile_options(inspect_session PRIVATE /W4)
else()
  target_compile_options(inspect_session PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install to bin directory
install(TARGETS inspect_session DESTINATION bin)
