# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.22)

project(orpheus_clip_composer LANGUAGES C CXX)

set(SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(ORPHEUS_SDK_BUILD_DIR "" CACHE PATH
    "Path to a configured Orpheus SDK build tree for standalone Clip Composer builds")

function(resolve_orpheus_sdk)
  if(TARGET Orpheus::core)
    return()
  endif()

  if(ORPHEUS_SDK_BUILD_DIR)
    find_package(OrpheusSDK CONFIG REQUIRED
      PATHS "${ORPHEUS_SDK_BUILD_DIR}"
      NO_DEFAULT_PATH)
    return()
  endif()

  find_package(OrpheusSDK CONFIG QUIET
    PATHS
      "${SDK_ROOT}/build"
      "${SDK_ROOT}/build-sdk-debug"
      "${SDK_ROOT}/build-sdk-release"
    NO_DEFAULT_PATH)

  if(TARGET Orpheus::core)
    return()
  endif()

  set(ORPHEUS_ENABLE_APP_CLIP_COMPOSER OFF CACHE BOOL "" FORCE)
  set(ORPHEUS_ENABLE_APP_WAVE_FINDER OFF CACHE BOOL "" FORCE)
  set(ORPHEUS_ENABLE_APP_JUCE_HOST OFF CACHE BOOL "" FORCE)
  set(ORPHEUS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(ORP_WITH_TESTS OFF CACHE BOOL "" FORCE)
  add_subdirectory("${SDK_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/orpheus-sdk" EXCLUDE_FROM_ALL)

  if(NOT TARGET Orpheus::core)
    message(FATAL_ERROR "Failed to resolve Orpheus SDK targets for Clip Composer.")
  endif()
endfunction()

resolve_orpheus_sdk()

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

# Fetch SQLite3
FetchContent_Declare(
  sqlite3
  URL https://www.sqlite.org/2023/sqlite-amalgamation-3440200.zip
)
FetchContent_GetProperties(sqlite3)
if(NOT sqlite3_POPULATED)
  FetchContent_Populate(sqlite3)
  add_library(sqlite3 STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
  target_include_directories(sqlite3 PUBLIC ${sqlite3_SOURCE_DIR})
  # Optimize for size/performance
  target_compile_definitions(sqlite3 PUBLIC
    SQLITE_THREADSAFE=1
    SQLITE_ENABLE_FTS5
    SQLITE_ENABLE_JSON1
    SQLITE_OMIT_LOAD_EXTENSION
  )
endif()


# Get Git commit hash
execute_process(COMMAND git rev-parse --short HEAD
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE APP_COMMIT_HASH
                OUTPUT_STRIP_TRAILING_WHITESPACE)

# Get build date and time
execute_process(COMMAND git log -1 --format=%cd --date=format:'%y%m%d-%H%M'
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE APP_BUILD_DATE
                OUTPUT_STRIP_TRAILING_WHITESPACE)

# Configure BuildInfo.h
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/Source/BuildInfo.h.in
  ${CMAKE_CURRENT_SOURCE_DIR}/Source/BuildInfo.h
  @ONLY
)

# Add shared packages as subdirectories
# CMAKE_SOURCE_DIR points to different locations depending on build context:
# - SDK root when building from /build
# - apps/clip-composer when building standalone
# Use CMAKE_CURRENT_SOURCE_DIR (always apps/clip-composer) for reliable path resolution
set(SHMUI_JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../packages/shmui-juce")
if(NOT TARGET orpheus_shmui_juce)
  add_subdirectory(${SHMUI_JUCE_PATH} ${CMAKE_CURRENT_BINARY_DIR}/shmui-juce)
endif()

set(OCC_APP_PLATFORM_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../packages/occ-app-platform")
if(NOT TARGET occ_app_platform)
  add_subdirectory(${OCC_APP_PLATFORM_PATH} ${CMAKE_CURRENT_BINARY_DIR}/occ-app-platform)
endif()

# Create JUCE GUI application
juce_add_gui_app(orpheus_clip_composer_app
  PRODUCT_NAME "Orpheus Clip Composer"
  VERSION 0.2.0
  COMPANY_NAME "Orpheus"
  BUNDLE_ID "com.orpheus.clipcomposer"
  NEEDS_CURL FALSE
  NEEDS_WEB_BROWSER FALSE
)

# Source files
set(CLIP_COMPOSER_SOURCES
  Source/Main.cpp
  Source/MainComponent.cpp
  Source/ClipGrid/ClipButton.cpp
  Source/ClipGrid/ClipGrid.cpp
  Source/Session/SessionManager.cpp
  Source/Transport/TransportControls.cpp
  Source/UI/TabSwitcher.cpp
  Source/UI/AudioSettingsDialog.cpp
  Source/UI/ClipEditDialog.cpp
  Source/UI/ColorSwatchPicker.cpp
  Source/UI/WaveformDisplay.cpp
  Source/UI/PreviewPlayer.cpp
  Source/Audio/AudioEngine.cpp
  # Core Managers (OCC-specific, not extracted to occ-app-platform)
  Source/Core/HotKeyManager.cpp
  Source/Core/MIDIDeviceManager.cpp
  Source/Core/ClipCommands.cpp

  # UI Components
  Source/UI/SessionHistoryWindow.cpp
  Source/UI/HeartbeatIndicator.cpp

  # UI Dialogs (OCC116/OCC117)
  Source/UI/HotKeySetupDialog.cpp
  Source/UI/MIDIDevicesDialog.cpp
  Source/UI/PasteSpecialDialog.cpp
  Source/UI/MIDIMonitorWindow.cpp
  Source/UI/LevelMetersWindow.cpp
  Source/UI/AboutDialog.cpp
)

target_sources(orpheus_clip_composer_app PRIVATE ${CLIP_COMPOSER_SOURCES})

# C++20 required (matches Orpheus SDK)
target_compile_features(orpheus_clip_composer_app PRIVATE cxx_std_20)

# Include directories
target_include_directories(orpheus_clip_composer_app
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

# JUCE compile definitions
target_compile_definitions(orpheus_clip_composer_app
  PRIVATE
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_DISABLE_JUCE_VERSION_PRINTING=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
)

# Link against JUCE modules
target_link_libraries(orpheus_clip_composer_app
  PRIVATE
    juce::juce_gui_extra
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_utils
    juce::juce_events
    orpheus_shmui_juce              # shmui-juce audio visualization
    occ_app_platform                # Shared app infrastructure (ServiceContext, Database, UndoManager, etc.)
    orpheus_audio_driver_manager    # Device enumeration (createAudioDriverManager)
    Orpheus::core
)

# Output name
set_target_properties(orpheus_clip_composer_app PROPERTIES
  OUTPUT_NAME "OrpheusClipComposer"
)

# Apply compiler warnings (matches SDK build system)
# Note: JUCE 8.0.4 has some warnings we can't control, so we don't use orpheus_enable_warnings
# which enables -Werror. We'll re-enable once JUCE issues are resolved or we upgrade.
# include(${CMAKE_SOURCE_DIR}/cmake/CompilerWarnings.cmake)
# orpheus_enable_warnings(orpheus_clip_composer_app)

#==============================================================================
# Testing Infrastructure (Sprint A4)
#==============================================================================

# Enable CTest
enable_testing()

# Fetch GoogleTest if testing is enabled
if(NOT TARGET GTest::gtest)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE
  )
  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

# Add tests subdirectory
add_subdirectory(tests)
