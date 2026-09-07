# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.22)

project(orpheus_package_runtime_consumer LANGUAGES CXX)

if(NOT DEFINED ORPHEUS_REQUIRED_VERSION OR ORPHEUS_REQUIRED_VERSION STREQUAL "")
  message(FATAL_ERROR "ORPHEUS_REQUIRED_VERSION must name the installed SDK version under test")
endif()

if(NOT DEFINED TREEFALL_DISCOVERY_MODE OR TREEFALL_DISCOVERY_MODE STREQUAL "" OR
   TREEFALL_DISCOVERY_MODE STREQUAL "legacy")
  find_package(OrpheusSDK ${ORPHEUS_REQUIRED_VERSION} CONFIG REQUIRED)
elseif(TREEFALL_DISCOVERY_MODE STREQUAL "treefall")
  find_package(TreefallSDK ${ORPHEUS_REQUIRED_VERSION} CONFIG REQUIRED)
elseif(TREEFALL_DISCOVERY_MODE STREQUAL "legacy-first")
  find_package(OrpheusSDK ${ORPHEUS_REQUIRED_VERSION} CONFIG REQUIRED)
  find_package(TreefallSDK ${ORPHEUS_REQUIRED_VERSION} CONFIG REQUIRED)
elseif(TREEFALL_DISCOVERY_MODE STREQUAL "treefall-first")
  find_package(TreefallSDK ${ORPHEUS_REQUIRED_VERSION} CONFIG REQUIRED)
  find_package(OrpheusSDK ${ORPHEUS_REQUIRED_VERSION} CONFIG REQUIRED)
else()
  message(FATAL_ERROR "Unknown TREEFALL_DISCOVERY_MODE=${TREEFALL_DISCOVERY_MODE}")
endif()

set(PACKAGE_TARGET_PREFIX Orpheus)
if(TREEFALL_DISCOVERY_MODE STREQUAL "treefall" OR
   TREEFALL_DISCOVERY_MODE STREQUAL "treefall-first")
  set(PACKAGE_TARGET_PREFIX Treefall)
endif()

add_executable(orpheus_package_runtime_consumer main.cpp)
target_compile_features(orpheus_package_runtime_consumer PRIVATE cxx_std_20)
target_link_libraries(orpheus_package_runtime_consumer PRIVATE
  ${PACKAGE_TARGET_PREFIX}::transport
  ${PACKAGE_TARGET_PREFIX}::audio_io
  ${PACKAGE_TARGET_PREFIX}::audio_driver_manager)

if(WIN32)
  foreach(runtime_target
      Orpheus::orpheus_session
      Orpheus::orpheus_clipgrid
      Orpheus::orpheus_render)
    if(TARGET ${runtime_target})
      add_custom_command(TARGET orpheus_package_runtime_consumer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:orpheus_package_runtime_consumer>
                $<TARGET_FILE:${runtime_target}>
                $<TARGET_FILE_DIR:orpheus_package_runtime_consumer>
        COMMAND_EXPAND_LISTS)
    endif()
  endforeach()
endif()

enable_testing()
add_test(NAME orpheus_package_runtime_consumer COMMAND orpheus_package_runtime_consumer)
