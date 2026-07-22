# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.22)

project(orpheus_find_package_smoke LANGUAGES CXX)

if(NOT DEFINED ORPHEUS_REQUIRED_VERSION)
  message(FATAL_ERROR "ORPHEUS_REQUIRED_VERSION is required")
endif()

find_package(OrpheusSDK ${ORPHEUS_REQUIRED_VERSION} EXACT REQUIRED CONFIG)

foreach(required_target
    Orpheus::core
    Orpheus::diagnostics
    Orpheus::audio_utils
    Orpheus::audio_io
    Orpheus::audio_driver_manager
    Orpheus::routing
    Orpheus::transport)
  if(NOT TARGET ${required_target})
    message(FATAL_ERROR "Installed target manifest is missing ${required_target}")
  endif()
endforeach()

if(NOT OrpheusSDK_VERSION VERSION_EQUAL ORPHEUS_REQUIRED_VERSION)
  message(FATAL_ERROR
    "Package reports ${OrpheusSDK_VERSION}; expected ${ORPHEUS_REQUIRED_VERSION}")
endif()

enable_testing()

function(add_orpheus_fixture target source)
  add_executable(${target} ${source})
  target_compile_features(${target} PRIVATE cxx_std_20)
  add_test(NAME ${target} COMMAND ${target})
endfunction()

add_orpheus_fixture(orpheus_find_package_core main.cpp)
target_link_libraries(orpheus_find_package_core
  PRIVATE Orpheus::core Orpheus::audio_utils)

add_orpheus_fixture(orpheus_find_package_diagnostics
  ${CMAKE_CURRENT_LIST_DIR}/../../verify_diagnostics_standalone.cpp)
target_link_libraries(orpheus_find_package_diagnostics PRIVATE Orpheus::diagnostics)

add_orpheus_fixture(orpheus_find_package_analysis analysis.cpp)
target_link_libraries(orpheus_find_package_analysis PRIVATE Orpheus::audio_utils)

add_orpheus_fixture(orpheus_find_package_trigger_voice trigger_voice.cpp)
target_link_libraries(orpheus_find_package_trigger_voice PRIVATE Orpheus::audio_utils)
add_orpheus_fixture(orpheus_find_package_tape_varispeed tape_varispeed.cpp)
target_link_libraries(orpheus_find_package_tape_varispeed PRIVATE Orpheus::audio_utils)
add_orpheus_fixture(orpheus_find_package_clip_dsp clip_dsp.cpp)
target_link_libraries(orpheus_find_package_clip_dsp PRIVATE Orpheus::transport)


add_orpheus_fixture(orpheus_find_package_media_integrity media_integrity.cpp)
target_link_libraries(orpheus_find_package_media_integrity PRIVATE Orpheus::audio_utils)

add_orpheus_fixture(orpheus_find_package_writer_input writer_input.cpp)
target_link_libraries(orpheus_find_package_writer_input PRIVATE Orpheus::audio_utils)

add_orpheus_fixture(orpheus_find_package_transport_routing transport_routing.cpp)
target_link_libraries(orpheus_find_package_transport_routing
  PRIVATE Orpheus::transport Orpheus::routing Orpheus::audio_driver_manager)

add_orpheus_fixture(orpheus_find_package_workflow_contracts workflow_contracts.cpp)
target_link_libraries(orpheus_find_package_workflow_contracts
  PRIVATE Orpheus::transport)

add_orpheus_fixture(orpheus_find_package_legacy_transport
  legacy_transport_implementation.cpp)
target_link_libraries(orpheus_find_package_legacy_transport
  PRIVATE Orpheus::transport)

add_orpheus_fixture(orpheus_find_package_legacy_transport_defaults
  legacy_transport_defaults.cpp)
target_link_libraries(orpheus_find_package_legacy_transport_defaults
  PRIVATE Orpheus::transport)

if(APPLE AND NOT TARGET Orpheus::audio_driver_coreaudio)
  message(FATAL_ERROR "macOS package is missing Orpheus::audio_driver_coreaudio")
endif()

if(APPLE)
  add_compile_options(-stdlib=libc++)
  add_link_options(-stdlib=libc++)
endif()
