# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.22)

project(orpheus_find_package_smoke LANGUAGES C CXX)

if(NOT DEFINED ORPHEUS_REQUIRED_VERSION)
  message(FATAL_ERROR "ORPHEUS_REQUIRED_VERSION is required")
endif()

# Optional installed ShmUI exports refer to application-provided JUCE modules,
# exactly as in the existing dedicated package consumer. Only SDK discovery is
# prefix-only; never substitute SDK build-tree targets for these dependencies.
if(EXPECTED_SHMUI OR EXPECTED_SHMUI_GL)
  include(FetchContent)
  set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    JUCE
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.4.tar.gz
    URL_HASH SHA256=0effe9823a6fd8a4f98e5c08be7d308350e417b2d15dcf34011e778fefbbf005
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE)
  FetchContent_MakeAvailable(JUCE)
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

include("${CMAKE_CURRENT_LIST_DIR}/installed_manifest.cmake")

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
        Orpheus::orpheus_abi_runtime
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

add_orpheus_fixture(orpheus_find_package_session_io session_io.cpp)
target_link_libraries(orpheus_find_package_session_io PRIVATE Orpheus::core)
add_orpheus_fixture(treefall_find_package_session_io session_io.cpp)
target_compile_definitions(treefall_find_package_session_io PRIVATE TREEFALL_SESSION_HEADER_FIRST=1)
target_link_libraries(treefall_find_package_session_io PRIVATE Treefall::core)

if(EXPECTED_REALTIME)
add_orpheus_fixture(orpheus_find_package_audio_io_telemetry audio_io_telemetry.cpp)
target_link_libraries(orpheus_find_package_audio_io_telemetry PRIVATE Orpheus::audio_io)
endif()

add_orpheus_fixture(orpheus_find_package_diagnostics
  ${CMAKE_CURRENT_LIST_DIR}/../../verify_diagnostics_standalone.cpp)
target_link_libraries(orpheus_find_package_diagnostics PRIVATE Orpheus::diagnostics)

if(EXPECTED_REALTIME)
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
endif()

if(EXPECTED_REALTIME)
add_orpheus_fixture(treefall_find_package_compat treefall_compat.cpp)
target_link_libraries(treefall_find_package_compat PRIVATE Treefall::transport Treefall::audio_utils)
add_orpheus_fixture(treefall_find_package_compat_legacy_first treefall_compat.cpp)
target_compile_definitions(treefall_find_package_compat_legacy_first PRIVATE TREEFALL_LEGACY_HEADER_FIRST=1)
target_link_libraries(treefall_find_package_compat_legacy_first
  PRIVATE Orpheus::transport Treefall::audio_utils)
endif()
add_orpheus_fixture(treefall_find_package_abi treefall_abi.c)
target_link_libraries(treefall_find_package_abi PRIVATE Treefall::core)
if(EXPECTED_REALTIME)
add_orpheus_fixture(treefall_find_package_audio_utils treefall_audio_utils.cpp)
target_link_libraries(treefall_find_package_audio_utils PRIVATE Treefall::audio_utils)
endif()
add_orpheus_fixture(treefall_find_package_diagnostics treefall_diagnostics.cpp)
target_link_libraries(treefall_find_package_diagnostics PRIVATE Treefall::diagnostics)

if(EXPECTED_REALTIME)
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
endif()


if(APPLE)
  add_compile_options(-stdlib=libc++)
  add_link_options(-stdlib=libc++)
endif()
