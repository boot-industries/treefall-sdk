cmake_minimum_required(VERSION 3.20)

project(reaper_sdk_examples)

cmake_policy(SET CMP0135 NEW)

# ==== one-shot target/test guards (inserted by fix/cmake-dup-targets) ====
macro(or_safe_add_library name)
  if(NOT TARGET ${name})
    add_library(${name} ${ARGN})
  endif()
endmacro()

macro(or_safe_add_executable name)
  if(NOT TARGET ${name})
    add_executable(${name} ${ARGN})
  endif()
endmacro()

# Usage: or_safe_add_test(<name> COMMAND <exe> [args...])
# Ensures the test is only registered once globally.
function(or_safe_add_test name)
  if(BUILD_TESTING)
    get_property(_had GLOBAL PROPERTY TEST_ADDED_${name})
    if(NOT _had)
      add_test(NAME ${name} ${ARGN})
      set_property(GLOBAL PROPERTY TEST_ADDED_${name} TRUE)
    endif()
  endif()
endfunction()
# ==== end one-shot guards ====

option(ORPHEUS_BUILD_REAPER_PLUGINS "Build REAPER plug-ins" OFF)
option(ORPHEUS_ENABLE_TESTS "Enable Orpheus SDK tests" ON)
option(BUILD_REAPER_CSURF "Build reaper_csurf plug-in" ON)
option(BUILD_REAPER_ATMOS "Build reaper_atmos plug-in" ON)
option(BUILD_REAPER_MP3 "Build reaper_mp3 plug-in" ON)
option(BUILD_REAPER_STT "Build reaper_stt plug-in" ON)

set(WDL_PATH "${CMAKE_SOURCE_DIR}/WDL")
if(EXISTS "${WDL_PATH}/WDL" AND NOT EXISTS "${WDL_PATH}/db2val.h")
  message(STATUS "Flattening Cockos WDL into ${WDL_PATH}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${WDL_PATH}/WDL" "${WDL_PATH}")
endif()

# Ensure consistent timestamps for FetchContent
set(DOWNLOAD_EXTRACT_TIMESTAMP true)

include(CTest)
include(FetchContent)

# Verify that the WDL headers are available
set(WDL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/WDL/WDL")
if(EXISTS "${WDL_ROOT}/wdltypes.h")
  set(WDL_FOUND ON)
else()
  set(WDL_FOUND OFF)
  message(WARNING "WDL not found; sample plug-ins will not be built")
endif()

# Third-party dependencies
set(IXWEBSOCKET_USE_TLS OFF CACHE BOOL "Use OpenSSL for TLS support")
set(IXWEBSOCKET_ENABLE_MBED_TLS OFF CACHE BOOL "Disable mbed TLS" FORCE)
set(IXWEBSOCKET_ENABLE_OPEN_SSL OFF CACHE BOOL "Disable optional OpenSSL helpers")
set(IXWEBSOCKET_BUILD_TESTS OFF CACHE BOOL "Disable ixwebsocket tests" FORCE)
set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "Skip ixwebsocket install rules" FORCE)

FetchContent_Declare(
  ixwebsocket
  URL https://github.com/machinezone/IXWebSocket/archive/refs/tags/v11.4.6.zip
)
FetchContent_MakeAvailable(ixwebsocket)

or_safe_add_library(reaper_stream STATIC sdk/reaper_stream/reaper_stream.cpp)
target_link_libraries(reaper_stream PUBLIC ixwebsocket::ixwebsocket)
target_include_directories(reaper_stream PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk)
target_compile_features(reaper_stream PUBLIC cxx_std_17)

or_safe_add_library(track_playlist STATIC sdk/track_playlist/track_playlist.cpp)
target_compile_features(track_playlist PUBLIC cxx_std_17)
target_include_directories(track_playlist PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk)

or_safe_add_executable(track_playlist_demo sdk/track_playlist/track_playlist_demo.cpp)
target_link_libraries(track_playlist_demo PRIVATE track_playlist)

if(BUILD_TESTING AND ORPHEUS_ENABLE_TESTS)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
  FetchContent_MakeAvailable(googletest)

  find_package(Threads REQUIRED)

  or_safe_add_executable(track_playlist_test sdk/track_playlist/track_playlist_test.cpp)
  target_link_libraries(track_playlist_test
    PRIVATE
      gmock_main
      track_playlist
      Threads::Threads
  )
  target_include_directories(track_playlist_test PRIVATE
    ${googletest_SOURCE_DIR}/googletest/include
    ${googletest_SOURCE_DIR}/googlemock/include
  )
  target_compile_features(track_playlist_test PRIVATE cxx_std_17)

  or_safe_add_test(track_playlist_test COMMAND track_playlist_test)
endif()

if(ORPHEUS_BUILD_REAPER_PLUGINS)
  set(_missing_prereqs)

  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/sdk/reaper_plugin.h")
    list(APPEND _missing_prereqs "${CMAKE_SOURCE_DIR}/sdk/reaper_plugin.h")
  endif()
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/WDL/swell/swell.h")
    list(APPEND _missing_prereqs "${CMAKE_SOURCE_DIR}/WDL/swell/swell.h")
  endif()
  if(NOT WDL_FOUND)
    list(APPEND _missing_prereqs "${WDL_ROOT}/wdltypes.h")
  endif()

  if(_missing_prereqs)
    string(JOIN "\n  - " _missing_list ${_missing_prereqs})
    message(FATAL_ERROR "REAPER plug-ins requested but prerequisites missing:\n  - ${_missing_list}")
  endif()

  message(STATUS "REAPER plug-ins: ON")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

  if(BUILD_REAPER_CSURF)
    add_subdirectory(reaper-plugins/reaper_csurf)
  endif()
  if(BUILD_REAPER_ATMOS)
    add_subdirectory(reaper-plugins/reaper_atmos)
  endif()
  if(BUILD_REAPER_MP3)
    add_subdirectory(reaper-plugins/reaper_mp3)
  endif()
  if(BUILD_REAPER_STT)
    add_subdirectory(reaper-plugins/reaper_stt)
  endif()
else()
  message(STATUS "REAPER plug-ins: OFF (set ORPHEUS_BUILD_REAPER_PLUGINS=ON to enable)")
endif()
