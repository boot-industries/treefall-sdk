# SPDX-License-Identifier: MIT
#
# ORP127 T10: submodule / co-development consumption smoke test.
#
# FourTrack consumes the SDK as a git submodule via add_subdirectory (rather
# than find_package on an installed tree). The failure modes differ from the
# find_package path — here we exercise add_subdirectory() target visibility and
# alias availability, not install() rules or exported config. This guards both
# consumption paths so a change that breaks one but not the other is caught.

cmake_minimum_required(VERSION 3.22)

project(orpheus_add_subdirectory_smoke LANGUAGES CXX)

if(NOT DEFINED ORPHEUS_SDK_SOURCE_DIR)
  message(FATAL_ERROR "ORPHEUS_SDK_SOURCE_DIR must point at the SDK source tree")
endif()

# Consume the SDK the way a submodule host does: pull it in as a subdirectory.
# Turn off the SDK's own apps/tests to keep this smoke build lean and avoid
# recursively re-adding this very test.
set(ORP_WITH_TESTS OFF CACHE BOOL "" FORCE)
set(ORPHEUS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ORP_ENABLE_ASAN OFF CACHE BOOL "" FORCE)
set(ORP_ENABLE_UBSAN OFF CACHE BOOL "" FORCE)

add_subdirectory(${ORPHEUS_SDK_SOURCE_DIR} ${CMAKE_BINARY_DIR}/orpheus_sdk_build)

if(NOT DEFINED TREEFALL_DISCOVERY_MODE OR TREEFALL_DISCOVERY_MODE STREQUAL "legacy")
  set(consumer_namespaces Orpheus)
elseif(TREEFALL_DISCOVERY_MODE STREQUAL "treefall")
  set(consumer_namespaces Treefall)
elseif(TREEFALL_DISCOVERY_MODE STREQUAL "legacy-first")
  set(consumer_namespaces Orpheus Treefall)
elseif(TREEFALL_DISCOVERY_MODE STREQUAL "treefall-first")
  set(consumer_namespaces Treefall Orpheus)
else()
  message(FATAL_ERROR "Unknown source consumption mode: ${TREEFALL_DISCOVERY_MODE}")
endif()

# The five modules FourTrack links must all be reachable as aliases when the SDK
# is added as a subdirectory (ORP127 downstream-consumer requirement).
foreach(_tgt Orpheus::transport Orpheus::routing Orpheus::audio_utils Orpheus::diagnostics)
  if(NOT TARGET ${_tgt})
    message(FATAL_ERROR "Expected ${_tgt} to be available via add_subdirectory")
  endif()
endforeach()

add_executable(orpheus_add_subdirectory_smoke main.cpp)
foreach(consumer_namespace IN LISTS consumer_namespaces)
  target_link_libraries(orpheus_add_subdirectory_smoke PRIVATE
    ${consumer_namespace}::transport ${consumer_namespace}::routing
    ${consumer_namespace}::audio_utils ${consumer_namespace}::audio_io)
endforeach()
enable_testing()
add_test(NAME source_consumer_runtime COMMAND orpheus_add_subdirectory_smoke)
