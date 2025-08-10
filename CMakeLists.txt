cmake_minimum_required(VERSION 3.20)

cmake_minimum_required(VERSION 3.15)
project(reaper_sdk_examples)
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

add_library(track_playlist STATIC sdk/track_playlist/track_playlist.cpp)
target_compile_features(track_playlist PUBLIC cxx_std_17)
target_include_directories(track_playlist PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk)

add_executable(track_playlist_demo sdk/track_playlist/track_playlist_demo.cpp)
target_link_libraries(track_playlist_demo track_playlist)

if(WDL_FOUND)
  # Sample plug-ins
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

  option(BUILD_REAPER_CSURF "Build reaper_csurf plug-in" ON)
  option(BUILD_REAPER_ATMOS "Build reaper_atmos plug-in" ON)
  option(BUILD_REAPER_MP3 "Build reaper_mp3 plug-in" ON)
  option(BUILD_REAPER_STT "Build reaper_stt plug-in" ON)

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
endif()

if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
  FetchContent_MakeAvailable(googletest)

  add_executable(track_playlist_test sdk/track_playlist/track_playlist_test.cpp)
  target_compile_features(track_playlist_test PRIVATE cxx_std_17)
  target_link_libraries(track_playlist_test track_playlist GTest::gtest_main)
  add_test(NAME track_playlist_test COMMAND track_playlist_test)
endif()
