# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.22)

project(orpheus VERSION 0.9.1 LANGUAGES CXX)

set(ORPHEUS_ABI_MAJOR 1)
set(ORPHEUS_ABI_MINOR 0)
set(ORPHEUS_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/include")
file(MAKE_DIRECTORY "${ORPHEUS_GENERATED_INCLUDE_DIR}/orpheus")
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/orpheus_version.h.in"
  "${ORPHEUS_GENERATED_INCLUDE_DIR}/orpheus/version.h"
  @ONLY)

# Generate only SDK-owned forwarding headers; imported ShmUI headers are not mirrored.
set(TREEFALL_GENERATED_INCLUDE_DIR "${ORPHEUS_GENERATED_INCLUDE_DIR}/treefall")
file(REMOVE_RECURSE "${TREEFALL_GENERATED_INCLUDE_DIR}")
file(MAKE_DIRECTORY "${TREEFALL_GENERATED_INCLUDE_DIR}")
file(GLOB_RECURSE TREEFALL_CANONICAL_HEADERS CONFIGURE_DEPENDS
  RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/include/orpheus"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/orpheus/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/orpheus/*.hpp")
list(SORT TREEFALL_CANONICAL_HEADERS)
foreach(TREEFALL_FORWARD_HEADER IN LISTS TREEFALL_CANONICAL_HEADERS)
  if(TREEFALL_FORWARD_HEADER MATCHES "^(abi|errors|abi_version|export|namespace|version)\\.h$")
    continue()
  endif()
  get_filename_component(_treefall_header_directory "${TREEFALL_FORWARD_HEADER}" DIRECTORY)
  file(MAKE_DIRECTORY "${TREEFALL_GENERATED_INCLUDE_DIR}/${_treefall_header_directory}")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/treefall_forward.h.in"
    "${TREEFALL_GENERATED_INCLUDE_DIR}/${TREEFALL_FORWARD_HEADER}" @ONLY)
endforeach()
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/treefall_version.h.in"
  "${TREEFALL_GENERATED_INCLUDE_DIR}/version.h" @ONLY)

foreach(SDK_METADATA_BRAND orpheus treefall)
  if(SDK_METADATA_BRAND STREQUAL "orpheus")
    set(SDK_METADATA_PACKAGE OrpheusSDK)
    set(SDK_METADATA_NAMESPACE Orpheus)
    set(_sdk_target_manifest installed-targets.json)
  else()
    set(SDK_METADATA_PACKAGE TreefallSDK)
    set(SDK_METADATA_NAMESPACE Treefall)
    set(_sdk_target_manifest treefall-installed-targets.json)
  endif()
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/orpheus_release_metadata.json.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${SDK_METADATA_BRAND}-sdk.json" @ONLY)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/installed-targets.json.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${_sdk_target_manifest}" @ONLY)
endforeach()

set(ORPHEUS_ROOT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

option(ORP_WITH_TESTS "Build Orpheus tests" ON)
option(ORPHEUS_ENABLE_EXTENDED_TESTS
  "Build package, hardware, and stress tests in addition to the fast suite" ON)
option(ORPHEUS_ENABLE_ADAPTERS "Build Orpheus host adapters" ON)
option(ORPHEUS_ENABLE_ADAPTER_MINHOST "Build the minimal host adapter" ON)
option(ORPHEUS_ENABLE_ADAPTER_REAPER "Build the REAPER adapter" OFF)
option(ORPHEUS_ENABLE_APP_JUCE_HOST "Build the Orpheus JUCE demo host" OFF)
option(ORPHEUS_ENABLE_APP_WAVE_FINDER "Build Orpheus Wave Finder application" OFF)
option(ORPHEUS_BUILD_EXAMPLES "Build Orpheus SDK example applications" OFF)
option(ORPHEUS_ENABLE_SHMUI_JUCE "Build + export the shmui-juce UI component package (requires JUCE)" OFF)
option(ENABLE_LTO "Enable interprocedural optimization" OFF)
option(ORP_BUILD_SHARED_CORE "Build Orpheus ABI libraries as shared" OFF)
option(ORP_BUILD_ABI_DYNAMIC "Build shared core libs for abi_link test" ON)

# M2: Real-time infrastructure options
option(ORPHEUS_ENABLE_REALTIME "Build real-time playback engine (M2)" ON)
option(ORPHEUS_ENABLE_AUDIO_CALLBACK_TIMING
  "Collect bounded device-callback timing diagnostics" OFF)

if(DEFINED ORPHEUS_BUILD_SHARED)
  set(ORP_BUILD_SHARED_CORE ${ORPHEUS_BUILD_SHARED} CACHE BOOL "" FORCE)
endif()

if(DEFINED ORP_BUILD_REAPER)
  message(WARNING "ORP_BUILD_REAPER is deprecated. Use ORPHEUS_ENABLE_ADAPTER_REAPER instead.")
  set(ORPHEUS_ENABLE_ADAPTER_REAPER ${ORP_BUILD_REAPER} CACHE BOOL "" FORCE)
endif()

if(DEFINED ORP_BUILD_MINHOST)
  message(WARNING "ORP_BUILD_MINHOST is deprecated. Use ORPHEUS_ENABLE_ADAPTER_MINHOST instead.")
  set(ORPHEUS_ENABLE_ADAPTER_MINHOST ${ORP_BUILD_MINHOST} CACHE BOOL "" FORCE)
endif()

if(NOT ORPHEUS_ENABLE_ADAPTERS)
  set(ORPHEUS_ENABLE_ADAPTER_MINHOST OFF CACHE BOOL "" FORCE)
  set(ORPHEUS_ENABLE_ADAPTER_REAPER OFF CACHE BOOL "" FORCE)
endif()

if(ORP_BUILD_ABI_DYNAMIC)
  set(ORP_BUILD_SHARED_CORE ON CACHE BOOL "" FORCE)
endif()

include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
  set(CMAKE_THREAD_PREFER_PTHREAD OFF)
  set(THREADS_PREFER_PTHREAD_FLAG OFF)
  # Suppress CRT secure warnings for standard C functions
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

find_package(Threads REQUIRED)

if(TARGET Threads::Threads AND NOT WIN32)
  set(ORPHEUS_THREADS_TARGET Threads::Threads)
endif()

if(APPLE)
  # Use libc++ across all builds
  # Note: CMAKE_OSX_ARCHITECTURES can be overridden via command line for universal builds
  if(NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
  endif()
  add_compile_options(-stdlib=libc++)
  add_link_options(-stdlib=libc++)
endif()

if(APPLE)
  set(CMAKE_INSTALL_RPATH "@loader_path")
  set(CMAKE_BUILD_RPATH "${CMAKE_BINARY_DIR}")
elseif(UNIX)
  set(CMAKE_INSTALL_RPATH "$ORIGIN")
  set(CMAKE_BUILD_RPATH "${CMAKE_BINARY_DIR}")
endif()

include(${ORPHEUS_ROOT_SOURCE_DIR}/cmake/Sanitizers.cmake)

if(ENABLE_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
  if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
  else()
    message(WARNING "IPO not supported: ${ipo_error}")
  endif()
endif()

add_subdirectory(src)

if(ORPHEUS_ENABLE_ADAPTER_REAPER OR ORPHEUS_ENABLE_ADAPTER_MINHOST)
  add_subdirectory(adapters/shared)
endif()

if(ORPHEUS_ENABLE_ADAPTER_REAPER)
  message(WARNING "REAPER adapter is EXPERIMENTAL and may change without notice. Not recommended for production use. See adapters/reaper/README.md for details.")
  add_subdirectory(adapters/reaper)
endif()

if(ORPHEUS_ENABLE_ADAPTER_MINHOST)
  add_subdirectory(adapters/minhost)
endif()

if(ORPHEUS_ENABLE_APP_JUCE_HOST)
  add_subdirectory(apps/juce-demo-host)
endif()

if(ORPHEUS_ENABLE_APP_WAVE_FINDER)
  add_subdirectory(apps/wave-finder)
endif()

# shmui-juce UI components — built + exported only on request, and only when a
# JUCE target is available (the core SDK does not depend on JUCE). Consumers
# that provide JUCE (e.g. via FetchContent) can then link Orpheus::shmui_juce.
if(ORPHEUS_ENABLE_SHMUI_JUCE)
  if(TARGET juce::juce_gui_basics)
    if(NOT TARGET orpheus_shmui_juce)
      add_subdirectory(packages/shmui-juce)
    endif()
  else()
    message(WARNING
      "ORPHEUS_ENABLE_SHMUI_JUCE=ON but no juce::juce_gui_basics target is "
      "available; skipping shmui-juce. Provide JUCE before configuring the SDK.")
  endif()
endif()
# Every independently linkable public library carries generated SDK includes.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/TreefallCompatibility.cmake")
foreach(_treefall_entry IN LISTS TREEFALL_COMPATIBILITY_TARGETS)
  string(REPLACE "|" ";" _treefall_pair "${_treefall_entry}")
  list(GET _treefall_pair 1 _treefall_native)
  if(TARGET "${_treefall_native}")
    target_include_directories("${_treefall_native}" INTERFACE
      $<BUILD_INTERFACE:${ORPHEUS_GENERATED_INCLUDE_DIR}>
      $<INSTALL_INTERFACE:include>)
  endif()
endforeach()

treefall_register_compatibility_targets()

include(CTest)
if(ORP_WITH_TESTS)
  include(${ORPHEUS_ROOT_SOURCE_DIR}/cmake/Dependencies.cmake)
  enable_testing()
endif()

add_subdirectory(tools)

if(ORPHEUS_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(ORP_WITH_TESTS)
  add_subdirectory(tests)
endif()

install(DIRECTORY include/orpheus DESTINATION include)
install(FILES
  "${ORPHEUS_GENERATED_INCLUDE_DIR}/orpheus/version.h"
  DESTINATION include/orpheus)
install(DIRECTORY include/treefall DESTINATION include)
install(DIRECTORY "${TREEFALL_GENERATED_INCLUDE_DIR}/" DESTINATION include/treefall)

install(TARGETS orpheus_session orpheus_clipgrid orpheus_render
  EXPORT OrpheusSDKTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

# M2: Real-time infrastructure targets
if(ORPHEUS_ENABLE_REALTIME)
  install(TARGETS orpheus_transport orpheus_routing
    EXPORT OrpheusSDKTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)

  if(TARGET orpheus_audio_io)
    install(TARGETS orpheus_audio_io
      EXPORT OrpheusSDKTargets
      ARCHIVE DESTINATION lib
      LIBRARY DESTINATION lib
      RUNTIME DESTINATION bin)
  endif()

  if(TARGET orpheus_audio_utils)
    install(TARGETS orpheus_audio_utils
      EXPORT OrpheusSDKTargets
      ARCHIVE DESTINATION lib
      LIBRARY DESTINATION lib
      RUNTIME DESTINATION bin)
  endif()

  if(TARGET orpheus_audio_driver_manager)
    install(TARGETS orpheus_audio_driver_manager
      EXPORT OrpheusSDKTargets
      ARCHIVE DESTINATION lib
      LIBRARY DESTINATION lib
      RUNTIME DESTINATION bin)
  endif()

  # Platform audio drivers
  if(TARGET orpheus_audio_driver_coreaudio)
    install(TARGETS orpheus_audio_driver_coreaudio
      EXPORT OrpheusSDKTargets
      ARCHIVE DESTINATION lib
      LIBRARY DESTINATION lib
      RUNTIME DESTINATION bin)
  endif()

  if(TARGET orpheus_audio_driver_wasapi)
    install(TARGETS orpheus_audio_driver_wasapi
      EXPORT OrpheusSDKTargets
      ARCHIVE DESTINATION lib
      LIBRARY DESTINATION lib
      RUNTIME DESTINATION bin)
  endif()

endif()

install(TARGETS orpheus_core
  EXPORT OrpheusSDKTargets)

install(TARGETS orpheus_diagnostics
  EXPORT OrpheusSDKTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

install(TARGETS orpheus_sanitizers
  EXPORT OrpheusSDKTargets)

# Export shmui-juce ONLY when the SDK itself was asked to build it for
# distribution (ORPHEUS_ENABLE_SHMUI_JUCE). A consumer app that merely adds the
# package as a subdirectory (wave-finder, Clip Composer) creates the target too,
# but must NOT trigger an install/export here — its JUCE modules come from a
# per-app FetchContent and are not in OrpheusSDKTargets, which would make the
# export fail. In that mode the app links orpheus_shmui_juce directly.
if(ORPHEUS_ENABLE_SHMUI_JUCE AND TARGET orpheus_shmui_juce)
  install(TARGETS orpheus_shmui_juce
    EXPORT OrpheusSDKTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)
  install(DIRECTORY packages/shmui-juce/
    DESTINATION include/orpheus/shmui-juce
    FILES_MATCHING
      PATTERN "*.h"
      PATTERN "*.hpp")
  if(TARGET orpheus_shmui_juce_gl)
    install(TARGETS orpheus_shmui_juce_gl
      EXPORT OrpheusSDKTargets
      ARCHIVE DESTINATION lib
      LIBRARY DESTINATION lib
      RUNTIME DESTINATION bin)
  endif()
endif()

if(ORPHEUS_ENABLE_ADAPTER_MINHOST)
  install(TARGETS orpheus_minhost
    EXPORT OrpheusSDKTargets
    RUNTIME DESTINATION bin)
endif()

if(ORPHEUS_ENABLE_ADAPTER_REAPER)
  install(TARGETS reaper_orpheus
    EXPORT OrpheusSDKTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)
endif()

if(ORPHEUS_ENABLE_ADAPTER_REAPER)
  install(DIRECTORY adapters/reaper/include/ DESTINATION include)
endif()

if(TARGET orpheus_perf_render_click)
  install(TARGETS orpheus_perf_render_click
    EXPORT OrpheusSDKTargets
    RUNTIME DESTINATION bin)
endif()

install(FILES README.md LICENSE DESTINATION .)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/orpheus-sdk.json"
  "${CMAKE_CURRENT_BINARY_DIR}/installed-targets.json"
  DESTINATION share/orpheus)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/treefall-sdk.json" DESTINATION share/treefall)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/treefall-installed-targets.json"
  DESTINATION share/treefall RENAME installed-targets.json)

set(INSTALL_CONFIGDIR lib/cmake/OrpheusSDK)

install(EXPORT OrpheusSDKTargets
  FILE OrpheusSDKTargets.cmake
  NAMESPACE Orpheus::
  DESTINATION ${INSTALL_CONFIGDIR})

configure_package_config_file(
  ${ORPHEUS_ROOT_SOURCE_DIR}/cmake/OrpheusSDKConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/OrpheusSDKConfig.cmake
  INSTALL_DESTINATION ${INSTALL_CONFIGDIR})

if(PROJECT_VERSION_MAJOR EQUAL 0)
  set(ORPHEUS_PACKAGE_COMPATIBILITY SameMinorVersion)
else()
  set(ORPHEUS_PACKAGE_COMPATIBILITY SameMajorVersion)
endif()

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/OrpheusSDKConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY ${ORPHEUS_PACKAGE_COMPATIBILITY})

set(TREEFALL_ORPHEUS_CONFIG_RELATIVE OrpheusSDKConfig.cmake)
configure_package_config_file(
  "${ORPHEUS_ROOT_SOURCE_DIR}/cmake/TreefallSDKConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/TreefallSDKConfig.cmake"
  INSTALL_DESTINATION lib/cmake/TreefallSDK)
set(TREEFALL_ORPHEUS_CONFIG_RELATIVE ../OrpheusSDK/OrpheusSDKConfig.cmake)
configure_package_config_file(
  "${ORPHEUS_ROOT_SOURCE_DIR}/cmake/TreefallSDKConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/install-config/TreefallSDKConfig.cmake"
  INSTALL_DESTINATION lib/cmake/TreefallSDK)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/TreefallSDKConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY ${ORPHEUS_PACKAGE_COMPATIBILITY})
configure_file("${ORPHEUS_ROOT_SOURCE_DIR}/cmake/TreefallCompatibility.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/TreefallCompatibility.cmake" COPYONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/install-config/TreefallSDKConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/TreefallSDKConfigVersion.cmake"
  DESTINATION lib/cmake/TreefallSDK)
install(FILES "${ORPHEUS_ROOT_SOURCE_DIR}/cmake/TreefallCompatibility.cmake"
  DESTINATION ${INSTALL_CONFIGDIR})

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/OrpheusSDKConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/OrpheusSDKConfigVersion.cmake
  DESTINATION ${INSTALL_CONFIGDIR})

# A build-tree export cannot preserve the local JUCE module targets used by the
# optional package. The installed export carries their install-interface names.
if(NOT (ORPHEUS_ENABLE_SHMUI_JUCE AND TARGET orpheus_shmui_juce))
  export(EXPORT OrpheusSDKTargets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/OrpheusSDKTargets.cmake
    NAMESPACE Orpheus::)
endif()

set(CPACK_PACKAGE_NAME "treefall-sdk")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "treefall-sdk-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_GENERATOR "ZIP")
set(CPACK_RESOURCE_FILE_LICENSE "${ORPHEUS_ROOT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${ORPHEUS_ROOT_SOURCE_DIR}/README.md")

include(CPack)
