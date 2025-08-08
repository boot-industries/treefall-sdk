cmake_minimum_required(VERSION 3.15)
project(reaper_sdk_examples)

# Verify that the WDL headers are available
set(WDL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/WDL")
if(EXISTS "${WDL_ROOT}/wdltypes.h")
  add_library(example_m3u MODULE sdk/example_m3u/import_m3u.cpp)
  target_include_directories(example_m3u PRIVATE ${WDL_ROOT} ${CMAKE_CURRENT_SOURCE_DIR}/sdk)

  add_library(example_raw MODULE
    sdk/example_raw/pcmsrc_raw.cpp
    sdk/example_raw/pcmsink_raw.cpp)
  target_include_directories(example_raw PRIVATE ${WDL_ROOT} ${CMAKE_CURRENT_SOURCE_DIR}/sdk)
else()
  message(FATAL_ERROR "WDL not found")
endif()

