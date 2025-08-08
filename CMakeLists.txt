cmake_minimum_required(VERSION 3.15)
project(reaper_sdk_examples)

# Verify that the WDL headers are available
set(WDL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/WDL/WDL")
if(EXISTS "${WDL_ROOT}/wdltypes.h")
  add_library(track_playlist STATIC sdk/track_playlist/track_playlist.cpp)
  target_compile_features(track_playlist PUBLIC cxx_std_17)
  target_include_directories(track_playlist PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sdk)

  add_executable(track_playlist_demo sdk/track_playlist/track_playlist_demo.cpp)
  target_link_libraries(track_playlist_demo track_playlist)
else()
  message(FATAL_ERROR "WDL not found")
endif()

