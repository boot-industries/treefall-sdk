# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.22)

project(orpheus VERSION 0.2.0 LANGUAGES CXX)

option(ORP_WITH_TESTS "Build Orpheus tests" ON)
option(ORPHEUS_ENABLE_ADAPTERS "Build Orpheus host adapters" ON)
option(ORPHEUS_ENABLE_ADAPTER_MINHOST "Build the minimal host adapter" ON)
option(ORPHEUS_ENABLE_ADAPTER_REAPER "Build the REAPER adapter" OFF)
option(ORPHEUS_ENABLE_APP_JUCE_HOST "Build the Orpheus JUCE demo host" OFF)
option(ENABLE_LTO "Enable interprocedural optimization" OFF)
option(ORP_BUILD_SHARED_CORE "Build Orpheus ABI libraries as shared" OFF)
option(ORP_BUILD_ABI_DYNAMIC "Build shared core libs for abi_link test" ON)

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
endif()

find_package(Threads REQUIRED)

if(TARGET Threads::Threads AND NOT WIN32)
  set(ORPHEUS_THREADS_TARGET Threads::Threads)
endif()

if(APPLE)
  # Force a single architecture slice and ensure libc++ is used across all builds.
  set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
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

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options("$<$<CONFIG:Debug>:-fsanitize=address>")
  add_link_options("$<$<CONFIG:Debug>:-fsanitize=address>")
else()
  message(STATUS "Sanitizers are not enabled on this compiler.")
endif()

option(ORP_ENABLE_UBSAN "Enable Undefined Behavior Sanitizer in Debug" ON)

if(ORP_ENABLE_UBSAN AND CMAKE_BUILD_TYPE MATCHES "Debug" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-fsanitize=undefined)
  add_link_options(-fsanitize=undefined)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # GCC-based UBSan builds sometimes require explicit runtime linkage.
    set(ORP_EXTRA_UBSAN_LIB ubsan)
  endif()
endif()

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

if(ORPHEUS_ENABLE_ADAPTER_REAPER)
  add_subdirectory(adapters/reaper)
endif()

if(ORPHEUS_ENABLE_ADAPTER_MINHOST)
  add_subdirectory(adapters/minhost)
endif()

if(ORPHEUS_ENABLE_APP_JUCE_HOST)
  add_subdirectory(apps/juce-demo-host)
endif()

include(CTest)
if(ORP_WITH_TESTS)
  include(${CMAKE_SOURCE_DIR}/cmake/Dependencies.cmake)
  enable_testing()
endif()

add_subdirectory(tools)

if(ORP_WITH_TESTS)
  add_subdirectory(tests)
endif()

install(DIRECTORY include/orpheus DESTINATION include)

install(TARGETS orpheus_session orpheus_clipgrid orpheus_render
  EXPORT OrpheusSDKTargets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

install(TARGETS orpheus_core
  EXPORT OrpheusSDKTargets)

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

set(INSTALL_CONFIGDIR lib/cmake/OrpheusSDK)

install(EXPORT OrpheusSDKTargets
  FILE OrpheusSDKTargets.cmake
  NAMESPACE Orpheus::
  DESTINATION ${INSTALL_CONFIGDIR})

configure_package_config_file(
  ${CMAKE_SOURCE_DIR}/cmake/OrpheusSDKConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/OrpheusSDKConfig.cmake
  INSTALL_DESTINATION ${INSTALL_CONFIGDIR})

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/OrpheusSDKConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/OrpheusSDKConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/OrpheusSDKConfigVersion.cmake
  DESTINATION ${INSTALL_CONFIGDIR})

export(EXPORT OrpheusSDKTargets
  FILE ${CMAKE_CURRENT_BINARY_DIR}/OrpheusSDKTargets.cmake
  NAMESPACE Orpheus::)

set(CPACK_PACKAGE_NAME "orpheus-sdk")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "orpheus-sdk-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_GENERATOR "ZIP")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

include(CPack)
