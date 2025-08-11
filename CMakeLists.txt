# ---- GTest/GMock includes (global) ----
include_directories(${CMAKE_BINARY_DIR}/_deps/googletest-src/googletest/include ${CMAKE_BINARY_DIR}/_deps/googletest-src/googlemock/include)

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


cmake_minimum_required(VERSION 3.20)

project(reaper_sdk_examples)

# ---- define-once helpers for duplicate-merge safety ----
macro(add_library_if_missing name)
  if(NOT TARGET ${name})
    add_library(${name} ${ARGN})
  endif()
endmacro()

macro(add_executable_if_missing name)
  if(NOT TARGET ${name})
    add_executable(${name} ${ARGN})
  endif()
endmacro()
# ---------------------------------------------------------

option(ORPHEUS_BUILD_REAPER_PLUGINS "Build REAPER plug-ins" ON)
set(WDL_PATH "${CMAKE_SOURCE_DIR}/WDL")
if(EXISTS "${WDL_PATH}/WDL" AND NOT EXISTS "${WDL_PATH}/db2val.h")
  message(STATUS "Flattening Cockos WDL into ${WDL_PATH}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${WDL_PATH}/WDL" "${WDL_PATH}")
endif()
# Ensure consistent timestamps for FetchContent
set(DOWNLOAD_EXTRACT_TIMESTAMP true)

include(CTest)

# Verify that the WDL headers are available
set(WDL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/WDL/WDL")
if(EXISTS "${WDL_ROOT}/wdltypes.h")
  set(WDL_FOUND ON)
else()
  set(WDL_FOUND OFF)
  message(WARNING "WDL not found; sample plug-ins will not be built")
endif()
if(NOT TARGET track_playlist)
if(NOT TARGET track_playlist)


or_safe_add_library(track_playlist STATIC sdk/track_playlist/track_playlist.cpp)
endif()
target_compile_features(track_playlist PUBLIC cxx_std_17)
target_include_directories(track_playlist PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk)
if(NOT TARGET track_playlist_demo)
if(NOT TARGET track_playlist_demo)


or_safe_add_executable(track_playlist_demo sdk/track_playlist/track_playlist_demo.cpp)
target_link_libraries(track_playlist_demo PRIVATE track_playlist)
endif()
if(WDL_FOUND)
  # Sample plug-ins
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

  option(BUILD_REAPER_CSURF "Build reaper_csurf plug-in" ON)
  option(BUILD_REAPER_ATMOS "Build reaper_atmos plug-in" ON)
  option(BUILD_REAPER_MP3 "Build reaper_mp3 plug-in" ON)
  option(BUILD_REAPER_STT "Build reaper_stt plug-in" ON)

  if(BUILD_REAPER_CSURF)
#     add_subdirectory(reaper-plugins/reaper_csurf)
  endif()
  if(BUILD_REAPER_ATMOS)
#     add_subdirectory(reaper-plugins/reaper_atmos)
  endif()
  if(BUILD_REAPER_MP3)
#     add_subdirectory(reaper-plugins/reaper_mp3)
  endif()
  if(BUILD_REAPER_STT)
#     add_subdirectory(reaper-plugins/reaper_stt)
  endif()
endif()

if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
endif()
endif()
  FetchContent_MakeAvailable(googletest)
if(ORPHEUS_ENABLE_TESTS AND NOT TARGET track_playlist_test)

  or_safe_add_executable(track_playlist_test sdk/track_playlist/track_playlist_test.cpp)
target_link_libraries(track_playlist_test
  PRIVATE
    gmock_main
    track_playlist
    Threads::Threads
)
target_include_directories(track_playlist_test PRIVATE ${googletest_SOURCE_DIR}/googletest/include ${googletest_SOURCE_DIR}/googlemock/include)
  target_compile_features(track_playlist_test PRIVATE cxx_std_17)
  find_package(Threads REQUIRED)
# add_test guarded to avoid duplicate registration
if(TARGET track_playlist_test AND NOT DEFINED __TRACK_PLAYLIST_TEST_ADDED)

  or_safe_add_test(track_playlist_test COMMAND track_playlist_test)
endif()
  set(__TRACK_PLAYLIST_TEST_ADDED TRUE)
endif()
endif()
cmake_minimum_required(VERSION 3.20)

cmake_minimum_required(VERSION 3.15)
project(reaper_sdk_examples)
option(ORPHEUS_BUILD_REAPER_PLUGINS "Build REAPER plug-ins" OFF)
set(WDL_PATH "${CMAKE_SOURCE_DIR}/WDL")
if(EXISTS "${WDL_PATH}/WDL" AND NOT EXISTS "${WDL_PATH}/db2val.h")
  message(STATUS "Flattening Cockos WDL into ${WDL_PATH}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${WDL_PATH}/WDL" "${WDL_PATH}")
endif()
# Ensure consistent timestamps for FetchContent
set(DOWNLOAD_EXTRACT_TIMESTAMP true)

include(CTest)

# Verify that the WDL headers are available
set(WDL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/WDL/WDL")
if(EXISTS "${WDL_ROOT}/wdltypes.h")
  set(WDL_FOUND ON)
else()
  set(WDL_FOUND OFF)
  message(WARNING "WDL not found; sample plug-ins will not be built")
endif()
if(NOT TARGET track_playlist)

or_safe_add_library(track_playlist STATIC sdk/track_playlist/track_playlist.cpp)
target_compile_features(track_playlist PUBLIC cxx_std_17)
target_include_directories(track_playlist PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk)
if(NOT TARGET track_playlist_demo)

or_safe_add_executable(track_playlist_demo sdk/track_playlist/track_playlist_demo.cpp)
if(WDL_FOUND)
  # Sample plug-ins
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

  option(BUILD_REAPER_CSURF "Build reaper_csurf plug-in" ON)
  option(BUILD_REAPER_ATMOS "Build reaper_atmos plug-in" ON)
  option(BUILD_REAPER_MP3 "Build reaper_mp3 plug-in" ON)
  option(BUILD_REAPER_STT "Build reaper_stt plug-in" ON)

  if(BUILD_REAPER_CSURF)
#     add_subdirectory(reaper-plugins/reaper_csurf)
  endif()
  if(BUILD_REAPER_ATMOS)
#     add_subdirectory(reaper-plugins/reaper_atmos)
  endif()
  if(BUILD_REAPER_MP3)
#     add_subdirectory(reaper-plugins/reaper_mp3)
  endif()
  if(BUILD_REAPER_STT)
#     add_subdirectory(reaper-plugins/reaper_stt)
  endif()
endif()

if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
endif()
endif()
  FetchContent_MakeAvailable(googletest)

  or_safe_add_executable(track_playlist_test sdk/track_playlist/track_playlist_test.cpp)
  target_compile_features(track_playlist_test PRIVATE cxx_std_17)
    or_safe_add_test(track_playlist_test COMMAND track_playlist_test)
endif()
# ---- REAPER plugins status ----
if(NOT ORPHEUS_BUILD_REAPER_PLUGINS)
  message(STATUS "REAPER plug-ins: OFF (set ORPHEUS_BUILD_REAPER_PLUGINS=ON to enable)")
else()
  # Minimal sanity: need SDK & WDL bits if user turns this on
  set(_SDK_H "${CMAKE_SOURCE_DIR}/sdk/reaper_plugin.h")
  set(_WDL_SWELL "${CMAKE_SOURCE_DIR}/WDL/swell/swell.h")
  if(NOT EXISTS "${_SDK_H}" OR NOT EXISTS "${_WDL_SWELL}")
    message(FATAL_ERROR "REAPER plug-ins: ON but prerequisites missing:
- ${_SDK_H}
- ${_WDL_SWELL}
Provide REAPER SDK + full WDL (or vendor as needed).")
  endif()
  message(STATUS "REAPER plug-ins: ON")
endif()
# ---- end status ----
cmake_minimum_required(VERSION 3.20)

cmake_minimum_required(VERSION 3.15)
project(reaper_sdk_examples)
option(ORPHEUS_BUILD_REAPER_PLUGINS "Build REAPER plug-ins" OFF)
set(WDL_PATH "${CMAKE_SOURCE_DIR}/WDL")
if(EXISTS "${WDL_PATH}/WDL" AND NOT EXISTS "${WDL_PATH}/db2val.h")
  message(STATUS "Flattening Cockos WDL into ${WDL_PATH}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${WDL_PATH}/WDL" "${WDL_PATH}")
endif()
# Ensure consistent timestamps for FetchContent
set(DOWNLOAD_EXTRACT_TIMESTAMP true)

include(CTest)

# Verify that the WDL headers are available
set(WDL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/WDL/WDL")
if(EXISTS "${WDL_ROOT}/wdltypes.h")
  set(WDL_FOUND ON)
else()
  set(WDL_FOUND OFF)
  message(WARNING "WDL not found; sample plug-ins will not be built")
endif()
if(NOT TARGET track_playlist)

or_safe_add_library(track_playlist STATIC sdk/track_playlist/track_playlist.cpp)
target_compile_features(track_playlist PUBLIC cxx_std_17)
target_include_directories(track_playlist PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk)
if(NOT TARGET track_playlist_demo)

or_safe_add_executable(track_playlist_demo sdk/track_playlist/track_playlist_demo.cpp)
if(WDL_FOUND)
  # Sample plug-ins
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

  option(BUILD_REAPER_CSURF "Build reaper_csurf plug-in" ON)
  option(BUILD_REAPER_ATMOS "Build reaper_atmos plug-in" ON)
  option(BUILD_REAPER_MP3 "Build reaper_mp3 plug-in" ON)
  option(BUILD_REAPER_STT "Build reaper_stt plug-in" ON)

  if(BUILD_REAPER_CSURF)
#     add_subdirectory(reaper-plugins/reaper_csurf)
  endif()
  if(BUILD_REAPER_ATMOS)
#     add_subdirectory(reaper-plugins/reaper_atmos)
  endif()
  if(BUILD_REAPER_MP3)
#     add_subdirectory(reaper-plugins/reaper_mp3)
  endif()
  if(BUILD_REAPER_STT)
#     add_subdirectory(reaper-plugins/reaper_stt)
  endif()
endif()

if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
endif()
endif()
  FetchContent_MakeAvailable(googletest)

  or_safe_add_executable(track_playlist_test sdk/track_playlist/track_playlist_test.cpp)
  target_compile_features(track_playlist_test PRIVATE cxx_std_17)
    or_safe_add_test(track_playlist_test COMMAND track_playlist_test)
endif()
cmake_minimum_required(VERSION 3.20)

cmake_minimum_required(VERSION 3.15)
project(reaper_sdk_examples)
option(ORPHEUS_BUILD_REAPER_PLUGINS "Build REAPER plug-ins" OFF)
set(WDL_PATH "${CMAKE_SOURCE_DIR}/WDL")
if(EXISTS "${WDL_PATH}/WDL" AND NOT EXISTS "${WDL_PATH}/db2val.h")
  message(STATUS "Flattening Cockos WDL into ${WDL_PATH}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${WDL_PATH}/WDL" "${WDL_PATH}")
endif()
# Ensure consistent timestamps for FetchContent
set(DOWNLOAD_EXTRACT_TIMESTAMP true)

include(CTest)

# Verify that the WDL headers are available
set(WDL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/WDL/WDL")
if(EXISTS "${WDL_ROOT}/wdltypes.h")
  set(WDL_FOUND ON)
else()
  set(WDL_FOUND OFF)
  message(WARNING "WDL not found; sample plug-ins will not be built")
endif()
if(NOT TARGET track_playlist)

or_safe_add_library(track_playlist STATIC sdk/track_playlist/track_playlist.cpp)
target_compile_features(track_playlist PUBLIC cxx_std_17)
target_include_directories(track_playlist PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk)
if(NOT TARGET track_playlist_demo)

or_safe_add_executable(track_playlist_demo sdk/track_playlist/track_playlist_demo.cpp)
if(WDL_FOUND)
  # Sample plug-ins
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

  option(BUILD_REAPER_CSURF "Build reaper_csurf plug-in" ON)
  option(BUILD_REAPER_ATMOS "Build reaper_atmos plug-in" ON)
  option(BUILD_REAPER_MP3 "Build reaper_mp3 plug-in" ON)
  option(BUILD_REAPER_STT "Build reaper_stt plug-in" ON)

  if(BUILD_REAPER_CSURF)
#     add_subdirectory(reaper-plugins/reaper_csurf)
  endif()
  if(BUILD_REAPER_ATMOS)
#     add_subdirectory(reaper-plugins/reaper_atmos)
  endif()
  if(BUILD_REAPER_MP3)
#     add_subdirectory(reaper-plugins/reaper_mp3)
  endif()
  if(BUILD_REAPER_STT)
#     add_subdirectory(reaper-plugins/reaper_stt)
  endif()
endif()

if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
endif()
endif()
  FetchContent_MakeAvailable(googletest)

  or_safe_add_executable(track_playlist_test sdk/track_playlist/track_playlist_test.cpp)
  target_compile_features(track_playlist_test PRIVATE cxx_std_17)
    or_safe_add_test(track_playlist_test COMMAND track_playlist_test)
endif()
# ---- REAPER plugins status ----
if(NOT ORPHEUS_BUILD_REAPER_PLUGINS)
  message(STATUS "REAPER plug-ins: OFF (set ORPHEUS_BUILD_REAPER_PLUGINS=ON to enable)")
else()
  # Minimal sanity: need SDK & WDL bits if user turns this on
  set(_SDK_H "${CMAKE_SOURCE_DIR}/sdk/reaper_plugin.h")
  set(_WDL_SWELL "${CMAKE_SOURCE_DIR}/WDL/swell/swell.h")
  if(NOT EXISTS "${_SDK_H}" OR NOT EXISTS "${_WDL_SWELL}")
    message(FATAL_ERROR "REAPER plug-ins: ON but prerequisites missing:
- ${_SDK_H}
- ${_WDL_SWELL}
Provide REAPER SDK + full WDL (or vendor as needed).")
  endif()
  message(STATUS "REAPER plug-ins: ON")
endif()
# ---- end status ----
cmake_minimum_required(VERSION 3.20)

cmake_minimum_required(VERSION 3.15)
project(reaper_sdk_examples)
option(ORPHEUS_BUILD_REAPER_PLUGINS "Build REAPER plug-ins" OFF)
set(WDL_PATH "${CMAKE_SOURCE_DIR}/WDL")
if(EXISTS "${WDL_PATH}/WDL" AND NOT EXISTS "${WDL_PATH}/db2val.h")
  message(STATUS "Flattening Cockos WDL into ${WDL_PATH}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${WDL_PATH}/WDL" "${WDL_PATH}")
endif()
# Ensure consistent timestamps for FetchContent
set(DOWNLOAD_EXTRACT_TIMESTAMP true)

include(CTest)

# Verify that the WDL headers are available
set(WDL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/WDL/WDL")
if(EXISTS "${WDL_ROOT}/wdltypes.h")
  set(WDL_FOUND ON)
else()
  set(WDL_FOUND OFF)
  message(WARNING "WDL not found; sample plug-ins will not be built")
endif()
if(NOT TARGET track_playlist)
if(NOT TARGET track_playlist)


or_safe_add_library(track_playlist STATIC sdk/track_playlist/track_playlist.cpp)
endif()
target_compile_features(track_playlist PUBLIC cxx_std_17)
target_include_directories(track_playlist PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk)
if(NOT TARGET track_playlist_demo)
if(NOT TARGET track_playlist_demo)


or_safe_add_executable(track_playlist_demo sdk/track_playlist/track_playlist_demo.cpp)
endif()
if(WDL_FOUND)
  # Sample plug-ins
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

  option(BUILD_REAPER_CSURF "Build reaper_csurf plug-in" ON)
  option(BUILD_REAPER_ATMOS "Build reaper_atmos plug-in" ON)
  option(BUILD_REAPER_MP3 "Build reaper_mp3 plug-in" ON)
  option(BUILD_REAPER_STT "Build reaper_stt plug-in" ON)

  if(BUILD_REAPER_CSURF)
#     add_subdirectory(reaper-plugins/reaper_csurf)
  endif()
  if(BUILD_REAPER_ATMOS)
#     add_subdirectory(reaper-plugins/reaper_atmos)
  endif()
  if(BUILD_REAPER_MP3)
#     add_subdirectory(reaper-plugins/reaper_mp3)
  endif()
  if(BUILD_REAPER_STT)
#     add_subdirectory(reaper-plugins/reaper_stt)
  endif()
endif()

if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
endif()
endif()
  FetchContent_MakeAvailable(googletest)

  or_safe_add_executable(track_playlist_test sdk/track_playlist/track_playlist_test.cpp)
  target_compile_features(track_playlist_test PRIVATE cxx_std_17)
  # add_test guarded to avoid duplicate registration
if(TARGET track_playlist_test AND NOT DEFINED __TRACK_PLAYLIST_TEST_ADDED)

  or_safe_add_test(track_playlist_test COMMAND track_playlist_test)
  set(__TRACK_PLAYLIST_TEST_ADDED TRUE)
endif()
endif()
cmake_minimum_required(VERSION 3.20)

cmake_minimum_required(VERSION 3.15)
project(reaper_sdk_examples)
option(ORPHEUS_BUILD_REAPER_PLUGINS "Build REAPER plug-ins" OFF)
set(WDL_PATH "${CMAKE_SOURCE_DIR}/WDL")
if(EXISTS "${WDL_PATH}/WDL" AND NOT EXISTS "${WDL_PATH}/db2val.h")
  message(STATUS "Flattening Cockos WDL into ${WDL_PATH}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${WDL_PATH}/WDL" "${WDL_PATH}")
endif()
# Ensure consistent timestamps for FetchContent
set(DOWNLOAD_EXTRACT_TIMESTAMP true)

include(CTest)

# Verify that the WDL headers are available
set(WDL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/WDL/WDL")
if(EXISTS "${WDL_ROOT}/wdltypes.h")
  set(WDL_FOUND ON)
else()
  set(WDL_FOUND OFF)
  message(WARNING "WDL not found; sample plug-ins will not be built")
endif()
if(NOT TARGET track_playlist)

or_safe_add_library(track_playlist STATIC sdk/track_playlist/track_playlist.cpp)
target_compile_features(track_playlist PUBLIC cxx_std_17)
target_include_directories(track_playlist PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk)
if(NOT TARGET track_playlist_demo)

or_safe_add_executable(track_playlist_demo sdk/track_playlist/track_playlist_demo.cpp)
if(WDL_FOUND)
  # Sample plug-ins
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

  option(BUILD_REAPER_CSURF "Build reaper_csurf plug-in" ON)
  option(BUILD_REAPER_ATMOS "Build reaper_atmos plug-in" ON)
  option(BUILD_REAPER_MP3 "Build reaper_mp3 plug-in" ON)
  option(BUILD_REAPER_STT "Build reaper_stt plug-in" ON)

  if(BUILD_REAPER_CSURF)
#     add_subdirectory(reaper-plugins/reaper_csurf)
  endif()
  if(BUILD_REAPER_ATMOS)
#     add_subdirectory(reaper-plugins/reaper_atmos)
  endif()
  if(BUILD_REAPER_MP3)
#     add_subdirectory(reaper-plugins/reaper_mp3)
  endif()
  if(BUILD_REAPER_STT)
#     add_subdirectory(reaper-plugins/reaper_stt)
  endif()
endif()

if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
endif()
endif()
  FetchContent_MakeAvailable(googletest)

  or_safe_add_executable(track_playlist_test sdk/track_playlist/track_playlist_test.cpp)
  target_compile_features(track_playlist_test PRIVATE cxx_std_17)
    or_safe_add_test(track_playlist_test COMMAND track_playlist_test)
endif()
# ---- REAPER plugins status ----
if(NOT ORPHEUS_BUILD_REAPER_PLUGINS)
  message(STATUS "REAPER plug-ins: OFF (set ORPHEUS_BUILD_REAPER_PLUGINS=ON to enable)")
else()
  # Minimal sanity: need SDK & WDL bits if user turns this on
  set(_SDK_H "${CMAKE_SOURCE_DIR}/sdk/reaper_plugin.h")
  set(_WDL_SWELL "${CMAKE_SOURCE_DIR}/WDL/swell/swell.h")
  if(NOT EXISTS "${_SDK_H}" OR NOT EXISTS "${_WDL_SWELL}")
    message(FATAL_ERROR "REAPER plug-ins: ON but prerequisites missing:
- ${_SDK_H}
- ${_WDL_SWELL}
Provide REAPER SDK + full WDL (or vendor as needed).")
  endif()
  message(STATUS "REAPER plug-ins: ON")
endif()
# ---- end status ----
cmake_minimum_required(VERSION 3.20)

cmake_minimum_required(VERSION 3.15)
project(reaper_sdk_examples)
option(ORPHEUS_BUILD_REAPER_PLUGINS "Build REAPER plug-ins" OFF)
set(WDL_PATH "${CMAKE_SOURCE_DIR}/WDL")
if(EXISTS "${WDL_PATH}/WDL" AND NOT EXISTS "${WDL_PATH}/db2val.h")
  message(STATUS "Flattening Cockos WDL into ${WDL_PATH}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${WDL_PATH}/WDL" "${WDL_PATH}")
endif()
# Ensure consistent timestamps for FetchContent
set(DOWNLOAD_EXTRACT_TIMESTAMP true)

include(CTest)

# Verify that the WDL headers are available
set(WDL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/WDL/WDL")
if(EXISTS "${WDL_ROOT}/wdltypes.h")
  set(WDL_FOUND ON)
else()
  set(WDL_FOUND OFF)
  message(WARNING "WDL not found; sample plug-ins will not be built")
endif()
if(NOT TARGET track_playlist)

or_safe_add_library(track_playlist STATIC sdk/track_playlist/track_playlist.cpp)
target_compile_features(track_playlist PUBLIC cxx_std_17)
target_include_directories(track_playlist PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk)
if(NOT TARGET track_playlist_demo)

or_safe_add_executable(track_playlist_demo sdk/track_playlist/track_playlist_demo.cpp)
if(WDL_FOUND)
  # Sample plug-ins
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

  option(BUILD_REAPER_CSURF "Build reaper_csurf plug-in" ON)
  option(BUILD_REAPER_ATMOS "Build reaper_atmos plug-in" ON)
  option(BUILD_REAPER_MP3 "Build reaper_mp3 plug-in" ON)
  option(BUILD_REAPER_STT "Build reaper_stt plug-in" ON)

  if(BUILD_REAPER_CSURF)
#     add_subdirectory(reaper-plugins/reaper_csurf)
  endif()
  if(BUILD_REAPER_ATMOS)
#     add_subdirectory(reaper-plugins/reaper_atmos)
  endif()
  if(BUILD_REAPER_MP3)
#     add_subdirectory(reaper-plugins/reaper_mp3)
  endif()
  if(BUILD_REAPER_STT)
#     add_subdirectory(reaper-plugins/reaper_stt)
  endif()
endif()

if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
endif()
endif()
  FetchContent_MakeAvailable(googletest)

  or_safe_add_executable(track_playlist_test sdk/track_playlist/track_playlist_test.cpp)
  target_compile_features(track_playlist_test PRIVATE cxx_std_17)
    or_safe_add_test(track_playlist_test COMMAND track_playlist_test)
endif()
cmake_minimum_required(VERSION 3.20)

cmake_minimum_required(VERSION 3.15)
project(reaper_sdk_examples)
option(ORPHEUS_BUILD_REAPER_PLUGINS "Build REAPER plug-ins" OFF)
set(WDL_PATH "${CMAKE_SOURCE_DIR}/WDL")
if(EXISTS "${WDL_PATH}/WDL" AND NOT EXISTS "${WDL_PATH}/db2val.h")
  message(STATUS "Flattening Cockos WDL into ${WDL_PATH}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${WDL_PATH}/WDL" "${WDL_PATH}")
endif()
# Ensure consistent timestamps for FetchContent
set(DOWNLOAD_EXTRACT_TIMESTAMP true)

include(CTest)

# Verify that the WDL headers are available
set(WDL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/WDL/WDL")
if(EXISTS "${WDL_ROOT}/wdltypes.h")
  set(WDL_FOUND ON)
else()
  set(WDL_FOUND OFF)
  message(WARNING "WDL not found; sample plug-ins will not be built")
endif()
if(NOT TARGET track_playlist)

or_safe_add_library(track_playlist STATIC sdk/track_playlist/track_playlist.cpp)
target_compile_features(track_playlist PUBLIC cxx_std_17)
target_include_directories(track_playlist PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk)
if(NOT TARGET track_playlist_demo)

or_safe_add_executable(track_playlist_demo sdk/track_playlist/track_playlist_demo.cpp)
if(WDL_FOUND)
  # Sample plug-ins
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

  option(BUILD_REAPER_CSURF "Build reaper_csurf plug-in" ON)
  option(BUILD_REAPER_ATMOS "Build reaper_atmos plug-in" ON)
  option(BUILD_REAPER_MP3 "Build reaper_mp3 plug-in" ON)
  option(BUILD_REAPER_STT "Build reaper_stt plug-in" ON)

  if(BUILD_REAPER_CSURF)
#     add_subdirectory(reaper-plugins/reaper_csurf)
  endif()
  if(BUILD_REAPER_ATMOS)
#     add_subdirectory(reaper-plugins/reaper_atmos)
  endif()
  if(BUILD_REAPER_MP3)
#     add_subdirectory(reaper-plugins/reaper_mp3)
  endif()
  if(BUILD_REAPER_STT)
#     add_subdirectory(reaper-plugins/reaper_stt)
  endif()
endif()

if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
endif()
endif()
  FetchContent_MakeAvailable(googletest)

  or_safe_add_executable(track_playlist_test sdk/track_playlist/track_playlist_test.cpp)
  target_compile_features(track_playlist_test PRIVATE cxx_std_17)
    or_safe_add_test(track_playlist_test COMMAND track_playlist_test)
endif()
# ---- REAPER plugins status ----
if(NOT ORPHEUS_BUILD_REAPER_PLUGINS)
  message(STATUS "REAPER plug-ins: OFF (set ORPHEUS_BUILD_REAPER_PLUGINS=ON to enable)")
else()
  # Minimal sanity: need SDK & WDL bits if user turns this on
  set(_SDK_H "${CMAKE_SOURCE_DIR}/sdk/reaper_plugin.h")
  set(_WDL_SWELL "${CMAKE_SOURCE_DIR}/WDL/swell/swell.h")
  if(NOT EXISTS "${_SDK_H}" OR NOT EXISTS "${_WDL_SWELL}")
    message(FATAL_ERROR "REAPER plug-ins: ON but prerequisites missing:
- ${_SDK_H}
- ${_WDL_SWELL}
Provide REAPER SDK + full WDL (or vendor as needed).")
  endif()
  message(STATUS "REAPER plug-ins: ON")
endif()
# ---- end status ----
cmake_minimum_required(VERSION 3.20)

cmake_minimum_required(VERSION 3.15)
project(reaper_sdk_examples)
option(ORPHEUS_BUILD_REAPER_PLUGINS "Build REAPER plug-ins" OFF)
set(WDL_PATH "${CMAKE_SOURCE_DIR}/WDL")
if(EXISTS "${WDL_PATH}/WDL" AND NOT EXISTS "${WDL_PATH}/db2val.h")
  message(STATUS "Flattening Cockos WDL into ${WDL_PATH}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${WDL_PATH}/WDL" "${WDL_PATH}")
endif()
# Ensure consistent timestamps for FetchContent
set(DOWNLOAD_EXTRACT_TIMESTAMP true)

include(CTest)

# Verify that the WDL headers are available
set(WDL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/WDL/WDL")
if(EXISTS "${WDL_ROOT}/wdltypes.h")
  set(WDL_FOUND ON)
else()
  set(WDL_FOUND OFF)
  message(WARNING "WDL not found; sample plug-ins will not be built")
endif()
if(NOT TARGET track_playlist)


or_safe_add_library(track_playlist STATIC sdk/track_playlist/track_playlist.cpp)
endif()
target_compile_features(track_playlist PUBLIC cxx_std_17)
target_include_directories(track_playlist PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk)
if(NOT TARGET track_playlist_demo)


or_safe_add_executable(track_playlist_demo sdk/track_playlist/track_playlist_demo.cpp)
endif()
if(WDL_FOUND)
  # Sample plug-ins
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

  option(BUILD_REAPER_CSURF "Build reaper_csurf plug-in" ON)
  option(BUILD_REAPER_ATMOS "Build reaper_atmos plug-in" ON)
  option(BUILD_REAPER_MP3 "Build reaper_mp3 plug-in" ON)
  option(BUILD_REAPER_STT "Build reaper_stt plug-in" ON)

  if(BUILD_REAPER_CSURF)
#     add_subdirectory(reaper-plugins/reaper_csurf)
  endif()
  if(BUILD_REAPER_ATMOS)
#     add_subdirectory(reaper-plugins/reaper_atmos)
  endif()
  if(BUILD_REAPER_MP3)
#     add_subdirectory(reaper-plugins/reaper_mp3)
  endif()
  if(BUILD_REAPER_STT)
#     add_subdirectory(reaper-plugins/reaper_stt)
  endif()
endif()

if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
  FetchContent_MakeAvailable(googletest)
if(ORPHEUS_ENABLE_TESTS AND NOT TARGET track_playlist_test)

  or_safe_add_executable(track_playlist_test sdk/track_playlist/track_playlist_test.cpp)
  target_compile_features(track_playlist_test PRIVATE cxx_std_17)
  # add_test guarded to avoid duplicate registration
if(TARGET track_playlist_test AND NOT DEFINED __TRACK_PLAYLIST_TEST_ADDED)

  or_safe_add_test(track_playlist_test COMMAND track_playlist_test)
endif()
  set(__TRACK_PLAYLIST_TEST_ADDED TRUE)
endif()
endif()
cmake_minimum_required(VERSION 3.20)

cmake_minimum_required(VERSION 3.15)
project(reaper_sdk_examples)
option(ORPHEUS_BUILD_REAPER_PLUGINS "Build REAPER plug-ins" OFF)
set(WDL_PATH "${CMAKE_SOURCE_DIR}/WDL")
if(EXISTS "${WDL_PATH}/WDL" AND NOT EXISTS "${WDL_PATH}/db2val.h")
  message(STATUS "Flattening Cockos WDL into ${WDL_PATH}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${WDL_PATH}/WDL" "${WDL_PATH}")
endif()
# Ensure consistent timestamps for FetchContent
set(DOWNLOAD_EXTRACT_TIMESTAMP true)

include(CTest)

# Verify that the WDL headers are available
set(WDL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/WDL/WDL")
if(EXISTS "${WDL_ROOT}/wdltypes.h")
  set(WDL_FOUND ON)
else()
  set(WDL_FOUND OFF)
  message(WARNING "WDL not found; sample plug-ins will not be built")
endif()

or_safe_add_library(track_playlist STATIC sdk/track_playlist/track_playlist.cpp)
target_compile_features(track_playlist PUBLIC cxx_std_17)
target_include_directories(track_playlist PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk)

or_safe_add_executable(track_playlist_demo sdk/track_playlist/track_playlist_demo.cpp)
if(WDL_FOUND)
  # Sample plug-ins
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

  option(BUILD_REAPER_CSURF "Build reaper_csurf plug-in" ON)
  option(BUILD_REAPER_ATMOS "Build reaper_atmos plug-in" ON)
  option(BUILD_REAPER_MP3 "Build reaper_mp3 plug-in" ON)
  option(BUILD_REAPER_STT "Build reaper_stt plug-in" ON)

  if(BUILD_REAPER_CSURF)
#     add_subdirectory(reaper-plugins/reaper_csurf)
  endif()
  if(BUILD_REAPER_ATMOS)
#     add_subdirectory(reaper-plugins/reaper_atmos)
  endif()
  if(BUILD_REAPER_MP3)
#     add_subdirectory(reaper-plugins/reaper_mp3)
  endif()
  if(BUILD_REAPER_STT)
#     add_subdirectory(reaper-plugins/reaper_stt)
  endif()
endif()

if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
  FetchContent_MakeAvailable(googletest)

  or_safe_add_executable(track_playlist_test sdk/track_playlist/track_playlist_test.cpp)
  target_compile_features(track_playlist_test PRIVATE cxx_std_17)
    or_safe_add_test(track_playlist_test COMMAND track_playlist_test)
endif()
# ---- REAPER plugins status ----
if(NOT ORPHEUS_BUILD_REAPER_PLUGINS)
  message(STATUS "REAPER plug-ins: OFF (set ORPHEUS_BUILD_REAPER_PLUGINS=ON to enable)")
else()
  # Minimal sanity: need SDK & WDL bits if user turns this on
  set(_SDK_H "${CMAKE_SOURCE_DIR}/sdk/reaper_plugin.h")
  set(_WDL_SWELL "${CMAKE_SOURCE_DIR}/WDL/swell/swell.h")
  if(NOT EXISTS "${_SDK_H}" OR NOT EXISTS "${_WDL_SWELL}")
    message(FATAL_ERROR "REAPER plug-ins: ON but prerequisites missing:
- ${_SDK_H}
- ${_WDL_SWELL}
Provide REAPER SDK + full WDL (or vendor as needed).")
  endif()
  message(STATUS "REAPER plug-ins: ON")
endif()
# ---- end status ----
cmake_minimum_required(VERSION 3.20)

cmake_minimum_required(VERSION 3.15)
project(reaper_sdk_examples)
option(ORPHEUS_BUILD_REAPER_PLUGINS "Build REAPER plug-ins" OFF)
set(WDL_PATH "${CMAKE_SOURCE_DIR}/WDL")
if(EXISTS "${WDL_PATH}/WDL" AND NOT EXISTS "${WDL_PATH}/db2val.h")
  message(STATUS "Flattening Cockos WDL into ${WDL_PATH}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${WDL_PATH}/WDL" "${WDL_PATH}")
endif()
# Ensure consistent timestamps for FetchContent
set(DOWNLOAD_EXTRACT_TIMESTAMP true)

include(CTest)

# Verify that the WDL headers are available
set(WDL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/WDL/WDL")
if(EXISTS "${WDL_ROOT}/wdltypes.h")
  set(WDL_FOUND ON)
else()
  set(WDL_FOUND OFF)
  message(WARNING "WDL not found; sample plug-ins will not be built")
endif()

or_safe_add_library(track_playlist STATIC sdk/track_playlist/track_playlist.cpp)
target_compile_features(track_playlist PUBLIC cxx_std_17)
target_include_directories(track_playlist PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk)

or_safe_add_executable(track_playlist_demo sdk/track_playlist/track_playlist_demo.cpp)
if(WDL_FOUND)
  # Sample plug-ins
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

  option(BUILD_REAPER_CSURF "Build reaper_csurf plug-in" ON)
  option(BUILD_REAPER_ATMOS "Build reaper_atmos plug-in" ON)
  option(BUILD_REAPER_MP3 "Build reaper_mp3 plug-in" ON)
  option(BUILD_REAPER_STT "Build reaper_stt plug-in" ON)

  if(BUILD_REAPER_CSURF)
#     add_subdirectory(reaper-plugins/reaper_csurf)
  endif()
  if(BUILD_REAPER_ATMOS)
#     add_subdirectory(reaper-plugins/reaper_atmos)
  endif()
  if(BUILD_REAPER_MP3)
#     add_subdirectory(reaper-plugins/reaper_mp3)
  endif()
  if(BUILD_REAPER_STT)
#     add_subdirectory(reaper-plugins/reaper_stt)
  endif()
endif()

if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
  FetchContent_MakeAvailable(googletest)

  or_safe_add_executable(track_playlist_test sdk/track_playlist/track_playlist_test.cpp)
  target_compile_features(track_playlist_test PRIVATE cxx_std_17)
    or_safe_add_test(track_playlist_test COMMAND track_playlist_test)
endif()
cmake_minimum_required(VERSION 3.20)

cmake_minimum_required(VERSION 3.15)
project(reaper_sdk_examples)
option(ORPHEUS_BUILD_REAPER_PLUGINS "Build REAPER plug-ins" OFF)
set(WDL_PATH "${CMAKE_SOURCE_DIR}/WDL")
if(EXISTS "${WDL_PATH}/WDL" AND NOT EXISTS "${WDL_PATH}/db2val.h")
  message(STATUS "Flattening Cockos WDL into ${WDL_PATH}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${WDL_PATH}/WDL" "${WDL_PATH}")
endif()
# Ensure consistent timestamps for FetchContent
set(DOWNLOAD_EXTRACT_TIMESTAMP true)

include(CTest)

# Verify that the WDL headers are available
set(WDL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/WDL/WDL")
if(EXISTS "${WDL_ROOT}/wdltypes.h")
  set(WDL_FOUND ON)
else()
  set(WDL_FOUND OFF)
  message(WARNING "WDL not found; sample plug-ins will not be built")
endif()

or_safe_add_library(track_playlist STATIC sdk/track_playlist/track_playlist.cpp)
target_compile_features(track_playlist PUBLIC cxx_std_17)
target_include_directories(track_playlist PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk)

or_safe_add_executable(track_playlist_demo sdk/track_playlist/track_playlist_demo.cpp)
if(WDL_FOUND)
  # Sample plug-ins
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

  option(BUILD_REAPER_CSURF "Build reaper_csurf plug-in" ON)
  option(BUILD_REAPER_ATMOS "Build reaper_atmos plug-in" ON)
  option(BUILD_REAPER_MP3 "Build reaper_mp3 plug-in" ON)
  option(BUILD_REAPER_STT "Build reaper_stt plug-in" ON)

  if(BUILD_REAPER_CSURF)
#     add_subdirectory(reaper-plugins/reaper_csurf)
  endif()
  if(BUILD_REAPER_ATMOS)
#     add_subdirectory(reaper-plugins/reaper_atmos)
  endif()
  if(BUILD_REAPER_MP3)
#     add_subdirectory(reaper-plugins/reaper_mp3)
  endif()
  if(BUILD_REAPER_STT)
#     add_subdirectory(reaper-plugins/reaper_stt)
  endif()
endif()

if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
  FetchContent_MakeAvailable(googletest)

  or_safe_add_executable(track_playlist_test sdk/track_playlist/track_playlist_test.cpp)
  target_compile_features(track_playlist_test PRIVATE cxx_std_17)
    or_safe_add_test(track_playlist_test COMMAND track_playlist_test)
endif()
# ---- REAPER plugins status ----
if(NOT ORPHEUS_BUILD_REAPER_PLUGINS)
  message(STATUS "REAPER plug-ins: OFF (set ORPHEUS_BUILD_REAPER_PLUGINS=ON to enable)")
else()
  # Minimal sanity: need SDK & WDL bits if user turns this on
  set(_SDK_H "${CMAKE_SOURCE_DIR}/sdk/reaper_plugin.h")
  set(_WDL_SWELL "${CMAKE_SOURCE_DIR}/WDL/swell/swell.h")
  if(NOT EXISTS "${_SDK_H}" OR NOT EXISTS "${_WDL_SWELL}")
    message(FATAL_ERROR "REAPER plug-ins: ON but prerequisites missing:
- ${_SDK_H}
- ${_WDL_SWELL}
Provide REAPER SDK + full WDL (or vendor as needed).")
  endif()
  message(STATUS "REAPER plug-ins: ON")
endif()
# ---- end status ----
cmake_minimum_required(VERSION 3.20)

cmake_minimum_required(VERSION 3.15)
project(reaper_sdk_examples)
option(ORPHEUS_BUILD_REAPER_PLUGINS "Build REAPER plug-ins" OFF)
set(WDL_PATH "${CMAKE_SOURCE_DIR}/WDL")
if(EXISTS "${WDL_PATH}/WDL" AND NOT EXISTS "${WDL_PATH}/db2val.h")
  message(STATUS "Flattening Cockos WDL into ${WDL_PATH}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${WDL_PATH}/WDL" "${WDL_PATH}")
endif()
# Ensure consistent timestamps for FetchContent
set(DOWNLOAD_EXTRACT_TIMESTAMP true)

include(CTest)

# Verify that the WDL headers are available
set(WDL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/WDL/WDL")
if(EXISTS "${WDL_ROOT}/wdltypes.h")
  set(WDL_FOUND ON)
else()
  set(WDL_FOUND OFF)
  message(WARNING "WDL not found; sample plug-ins will not be built")
endif()
if(NOT TARGET track_playlist)


or_safe_add_library(track_playlist STATIC sdk/track_playlist/track_playlist.cpp)
endif()
target_compile_features(track_playlist PUBLIC cxx_std_17)
target_include_directories(track_playlist PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk)
if(NOT TARGET track_playlist_demo)


or_safe_add_executable(track_playlist_demo sdk/track_playlist/track_playlist_demo.cpp)
endif()
if(WDL_FOUND)
  # Sample plug-ins
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

  option(BUILD_REAPER_CSURF "Build reaper_csurf plug-in" ON)
  option(BUILD_REAPER_ATMOS "Build reaper_atmos plug-in" ON)
  option(BUILD_REAPER_MP3 "Build reaper_mp3 plug-in" ON)
  option(BUILD_REAPER_STT "Build reaper_stt plug-in" ON)

  if(BUILD_REAPER_CSURF)
#     add_subdirectory(reaper-plugins/reaper_csurf)
  endif()
  if(BUILD_REAPER_ATMOS)
#     add_subdirectory(reaper-plugins/reaper_atmos)
  endif()
  if(BUILD_REAPER_MP3)
#     add_subdirectory(reaper-plugins/reaper_mp3)
  endif()
  if(BUILD_REAPER_STT)
#     add_subdirectory(reaper-plugins/reaper_stt)
  endif()
endif()

if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
  FetchContent_MakeAvailable(googletest)

  or_safe_add_executable(track_playlist_test sdk/track_playlist/track_playlist_test.cpp)
  target_compile_features(track_playlist_test PRIVATE cxx_std_17)
  # add_test guarded to avoid duplicate registration
if(TARGET track_playlist_test AND NOT DEFINED __TRACK_PLAYLIST_TEST_ADDED)

  or_safe_add_test(track_playlist_test COMMAND track_playlist_test)
  set(__TRACK_PLAYLIST_TEST_ADDED TRUE)
endif()
endif()
cmake_minimum_required(VERSION 3.20)

cmake_minimum_required(VERSION 3.15)
project(reaper_sdk_examples)
option(ORPHEUS_BUILD_REAPER_PLUGINS "Build REAPER plug-ins" OFF)
set(WDL_PATH "${CMAKE_SOURCE_DIR}/WDL")
if(EXISTS "${WDL_PATH}/WDL" AND NOT EXISTS "${WDL_PATH}/db2val.h")
  message(STATUS "Flattening Cockos WDL into ${WDL_PATH}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${WDL_PATH}/WDL" "${WDL_PATH}")
endif()
# Ensure consistent timestamps for FetchContent
set(DOWNLOAD_EXTRACT_TIMESTAMP true)

include(CTest)

# Verify that the WDL headers are available
set(WDL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/WDL/WDL")
if(EXISTS "${WDL_ROOT}/wdltypes.h")
  set(WDL_FOUND ON)
else()
  set(WDL_FOUND OFF)
  message(WARNING "WDL not found; sample plug-ins will not be built")
endif()

or_safe_add_library(track_playlist STATIC sdk/track_playlist/track_playlist.cpp)
target_compile_features(track_playlist PUBLIC cxx_std_17)
target_include_directories(track_playlist PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk)

or_safe_add_executable(track_playlist_demo sdk/track_playlist/track_playlist_demo.cpp)
if(WDL_FOUND)
  # Sample plug-ins
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

  option(BUILD_REAPER_CSURF "Build reaper_csurf plug-in" ON)
  option(BUILD_REAPER_ATMOS "Build reaper_atmos plug-in" ON)
  option(BUILD_REAPER_MP3 "Build reaper_mp3 plug-in" ON)
  option(BUILD_REAPER_STT "Build reaper_stt plug-in" ON)

  if(BUILD_REAPER_CSURF)
#     add_subdirectory(reaper-plugins/reaper_csurf)
  endif()
  if(BUILD_REAPER_ATMOS)
#     add_subdirectory(reaper-plugins/reaper_atmos)
  endif()
  if(BUILD_REAPER_MP3)
#     add_subdirectory(reaper-plugins/reaper_mp3)
  endif()
  if(BUILD_REAPER_STT)
#     add_subdirectory(reaper-plugins/reaper_stt)
  endif()
endif()

if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
  FetchContent_MakeAvailable(googletest)

  or_safe_add_executable(track_playlist_test sdk/track_playlist/track_playlist_test.cpp)
  target_compile_features(track_playlist_test PRIVATE cxx_std_17)
    or_safe_add_test(track_playlist_test COMMAND track_playlist_test)
endif()
# ---- REAPER plugins status ----
if(NOT ORPHEUS_BUILD_REAPER_PLUGINS)
  message(STATUS "REAPER plug-ins: OFF (set ORPHEUS_BUILD_REAPER_PLUGINS=ON to enable)")
else()
  # Minimal sanity: need SDK & WDL bits if user turns this on
  set(_SDK_H "${CMAKE_SOURCE_DIR}/sdk/reaper_plugin.h")
  set(_WDL_SWELL "${CMAKE_SOURCE_DIR}/WDL/swell/swell.h")
  if(NOT EXISTS "${_SDK_H}" OR NOT EXISTS "${_WDL_SWELL}")
    message(FATAL_ERROR "REAPER plug-ins: ON but prerequisites missing:
- ${_SDK_H}
- ${_WDL_SWELL}
Provide REAPER SDK + full WDL (or vendor as needed).")
  endif()
  message(STATUS "REAPER plug-ins: ON")
endif()
# ---- end status ----
cmake_minimum_required(VERSION 3.20)

cmake_minimum_required(VERSION 3.15)
project(reaper_sdk_examples)
option(ORPHEUS_BUILD_REAPER_PLUGINS "Build REAPER plug-ins" OFF)
set(WDL_PATH "${CMAKE_SOURCE_DIR}/WDL")
if(EXISTS "${WDL_PATH}/WDL" AND NOT EXISTS "${WDL_PATH}/db2val.h")
  message(STATUS "Flattening Cockos WDL into ${WDL_PATH}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${WDL_PATH}/WDL" "${WDL_PATH}")
endif()
# Ensure consistent timestamps for FetchContent
set(DOWNLOAD_EXTRACT_TIMESTAMP true)

include(CTest)

# Verify that the WDL headers are available
set(WDL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/WDL/WDL")
if(EXISTS "${WDL_ROOT}/wdltypes.h")
  set(WDL_FOUND ON)
else()
  set(WDL_FOUND OFF)
  message(WARNING "WDL not found; sample plug-ins will not be built")
endif()

or_safe_add_library(track_playlist STATIC sdk/track_playlist/track_playlist.cpp)
target_compile_features(track_playlist PUBLIC cxx_std_17)
target_include_directories(track_playlist PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk)

or_safe_add_executable(track_playlist_demo sdk/track_playlist/track_playlist_demo.cpp)
if(WDL_FOUND)
  # Sample plug-ins
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

  option(BUILD_REAPER_CSURF "Build reaper_csurf plug-in" ON)
  option(BUILD_REAPER_ATMOS "Build reaper_atmos plug-in" ON)
  option(BUILD_REAPER_MP3 "Build reaper_mp3 plug-in" ON)
  option(BUILD_REAPER_STT "Build reaper_stt plug-in" ON)

  if(BUILD_REAPER_CSURF)
#     add_subdirectory(reaper-plugins/reaper_csurf)
  endif()
  if(BUILD_REAPER_ATMOS)
#     add_subdirectory(reaper-plugins/reaper_atmos)
  endif()
  if(BUILD_REAPER_MP3)
#     add_subdirectory(reaper-plugins/reaper_mp3)
  endif()
  if(BUILD_REAPER_STT)
#     add_subdirectory(reaper-plugins/reaper_stt)
  endif()
endif()

if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
  FetchContent_MakeAvailable(googletest)

  or_safe_add_executable(track_playlist_test sdk/track_playlist/track_playlist_test.cpp)
  target_compile_features(track_playlist_test PRIVATE cxx_std_17)
    or_safe_add_test(track_playlist_test COMMAND track_playlist_test)
endif()
cmake_minimum_required(VERSION 3.20)

cmake_minimum_required(VERSION 3.15)
project(reaper_sdk_examples)
option(ORPHEUS_BUILD_REAPER_PLUGINS "Build REAPER plug-ins" OFF)
set(WDL_PATH "${CMAKE_SOURCE_DIR}/WDL")
if(EXISTS "${WDL_PATH}/WDL" AND NOT EXISTS "${WDL_PATH}/db2val.h")
  message(STATUS "Flattening Cockos WDL into ${WDL_PATH}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory "${WDL_PATH}/WDL" "${WDL_PATH}")
endif()
# Ensure consistent timestamps for FetchContent
set(DOWNLOAD_EXTRACT_TIMESTAMP true)

include(CTest)

# Verify that the WDL headers are available
set(WDL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/WDL/WDL")
if(EXISTS "${WDL_ROOT}/wdltypes.h")
  set(WDL_FOUND ON)
else()
  set(WDL_FOUND OFF)
  message(WARNING "WDL not found; sample plug-ins will not be built")
endif()

or_safe_add_library(track_playlist STATIC sdk/track_playlist/track_playlist.cpp)
target_compile_features(track_playlist PUBLIC cxx_std_17)
target_include_directories(track_playlist PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk)

or_safe_add_executable(track_playlist_demo sdk/track_playlist/track_playlist_demo.cpp)
if(WDL_FOUND)
  # Sample plug-ins
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

  option(BUILD_REAPER_CSURF "Build reaper_csurf plug-in" ON)
  option(BUILD_REAPER_ATMOS "Build reaper_atmos plug-in" ON)
  option(BUILD_REAPER_MP3 "Build reaper_mp3 plug-in" ON)
  option(BUILD_REAPER_STT "Build reaper_stt plug-in" ON)

  if(BUILD_REAPER_CSURF)
#     add_subdirectory(reaper-plugins/reaper_csurf)
  endif()
  if(BUILD_REAPER_ATMOS)
#     add_subdirectory(reaper-plugins/reaper_atmos)
  endif()
  if(BUILD_REAPER_MP3)
#     add_subdirectory(reaper-plugins/reaper_mp3)
  endif()
  if(BUILD_REAPER_STT)
#     add_subdirectory(reaper-plugins/reaper_stt)
  endif()
endif()

if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
  FetchContent_MakeAvailable(googletest)

  or_safe_add_executable(track_playlist_test sdk/track_playlist/track_playlist_test.cpp)
  target_compile_features(track_playlist_test PRIVATE cxx_std_17)
    or_safe_add_test(track_playlist_test COMMAND track_playlist_test)
endif()
# ---- REAPER plugins status ----
if(NOT ORPHEUS_BUILD_REAPER_PLUGINS)
  message(STATUS "REAPER plug-ins: OFF (set ORPHEUS_BUILD_REAPER_PLUGINS=ON to enable)")
else()
  # Minimal sanity: need SDK & WDL bits if user turns this on
  set(_SDK_H "${CMAKE_SOURCE_DIR}/sdk/reaper_plugin.h")
  set(_WDL_SWELL "${CMAKE_SOURCE_DIR}/WDL/swell/swell.h")
  if(NOT EXISTS "${_SDK_H}" OR NOT EXISTS "${_WDL_SWELL}")
    message(FATAL_ERROR "REAPER plug-ins: ON but prerequisites missing:
- ${_SDK_H}
- ${_WDL_SWELL}
Provide REAPER SDK + full WDL (or vendor as needed).")
  endif()
  message(STATUS "REAPER plug-ins: ON")
endif()
# ---- end status ----
# ---- REAPER plug-ins (guarded) ----------------------------------------------
option(ORPHEUS_BUILD_REAPER_PLUGINS "Build REAPER plug-ins" OFF)

if(ORPHEUS_BUILD_REAPER_PLUGINS)
  foreach(subdir IN ITEMS reaper_csurf reaper_atmos reaper_mp3 reaper_stt)
    if(NOT TARGET ${subdir})
      add_subdirectory(reaper-plugins/${subdir})
    endif()
  endforeach()
endif()
# ---- FINAL FIXUPS (must come after all the duplicated blocks) ----
if(TARGET track_playlist_test)
  # ensure keyword-signature and correct libs no matter what earlier blocks did
  target_link_libraries(track_playlist_test PRIVATE gmock_main track_playlist Threads::Threads)
  target_compile_features(track_playlist_test PRIVATE cxx_std_17)
endif()
# ---- THREADS + FINAL FIXUPS ----
find_package(Threads REQUIRED)
if(TARGET track_playlist_test)
  target_link_libraries(track_playlist_test PRIVATE gmock_main track_playlist Threads::Threads)
  target_compile_features(track_playlist_test PRIVATE cxx_std_17)
endif()
