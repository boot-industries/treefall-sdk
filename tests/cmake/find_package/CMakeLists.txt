# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.22)

project(orpheus_find_package_smoke LANGUAGES CXX)

find_package(OrpheusSDK REQUIRED CONFIG)

add_executable(orpheus_find_package_smoke main.cpp)

target_link_libraries(orpheus_find_package_smoke PRIVATE Orpheus::core)

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

if(APPLE)
  add_compile_options(-stdlib=libc++)
  add_link_options(-stdlib=libc++)
endif()
