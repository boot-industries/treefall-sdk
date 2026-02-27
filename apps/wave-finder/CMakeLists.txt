# SPDX-License-Identifier: MIT
#
# Wave Finder — Proof-of-concept app validating occ-app-platform extraction.
# A minimal JUCE GUI app that links the shared platform package + SDK targets.
cmake_minimum_required(VERSION 3.22)

project(orpheus_wave_finder LANGUAGES C CXX)

# Fetch JUCE if not already available
include(FetchContent)

if(NOT TARGET juce::juce_gui_basics)
  FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(juce)
endif()

# Fetch SQLite3 (guarded for multi-app builds)
if(NOT TARGET sqlite3)
  FetchContent_Declare(
    sqlite3
    URL https://www.sqlite.org/2023/sqlite-amalgamation-3440200.zip
  )
  FetchContent_GetProperties(sqlite3)
  if(NOT sqlite3_POPULATED)
    FetchContent_Populate(sqlite3)
    add_library(sqlite3 STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
    target_include_directories(sqlite3 PUBLIC ${sqlite3_SOURCE_DIR})
    target_compile_definitions(sqlite3 PUBLIC
      SQLITE_THREADSAFE=1
      SQLITE_ENABLE_FTS5
      SQLITE_ENABLE_JSON1
      SQLITE_OMIT_LOAD_EXTENSION
    )
  endif()
endif()

# Shared packages (guarded for multi-app builds)
set(OCC_APP_PLATFORM_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../packages/occ-app-platform")
if(NOT TARGET occ_app_platform)
  add_subdirectory(${OCC_APP_PLATFORM_PATH} ${CMAKE_CURRENT_BINARY_DIR}/occ-app-platform)
endif()

set(SHMUI_JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../packages/shmui-juce")
if(NOT TARGET orpheus_shmui_juce)
  add_subdirectory(${SHMUI_JUCE_PATH} ${CMAKE_CURRENT_BINARY_DIR}/shmui-juce)
endif()

# Create JUCE GUI application
juce_add_gui_app(orpheus_wave_finder_app
  PRODUCT_NAME "Orpheus Wave Finder"
  VERSION 0.1.0
  COMPANY_NAME "Orpheus"
  BUNDLE_ID "com.orpheus.wavefinder"
  NEEDS_CURL FALSE
  NEEDS_WEB_BROWSER FALSE
)

target_sources(orpheus_wave_finder_app PRIVATE
  Source/Main.cpp
  Source/MainComponent.cpp
)

target_compile_features(orpheus_wave_finder_app PRIVATE cxx_std_20)

# Include directories
set(SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
target_include_directories(orpheus_wave_finder_app
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${SDK_ROOT}/include
)

target_compile_definitions(orpheus_wave_finder_app
  PRIVATE
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_DISABLE_JUCE_VERSION_PRINTING=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
)

# Link shared packages + JUCE modules
target_link_libraries(orpheus_wave_finder_app
  PRIVATE
    juce::juce_gui_extra
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_utils
    juce::juce_events
    sqlite3
    orpheus_shmui_juce
    occ_app_platform
)

# Link Orpheus SDK modules
target_link_libraries(orpheus_wave_finder_app
  PRIVATE
    orpheus_transport
    orpheus_audio_io
    orpheus_routing
    $<$<PLATFORM_ID:Darwin>:orpheus_audio_driver_coreaudio>
)

# Link libsndfile
find_library(LIBSNDFILE_LIBRARY sndfile)
if(LIBSNDFILE_LIBRARY)
  target_link_libraries(orpheus_wave_finder_app PRIVATE ${LIBSNDFILE_LIBRARY})
else()
  message(FATAL_ERROR "libsndfile not found. Install with: brew install libsndfile")
endif()

# macOS frameworks
if(APPLE)
  target_link_libraries(orpheus_wave_finder_app PRIVATE
    "-framework CoreAudio"
    "-framework AudioToolbox"
    "-framework AudioUnit"
  )
endif()

set_target_properties(orpheus_wave_finder_app PROPERTIES
  OUTPUT_NAME "OrpheusWaveFinder"
)
