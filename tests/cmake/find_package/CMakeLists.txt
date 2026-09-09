# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.22)

project(orpheus_find_package_smoke LANGUAGES C CXX)

if(NOT DEFINED ORPHEUS_REQUIRED_VERSION)
  message(FATAL_ERROR "ORPHEUS_REQUIRED_VERSION is required")
endif()

if(NOT DEFINED TREEFALL_DISCOVERY_MODE)
  set(TREEFALL_DISCOVERY_MODE legacy)
endif()
if(TREEFALL_DISCOVERY_MODE STREQUAL "treefall")
  find_package(TreefallSDK ${ORPHEUS_REQUIRED_VERSION} EXACT REQUIRED CONFIG)
elseif(TREEFALL_DISCOVERY_MODE STREQUAL "legacy-first")
  find_package(OrpheusSDK ${ORPHEUS_REQUIRED_VERSION} EXACT REQUIRED CONFIG)
  find_package(TreefallSDK ${ORPHEUS_REQUIRED_VERSION} EXACT REQUIRED CONFIG)
elseif(TREEFALL_DISCOVERY_MODE STREQUAL "treefall-first")
  find_package(TreefallSDK ${ORPHEUS_REQUIRED_VERSION} EXACT REQUIRED CONFIG)
  find_package(OrpheusSDK ${ORPHEUS_REQUIRED_VERSION} EXACT REQUIRED CONFIG)
elseif(TREEFALL_DISCOVERY_MODE STREQUAL "legacy")
  find_package(OrpheusSDK ${ORPHEUS_REQUIRED_VERSION} EXACT REQUIRED CONFIG)
else()
  message(FATAL_ERROR "Unknown discovery mode: ${TREEFALL_DISCOVERY_MODE}")
endif()

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
  if(source MATCHES "\\.c$")
    target_compile_features(${target} PRIVATE c_std_11)
  else()
    target_compile_features(${target} PRIVATE cxx_std_20)
  endif()
  if(WIN32)
    foreach(runtime_target
        Orpheus::orpheus_session
        Orpheus::orpheus_clipgrid
        Orpheus::orpheus_render)
      if(TARGET ${runtime_target})
        add_custom_command(TARGET ${target} POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
                  $<TARGET_RUNTIME_DLLS:${target}>
                  $<TARGET_FILE:${runtime_target}>
                  $<TARGET_FILE_DIR:${target}>
          COMMAND_EXPAND_LISTS)
      endif()
    endforeach()
  endif()
  add_test(NAME ${target} COMMAND ${target})
endfunction()

add_orpheus_fixture(orpheus_find_package_core main.cpp)
target_link_libraries(orpheus_find_package_core PRIVATE Orpheus::core)

add_orpheus_fixture(orpheus_find_package_audio_io_telemetry audio_io_telemetry.cpp)
target_link_libraries(orpheus_find_package_audio_io_telemetry PRIVATE Orpheus::audio_io)

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

add_orpheus_fixture(orpheus_find_package_offline_render offline_render.cpp)
target_link_libraries(orpheus_find_package_offline_render
  PRIVATE Orpheus::transport Orpheus::audio_utils)

add_orpheus_fixture(treefall_find_package_compat treefall_compat.cpp)
target_link_libraries(treefall_find_package_compat PRIVATE Treefall::transport Treefall::audio_utils)
add_orpheus_fixture(treefall_find_package_compat_legacy_first treefall_compat.cpp)
target_compile_definitions(treefall_find_package_compat_legacy_first PRIVATE TREEFALL_LEGACY_HEADER_FIRST=1)
target_link_libraries(treefall_find_package_compat_legacy_first
  PRIVATE Orpheus::transport Treefall::audio_utils)
add_orpheus_fixture(treefall_find_package_abi treefall_abi.c)
target_link_libraries(treefall_find_package_abi PRIVATE Treefall::core)
add_orpheus_fixture(treefall_find_package_audio_utils treefall_audio_utils.cpp)
target_link_libraries(treefall_find_package_audio_utils PRIVATE Treefall::audio_utils)
add_orpheus_fixture(treefall_find_package_diagnostics treefall_diagnostics.cpp)
target_link_libraries(treefall_find_package_diagnostics PRIVATE Treefall::diagnostics)

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
