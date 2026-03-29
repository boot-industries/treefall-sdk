# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.22)

project(orpheus_find_package_smoke LANGUAGES CXX)

find_package(OrpheusSDK REQUIRED CONFIG)

add_executable(orpheus_find_package_smoke main.cpp)

target_link_libraries(orpheus_find_package_smoke PRIVATE Orpheus::core)

if(NOT TARGET Orpheus::diagnostics)
  message(FATAL_ERROR "Expected Orpheus::diagnostics to be exported by OrpheusSDK")
endif()

add_executable(orpheus_find_package_diagnostics
  ${CMAKE_CURRENT_LIST_DIR}/../../verify_diagnostics_standalone.cpp)
target_link_libraries(orpheus_find_package_diagnostics PRIVATE Orpheus::diagnostics)

if(NOT TARGET Orpheus::audio_utils)
  message(FATAL_ERROR "Expected Orpheus::audio_utils to be exported by OrpheusSDK")
endif()

add_executable(orpheus_find_package_audio_utils
  ${CMAKE_CURRENT_LIST_DIR}/../../verify_audio_utils_standalone.cpp)
target_link_libraries(orpheus_find_package_audio_utils PRIVATE Orpheus::audio_utils)

# When the SDK is consumed via find_package, CompilerWarnings.cmake is expected
# to live next to the generated package config inside a "cmake" directory.
# Fall back to the in-tree helper when running the smoke test from the source
# tree so we reuse the exact same warning policy.
set(_orpheus_warning_module "${CMAKE_CURRENT_LIST_DIR}/cmake/CompilerWarnings.cmake")
if(NOT EXISTS "${_orpheus_warning_module}")
  set(_orpheus_warning_module
      "${CMAKE_CURRENT_LIST_DIR}/../../cmake/CompilerWarnings.cmake")
endif()

include(${_orpheus_warning_module})
orpheus_enable_warnings(orpheus_find_package_smoke)
orpheus_enable_warnings(orpheus_find_package_diagnostics)
orpheus_enable_warnings(orpheus_find_package_audio_utils)

if(APPLE)
  add_compile_options(-stdlib=libc++)
  add_link_options(-stdlib=libc++)
endif()
